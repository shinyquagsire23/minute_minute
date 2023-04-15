/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef MINUTE_BOOT1

#include "filepicker.h"

#include "smc.h"
#include "gfx.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>

char *_filename;
char _directory[_MAX_LFN + 1];

picker* __picker;
picker *picker_chain[100];

int opened_pickers = 0;

void picker_init(picker* new_picker);
void picker_print_filenames();
void picker_update();
void picker_next_selection();
void picker_prev_selection();
void picker_next_jump();
void picker_prev_jump();

int pick_sprintf(char *out, const char* fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    vsprintf(out, fmt, va);
    va_end(va);

    return 0;
}

int pick_snprintf(char *out, unsigned int len, const char* fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    vsnprintf(out, len, fmt, va);
    va_end(va);

    return 0;
}

char *pick_strcpy(char *dest, char *src)
{
    int count = 0;
    while(1)
    {
        dest[count] = src[count];
        if(dest[count] == '\0') break;

        count++;
    }
    return dest;
}

char* pick_file(char* path, bool folderpick, char* filename_buf)
{
    char *filename;
    _filename = filename_buf;

    picker _picker = {{0}};
    pick_strcpy(_picker.path, path);
    _picker.folderpick = folderpick;

    FDIR dir;
    FILINFO info;
    FRESULT ret;

    static char lfn[_MAX_LFN + 1];
    info.lfname = lfn;
    info.lfsize = sizeof(lfn);

    ret = f_opendir(&dir, path);

    if (ret != FR_OK)
        return NULL;

    /*
     * TODO: More than 256 files/directories, if needed.
     * Currently any more than that sucks up quite a bit
     * of RAM and isn't strictly needed.
     *
     * At the moment though it at least caps it so it
     * doesn't crash.
     */
    while(true)
    {
        ret = f_readdir(&dir, &info);

        if (ret != FR_OK || info.fname[0] == 0) break;
        if (info.fname[0] == '.' && info.fname[1] == '\0' && !folderpick) continue;

        filename = *info.lfname ? info.lfname : info.fname;

        if ((info.fattrib & AM_DIR) && _picker.directories < 255) // directory
        {
            pick_strcpy(_picker.directory[_picker.directories], filename);
            _picker.directories++;

        }
        else if(_picker.files < 255 && !folderpick) // file
        {
            pick_strcpy(_picker.file[_picker.files], filename);
            _picker.files++;
        }
    }

    picker_init(&_picker);

    return _filename;
}

void picker_init(picker* new_picker)
{
    if(opened_pickers < 0)
    {
        opened_pickers = 0;
        return;
    }

    __picker = new_picker;
    __picker->selected = 0;
    __picker->update_needed = true;

    picker_update();

    while(true)
    {
        u8 input = smc_get_events();

        //TODO: There's no way to exit
        //break;

        if(input & SMC_EJECT_BUTTON)
        {
            if(__picker->selected > __picker->directories - 1) // file
            {
                pick_sprintf(_filename, "%s/%s", __picker->path, __picker->file[__picker->selected - (__picker->directories)]);
            }
            else // directory
            {
                // Throw the current directory on to our stack.
                picker_chain[opened_pickers++] = __picker;

                pick_sprintf(_directory, "%s/%s", __picker->path, __picker->directory[__picker->selected]);
                pick_file(_directory, __picker->folderpick, _filename);

                // Has a file been selected yet? If not, we have to show the previous directory, user probably pressed B.
                if(strlen(_filename) == 0)
                    picker_init(picker_chain[--opened_pickers]);
            }

            break;
        }

        //TODO: No way to select folders either...
        /*else if(__picker->folderpick && (input & BUTTON_Y))
        {
            if(__picker->selected <= __picker->directories) // selected folder
            {
                pick_sprintf(_filename, "%s", __picker->path);
            }

            break;
        }*/

        if(input & SMC_POWER_BUTTON) picker_next_selection();

        picker_update();
    }
}

void picker_print_filenames()
{
    int i = 0;
    char item_buffer[100] = {0};

    console_add_text(__picker->folderpick ? "Select a directory..." : "Select a file...");

    //TODO: No way to select folders
    /*if(__picker->folderpick)
        console_add_text("[] Select current directory");*/

    console_add_text("");

    for(i = __picker->show_y; i < (MAX_LINES - 6) + __picker->show_y; i++)
    {
        if(i < __picker->directories)
        {
            pick_snprintf(item_buffer, MAX_LINE_LENGTH, " %s/", __picker->directory[i]);
        }
        else
        {
            pick_snprintf(item_buffer, MAX_LINE_LENGTH, " %s", __picker->file[i - __picker->directories]);
        }
        console_add_text(item_buffer);
    }
}

void picker_update()
{
    int i = 0, x = 0, y = 0;
    console_get_xy(&x, &y);
    if(__picker->update_needed)
    {
        console_init();
        picker_print_filenames();
        console_show();
        __picker->update_needed = false;
    }

    int header_lines_skipped = __picker->folderpick? 3 : 2;

    // Update cursor.
    for(i = 0; i < (MAX_LINES - 6); i++)
        gfx_draw_string(GFX_DRC, i == __picker->selected - __picker->show_y ? ">" : " ", x + CHAR_WIDTH, (i+header_lines_skipped) * CHAR_WIDTH + y + CHAR_WIDTH * 2, GREEN);
}

void picker_next_selection()
{
    if(__picker->selected + 1 < (__picker->files + __picker->directories))
        __picker->selected++;
    else
    {
        __picker->selected = 0;
        if(__picker->show_y)
        {
            __picker->show_y = 0;
            __picker->update_needed = true;
        }
    }

    if(__picker->selected > (MAX_LINES - 7) + __picker->show_y)
    {
        __picker->show_y++;
        __picker->update_needed = true;
    }

    picker_update();
}

void picker_prev_selection()
{
    if(__picker->selected > 0)
        __picker->selected--;

    if(__picker->selected < __picker->show_y)
    {
        __picker->show_y--;
        __picker->update_needed = true;
    }

    picker_update();
}
void picker_next_jump()
{
    int jump_num = 1;
    if(__picker->selected + 5 < (__picker->files + __picker->directories))
        jump_num = 5;
    else
        jump_num = (__picker->files + __picker->directories - 1) - __picker->selected;
    __picker->selected += jump_num;

    if(__picker->selected > (MAX_LINES - 7) + __picker->show_y)
    {
        __picker->show_y += jump_num;
        __picker->update_needed = true;
    }

    picker_update();
}

void picker_prev_jump()
{
    int jump_num = 1;
    if(__picker->selected > 5)
        jump_num = 5;
    else
        jump_num = __picker->selected;
    __picker->selected -= jump_num;

    if(__picker->selected < __picker->show_y)
    {
        __picker->show_y -= jump_num;
        __picker->update_needed = true;
    }

    picker_update();
}

#endif
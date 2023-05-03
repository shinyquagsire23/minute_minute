/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "types.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "ini.h"
#include "minini.h"
#include "gpu.h"

struct {
    const char* section;
    minini_handler handler;
} minini_handlers[] = {
    {"mcp", mcp_ini},
    {"boot", boot_ini},
    {"clocks", clocks_ini},

    {NULL, NULL}
};

int minini_result = 0;

static int _minini_handler(void* user, const char* section, const char* name, const char* value)
{
    int i = 0;

    //printf("minini: %s %s %s\n", section, name, value);

    while(true)
    {
        if(!minini_handlers[i].section) break;

        if(!strcmp(minini_handlers[i].section, section))
            return minini_handlers[i].handler(name, value);

        i++;
    }

    return 0;
}

int minini_init(void)
{
    FILE* file = fopen("sdmc:/minute/minute.ini", "r");
    if(!file) {
        printf("minini: Failed to open `sdmc:/minute/minute.ini`!\n");
        return 1;
    }

    int res = ini_parse_file(file, _minini_handler, NULL);

    fclose(file);
    return res;
}

size_t minini_get_bytes(const char* value, void* out, size_t max)
{
    if(!value || !out) return 0;

    u8* output = out;

    int i = 0;
    int pos = 0;

    size_t size = strlen(value);

    while(true)
    {
        if(i >= max) return i;
        if(pos >= size) return i;

        char byte[3] = {0};
        memcpy(byte, value + pos, 2);

        output[i] = strtol(byte, NULL, 16);
        i++;

        pos += 2;
        if(value[pos] == ' ') pos++;
    }

    return i;
}

long long minini_get_int(const char* value, long long default_value)
{
    if(!value) return default_value;

    char* end = NULL;
    long long ret = strtoll(value, &end, 0);

    return end > value ? ret : default_value;
}

unsigned long long minini_get_uint(const char* value, unsigned long long default_value)
{
    if(!value) return default_value;

    char* end = NULL;
    unsigned long long ret = strtoull(value, &end, 0);

    return end > value ? ret : default_value;
}

double minini_get_real(const char* value, double default_value)
{
    if(!value) return default_value;

    char* end = NULL;
    double ret = strtod(value, &end);

    return end > value ? ret : default_value;
}

bool minini_get_bool(const char* value, bool default_value)
{
    bool ret = default_value;

    if(!value) return ret;

    // Create a copy of the value that we can modify.
    char* copy = NULL;
    asprintf(&copy, value);

    // Convert the string to lower-case.
    for(int i = 0; copy[i]; i++)
        copy[i] = tolower(copy[i]);

    static const struct {
        char* text;
        bool value;
    } table[] = {
        {"true", true},
        {"yes", true},
        {"on", true},
        {"1", true},

        {"false", false},
        {"no", false},
        {"off", false},
        {"0", false},

        {NULL, false}
    };

    int i = 0;

    while(true)
    {
        if(!table[i].text) break;

        if(!strcmp(table[i].text, copy) && strlen(table[i].text) == strlen(copy))
        {
            ret = table[i].value;
            break;
        }

        i++;
    }

    free(copy);

    return ret;
}

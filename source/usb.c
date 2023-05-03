#include "usb.h"

#include "pll.h"
#include "latte.h"
#include "utils.h"
#include <string.h>

int usb_init_2()
{
    return 0; // TODO
}

int usb_init()
{
    bsp_pll_cfg pllcfg;

    memset(&pllcfg.fastEn, 0, sizeof(bsp_pll_cfg));
    pll_usb_read(&pllcfg);

    if (memcmp(&usbpll_cfg, &pllcfg, sizeof(bsp_pll_cfg)))
    {
        write32(LT_USBFRCRST, 0xFE);
        pll_usb_write(&usbpll_cfg);
        write32(LT_USBFRCRST, 0xF6);
        udelay(50);
        write32(LT_USBFRCRST, 0xF4);
        udelay(1);
        write32(LT_USBFRCRST, 0xF0);
        udelay(1);
        write32(LT_USBFRCRST, 0x70);
        udelay(1);
        write32(LT_USBFRCRST, 0x60);
        udelay(1);
        write32(LT_USBFRCRST, 0x40);
        udelay(1);
        write32(LT_USBFRCRST, 0);
        udelay(1);

        return usb_init_2();
    }

    return 0;
}
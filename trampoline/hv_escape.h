#pragma once
#include <stdint.h>

#ifndef HV_ESCAPE
#define HV_ESCAPE(x) x
#endif

void hv_escape_tmr(uint32_t fw);

static inline bool hv_escape(uint32_t fw)
{
    if(fw < 0x50000000)
    {
        HV_ESCAPE(hv_escape_tmr(fw));
        return true;
    }
    return false;
}

#undef HV_ESCAPE

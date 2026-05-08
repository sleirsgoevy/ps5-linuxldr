#pragma once
#include "trampoline_params.h"

static uint64_t get_transmitter_control(void)
{
    //FIXME: 4.03 offset
    return trampoline_params->kdata_base - 0xa51d0;
}

static uint64_t get_mp3_initialize(void)
{
    //FIXME: 4.03 offset
    return trampoline_params->kdata_base - 0x27f960;
}

static uint64_t get_mp3_invoke(void)
{
    //FIXME: 4.03 offset
    return trampoline_params->kdata_base - 0x280b40;
}

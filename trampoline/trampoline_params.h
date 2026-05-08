#pragma once
#include <stdint.h>

struct trampoline_params
{
    uint64_t firmware_wakeup; //must come first
    uint32_t fw_version;
    uint32_t vram_size;
    uint64_t kdata_base;
};

extern struct trampoline_params* trampoline_params;

#pragma once
#include <stdint.h>

static inline void enable_uart(void)
{
    *(volatile uint32_t*)0xc0115110 &= ~0x200;
}

static inline void putchar(uint8_t c)
{
    #pragma GCC unroll(80)
    while((*(volatile uint32_t*)0xc101010c & 2048));
    *(volatile uint32_t*)0xc1010104 = c;
}

static inline void putstring(const char* s)
{
    while(*s)
        putchar(*s++);
}

static inline void puthex(uint64_t q)
{
    int k = 0;
    while(k < 60 && (q >> k) / 16)
        k += 4;
    while(k >= 0)
    {
        putchar("0123456789abcdef"[(q >> k) % 16]);
        k -= 4;
    }
}

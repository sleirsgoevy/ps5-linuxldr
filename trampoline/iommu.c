#include <stddef.h>
#include "iommu.h"
#include "vmmcall.h"
#include "uart.h"

extern uint64_t our_cr3;

/*
    We're given 16 pages at our_cr3.  How they are used:

    cr3+0x0000             PML4 of idmap pagetable
    cr3+0x1000             PML3 of idmap pagetable
    cr3+0x2000, cr3+0x3000 IOMMU command buffers (both in the same range)
    cr3+0x4000             IOMMU event log
    cr3+0x5000             IOMMU PML4
    cr3+0x6000             IOMMU PML3
*/
void hv_configure_iommu(void)
{
    putstring("Configuring IOMMU with identity mappings... ");
    vmmcall_iommu_set_guest_buffers(our_cr3 + 0x2000, our_cr3 + 0x2000, our_cr3 + 0x4000);
    uint64_t* pml4 = (uint64_t*)(our_cr3 + 0x5000);
    pml4[0] = (our_cr3 + 0x6000) | 7 | (15ull << 52);
    uint64_t* pml3 = (uint64_t*)(our_cr3 + 0x6000);
    for(size_t i = 0; i < 512; i++)
        pml3[i] = (i << 30) | 0x87 | (15ull << 52);
    uint64_t gcr3 = our_cr3 + 0x5000;
    static constexpr uint16_t devices[] = {0x2000, 0x2004, 0x2005, 0x4001, 0x4005, 0x4006};
    for(size_t i = 0; i < sizeof(devices) / sizeof(*devices); i++)
    {
        vmmcall_iommu_enable_device(devices[i]);
        vmmcall_iommu_bind_pasid(devices[i], 0, gcr3);
        while(vmmcall_iommu_check_cmd_completion());
    }
    putstring("done\r\n");
}

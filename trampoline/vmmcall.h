#pragma once
#include <stdint.h>

enum
{
    VMMCALL_IOMMU_SET_GUEST_BUFFERS = 6,
    VMMCALL_IOMMU_ENABLE_DEVICE = 7,
    VMMCALL_IOMMU_BIND_PASID = 8,
    VMMCALL_IOMMU_UNBIND_PASID = 9,
    VMMCALL_IOMMU_CHECK_CMD_COMPLETION = 10,
};

static int vmmcall_iommu_set_guest_buffers(uint64_t cb2, uint64_t cb3, uint64_t evlog)
{
    uint64_t out_rcx, out_rdx;
    int ans;
    asm volatile("vmmcall":"=a"(ans),"=c"(out_rcx),"=d"(out_rdx):"a"(VMMCALL_IOMMU_SET_GUEST_BUFFERS),"b"(cb2),"c"(cb3),"d"(evlog));
    return ans;
}

static int vmmcall_iommu_enable_device(uint16_t device)
{
    int ans;
    asm volatile("vmmcall":"=a"(ans):"a"(VMMCALL_IOMMU_ENABLE_DEVICE),"b"((uint64_t)device));
    return ans;
}

static int vmmcall_iommu_bind_pasid(uint16_t device, int pasid, uint64_t gcr3)
{
    int ans;
    asm volatile("vmmcall":"=a"(ans):"a"(VMMCALL_IOMMU_BIND_PASID),"b"((uint64_t)device),"c"((uint64_t)pasid),"d"(gcr3));
    return ans;
}

static int vmmcall_iommu_unbind_pasid(uint16_t device, int pasid)
{
    int ans;
    asm volatile("vmmcall":"=a"(ans):"a"(VMMCALL_IOMMU_UNBIND_PASID),"b"((uint64_t)device),"c"((uint64_t)pasid));
    return ans;
}

static int vmmcall_iommu_check_cmd_completion(void)
{
    int ans;
    asm volatile("vmmcall":"=a"(ans):"a"(VMMCALL_IOMMU_CHECK_CMD_COMPLETION));
    return ans;
}

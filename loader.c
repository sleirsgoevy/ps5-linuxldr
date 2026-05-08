#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <ps5/payload.h>
#include "loader.h"
#include "utils.h"

static const char* load_paths[] = {"/mnt/usb0", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3", "/mnt/usb0/PS5/Linux", "/mnt/usb1/PS5/Linux", "/mnt/usb2/PS5/Linux", "/mnt/usb3/PS5/Linux"};

static int open_file(const char* filename)
{
    for(size_t i = 0; i < sizeof(load_paths) / sizeof(*load_paths); i++)
    {
        char* path;
        asprintf(&path, "%s/%s", load_paths[i], filename);
        int fd = open(path, O_RDONLY);
        free(path);
        if(fd >= 0)
        {
            notify("%s found in %s", filename, load_paths[i]);
            return fd;
        }
    }
    return -1;
}

static size_t read_from_fd(int fd, void** out)
{
    char* buf = malloc(16384);
    size_t sz = 0;
    size_t cap = 16384;
    for(;;)
    {
        if(sz == cap)
        {
            cap *= 2;
            buf = realloc(buf, cap);
        }
        ssize_t chk = read(fd, buf+sz, cap-sz);
        if(chk <= 0)
            break;
        sz += chk;
    }
    close(fd);
    *out = buf;
    return sz;
}

static ssize_t receive_file(const char* filename, void** out)
{
    static int lsock = -1;
    if(lsock < 0)
    {
        lsock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in sin = {
            .sin_family = AF_INET,
            .sin_port = __builtin_bswap16(9999),
        };
        if(bind(lsock, (void*)&sin, sizeof(sin)) < 0)
        {
            notify("bind: %s", strerror(errno));
            close(lsock);
            lsock = -1;
            return -1;
        }
        listen(lsock, 1);
    }
    notify("%s not found anywhere, accepting on port 9999", filename);
    size_t size = read_from_fd(accept(lsock, 0, 0), out);
    char* path;
    asprintf(&path, "/user/netcat-cache-%s", filename);
    if(size)
    {
        unlink(path);
        int fd = open(path, O_WRONLY|O_CREAT|O_EXCL, 0666);
        size_t pos = 0;
        while(pos < size)
            pos += write(fd, (char*)*out + pos, size - pos);
        close(fd);
        return size;
    }
    else
    {
        free(*out);
        int fd = open(path, O_RDONLY);
        if(fd < 0)
            return -1;
        return read_from_fd(fd, out);
    }
}

ssize_t read_file(const char* filename, void** out, bool allow_netcat)
{
    int fd = open_file(filename);
    if(fd >= 0)
        return read_from_fd(fd, out);
    if(allow_netcat)
    {
        ssize_t sz = receive_file(filename, out);
        if(sz >= 0)
            return sz;
    }
    notify("%s not found", filename);
    return -1;
}

int sceKernelAllocateDirectMemory(int64_t min_addr, int64_t max_addr, uint64_t len, uint64_t alignment, int memorytype, int64_t* pa);
int sceKernelMapDirectMemory(void** addr, uint64_t len, int prot, int flags, uint64_t phys_addr, uint64_t alignment);

static uint64_t allocate_large_physical_memory(void** ptr, size_t sz, uint64_t alignment)
{
    if(sz == 0)
        return 0; //already physically contigous, can give any valid address
    if(alignment < (1 << 21))
        alignment = (1 << 21);
    sz = (sz + alignment - 1) & -alignment;
    int64_t pa;
    int err = sceKernelAllocateDirectMemory(0, 0x400000000, sz, alignment, 0, &pa);
    if(err)
    {
        notify("sceKernelAllocateDirectMemory failed: 0x%x", err);
        payload_exit(1);
    }
    *ptr = 0;
    err = sceKernelMapDirectMemory(ptr, sz, PROT_READ|PROT_WRITE, 0, pa, alignment);
    if(err)
    {
        notify("sceKernelMapDirectMemory failed: 0x%x", err);
        payload_exit(1);
    }
    memset(*ptr, 0, sz);
    mlock(*ptr, sz);
    //sony's "physical" addresses are fake, need to get our own from pagetables
    uint64_t pa_low = -1, pa_high = 0, pagesize;
    for(uint64_t va = (uint64_t)*ptr; va < (uint64_t)*ptr + sz; va += pagesize)
    {
        uint64_t pa = virt2phys(va, &pagesize);
        pa_low &= pa - va + (uint64_t)*ptr;
        pa_high |= pa - va + (uint64_t)*ptr;
    }
    if(pa_low != pa_high || pa_low == (uint64_t)-1)
    {
        notify("memory is not physically contigous");
        payload_exit(1);
    }
    leak_fd(*(int*)((uintptr_t)sceKernelAllocateDirectMemory + 0x68014 - 0x18990));
    return pa_low;
}

static uint64_t allocate_small_physical_memory(void** ptr, size_t sz, uint64_t alignment)
{
    static uint64_t cur_pa;
    static uint64_t end_pa;
    static char* cur_va;
    static constexpr size_t CHUNK_SIZE = 1 << 21;
    uint64_t next_pa = ((cur_pa + alignment - 1) & -alignment) + sz;
    if(next_pa > end_pa)
    {
        void* p;
        cur_pa = allocate_large_physical_memory(&p, CHUNK_SIZE, CHUNK_SIZE);
        end_pa = cur_pa + CHUNK_SIZE;
        cur_va = p;
    }
    uint64_t start = (cur_pa + alignment - 1) & -alignment;
    uint64_t end = start + sz;
    char* ans = cur_va + (start - cur_pa);
    cur_pa = end;
    cur_va = ans + sz;
    *ptr = ans;
    return start;
}

uint64_t allocate_physical_memory(void** ptr, size_t sz, uint64_t alignment)
{
    static constexpr size_t THRESHOLD = 65536;
    if(sz <= THRESHOLD && alignment <= THRESHOLD)
        return allocate_small_physical_memory(ptr, sz, alignment);
    else
        return allocate_large_physical_memory(ptr, sz, alignment);
}

uint64_t to_physical_memory_partial(void** buf, size_t sz, size_t copy_sz, uint64_t alignment, uint64_t misalignment, bool move)
{
    if(sz < copy_sz)
        sz = copy_sz;
    misalignment &= alignment - 1;
    void* outbuf;
    uint64_t pa = allocate_physical_memory(&outbuf, sz + misalignment, alignment);
    memcpy((char*)outbuf + misalignment, *buf, copy_sz);
    if(move)
        free(*buf);
    *buf = (char*)outbuf + misalignment;
    return pa + misalignment;
}

uint64_t to_physical_memory(void** buf, size_t sz, uint64_t alignment, uint64_t misalignment, bool move)
{
    return to_physical_memory_partial(buf, sz, sz, alignment, misalignment, move);
}

ssize_t read_file_to_phys(const char* filename, void** out, bool allow_netcat, uint64_t* out_phys, uint64_t alignment, uint64_t misalignment)
{
    ssize_t chk = read_file(filename, out, allow_netcat);
    if(chk < 0)
        return -1;
    *out_phys = to_physical_memory(out, chk, alignment, misalignment, true);
    return chk;
}

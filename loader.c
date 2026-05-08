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
#include <ps5/kernel.h>
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

//the memory at 0x100000000-0x400000000 is designated as "application memory"
//unless a game is currently running, this memory is basically guaranteed to be free

static constexpr uint64_t APP_MEMORY_START = 0x100000000;
static constexpr uint64_t APP_MEMORY_END = 0x400000000;

static void map_physical_memory(void)
{
    static bool mapped = false;
    if(mapped)
        return;
    uint64_t pml3[512] = {};
    for(size_t i = 0; i < 512; i++)
        pml3[i] = (i << 30) | 0x87;
    kernel_copyin(pml3, get_dmap_base() + APP_MEMORY_START, 4096);
    kwrite64(get_dmap_base() + get_cr3() + 8, APP_MEMORY_START | 7);
    mapped = true;
}

uint64_t allocate_physical_memory(void** ptr, size_t sz, uint64_t alignment)
{
    map_physical_memory();
    static uint64_t cur_phys = APP_MEMORY_START + 4096;
    uint64_t ans = -(-cur_phys & -alignment);
    if(ans + sz > APP_MEMORY_END)
    {
        notify("Out of physical memory");
        payload_exit(1);
    }
    cur_phys = ans + sz;
    *ptr = (void*)((1ull << 39) + ans);
    return ans;
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

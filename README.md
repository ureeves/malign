# malign
Linux kernel module providing aligned memory mappings

## Usage
When inserted, the module exposes the `/dev/malign` character device. The
device can be interfaces with using the `ioctl()` and `mmap()` system calls,
which when combined produce anonymous virtual address mappings aligned to a
configurable power-of-two boundary.

```C
/* Aligned to 4GiB boundary */
const uint8_t align = 32; 

int fd = open("/dev/malign", O_RDWR);

/* Alignment for subsequent mmap() calls */
malign_args args = {.align = align};
ioctl(fd, MALIGN_IOCTL, &args);

uintptr_t addr =
    (uintptr_t)mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

/* Alignment guaranteed, otherwise mmap() call fails */
assert(addr % (1UL << align) == 0);
```

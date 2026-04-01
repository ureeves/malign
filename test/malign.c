#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include "../src/include/malign.h"

#define MMAP_MAX_ALIGN 45
#define MMAP_MAX_SIZE 0x100000000
#define MMAP_FLAGS (MAP_PRIVATE | MAP_NORESERVE)

int main(int argc, char *argv[]) {
  uint8_t min_align_pow = __builtin_ctz(sysconf(_SC_PAGESIZE));

  FILE *urandom = fopen("/dev/urandom", "r");

  if (!urandom) {
    fprintf(stderr, "failed opening /dev/urandom: %s\n", strerror(errno));
    return 1;
  }

  FILE *malign = fopen("/dev/malign", "rw");

  if (!malign) {
    fclose(urandom);
    fprintf(stderr, "failed opening /dev/malign: %s\n", strerror(errno));
    return 1;
  }

  unsigned seed;
  if (fread(&seed, sizeof(seed), 1, urandom) != 1) {
    fclose(urandom);
    fclose(malign);
    fprintf(stderr, "failed reading /dev/malign: %s\n", strerror(errno));
    return 1;
  }
  fclose(urandom);

  srandom(seed);

  for (int i = 0; i < 1000000; i++) {
    uint8_t align =
        min_align_pow + random() % (MMAP_MAX_ALIGN + 1 - min_align_pow);
    unsigned long align_uint = 1UL << align;
    malign_args args = {.align = align};

    if (ioctl(fileno(malign), MALIGN_IOCTL, &args)) {
      fclose(malign);
      fprintf(stderr, "ioctl() call failed: %s\n", strerror(errno));
      return 1;
    }

    size_t len = ((size_t)random() % MMAP_MAX_SIZE) + 1;

    void *ptr_1st =
        mmap(NULL, len, PROT_READ | PROT_WRITE, MMAP_FLAGS, fileno(malign), 0);

    if (ptr_1st == MAP_FAILED) {
      fclose(malign);
      fprintf(stderr, "1st mmap() call failed: %s\n", strerror(errno));
      return 1;
    }

    void *ptr_2nd =
        mmap(NULL, len, PROT_READ | PROT_WRITE, MMAP_FLAGS, fileno(malign), 0);

    if (ptr_2nd == MAP_FAILED) {
      munmap(ptr_1st, len);
      fclose(malign);
      fprintf(stderr, "2nd mmap() call failed: %s\n", strerror(errno));
      return 1;
    }

    munmap(ptr_2nd, len);
    munmap(ptr_1st, len);

    if ((uintptr_t)ptr_1st % align_uint) {
      fclose(malign);
      fprintf(stderr, "1st mmap(): [%16p ∤ 1 << %2u; %10lu]\n", ptr_1st, align,
              len);
      return 1;
    }

    if ((uintptr_t)ptr_2nd % align_uint) {
      fclose(malign);
      fprintf(stderr, "2nd mmap(): [%16p ∤ 1 << %2u; %10lu]\n", ptr_2nd, align,
              len);
      return 1;
    }
  }

  fclose(malign);
  return 0;
}

#include "mm.h"

#include <linux/mm.h>
#include <linux/mman.h>

#define MALIGN_MIN_ALIGN (__u8) PAGE_SHIFT
#define MALIGN_MAX_ALIGN (__u8)(fls64(TASK_SIZE_MAX) - 1)

#define MALIGN_FDATA_PTR(data) (void *)((uintptr_t)data.align)
#define MALIGN_PTR_FDATA(ptr) ((malign_fdata){.align = (u8)((uintptr_t)ptr)})

typedef struct {
  u8 align;
} malign_fdata;

static unsigned long malign_stack_guard_gap = 256UL << PAGE_SHIFT;
static unsigned long malign_mmap_min_addr =
    CONFIG_DEFAULT_MMAP_MIN_ADDR > CONFIG_LSM_MMAP_MIN_ADDR
        ? CONFIG_DEFAULT_MMAP_MIN_ADDR
        : CONFIG_LSM_MMAP_MIN_ADDR;

static inline unsigned long
malign_stack_guard_start_gap(const struct vm_area_struct *vma) {
  if (vma->vm_flags & VM_GROWSDOWN)
    return malign_stack_guard_gap;

  /* See reasoning around the VM_SHADOW_STACK definition */
  if (vma->vm_flags & VM_SHADOW_STACK)
    return PAGE_SIZE;

  return 0;
}

static inline unsigned long
malign_vm_start_gap(const struct vm_area_struct *vma) {
  unsigned long gap = malign_stack_guard_start_gap(vma);
  unsigned long vm_start = vma->vm_start;

  vm_start -= gap;
  if (vm_start > vma->vm_start)
    vm_start = 0;
  return vm_start;
}

static inline unsigned long
malign_unmapped_area(struct vm_unmapped_area_info *info) {
  unsigned long length, gap;
  unsigned long low_limit, high_limit;
  struct vm_area_struct *tmp;
  VMA_ITERATOR(vmi, current->mm, 0);

  /* Adjust search length to account for worst case alignment overhead */
  length = info->length + info->align_mask + info->start_gap;
  if (length < info->length)
    return -ENOMEM;

  low_limit = info->low_limit;
  if (low_limit < malign_mmap_min_addr)
    low_limit = malign_mmap_min_addr;
  high_limit = info->high_limit;
retry:
  if (mas_empty_area(&vmi.mas, low_limit, high_limit - 1, length))
    return -ENOMEM;

  gap = vmi.mas.index + info->start_gap;
  gap += (info->align_offset - gap) & info->align_mask;
  tmp = vma_next(&vmi);
  if (tmp &&
      (tmp->vm_flags & VM_STARTGAP_FLAGS)) { /* Avoid prev check if possible */
    if (malign_vm_start_gap(tmp) < gap + length - 1) {
      low_limit = tmp->vm_end;
      mas_reset(&vmi.mas);
      goto retry;
    }
    mas_reset(&vmi.mas);
  } else {
    tmp = vma_prev(&vmi);
    if (tmp && vm_end_gap(tmp) > gap) {
      low_limit = vm_end_gap(tmp);
      mas_reset(&vmi.mas);
      goto retry;
    }
  }

  return gap;
}

static inline unsigned long
malign_unmapped_area_topdown(struct vm_unmapped_area_info *info) {
  unsigned long length, gap, gap_end;
  unsigned long low_limit, high_limit;
  struct vm_area_struct *tmp;
  VMA_ITERATOR(vmi, current->mm, 0);

  length = info->length + info->align_mask + info->start_gap;
  if (length < info->length)
    return -ENOMEM;

  low_limit = info->low_limit;
  if (low_limit < malign_mmap_min_addr)
    low_limit = malign_mmap_min_addr;
  high_limit = info->high_limit;
retry:
  if (mas_empty_area_rev(&vmi.mas, low_limit, high_limit - 1, length))
    return -ENOMEM;

  gap = vmi.mas.last + 1 - info->length;
  gap -= (gap - info->align_offset) & info->align_mask;
  gap_end = vmi.mas.last + 1;
  tmp = vma_next(&vmi);
  if (tmp && (tmp->vm_flags & VM_STARTGAP_FLAGS)) {
    if (malign_vm_start_gap(tmp) < gap_end) {
      high_limit = malign_vm_start_gap(tmp);
      mas_reset(&vmi.mas);
      goto retry;
    }
  } else {
    tmp = vma_prev(&vmi);
    if (tmp && vm_end_gap(tmp) > gap) {
      high_limit = tmp->vm_start;
      mas_reset(&vmi.mas);
      goto retry;
    }
  }

  return gap;
}

static inline struct vm_area_struct *
malign_find_vma_prev(struct mm_struct *mm, unsigned long addr,
                     struct vm_area_struct **pprev) {
  struct vm_area_struct *vma;
  VMA_ITERATOR(vmi, mm, addr);

  vma = mas_walk(&vmi.mas);
  *pprev = vma_prev(&vmi);
  if (!vma)
    vma = vma_next(&vmi);
  return vma;
}

static inline unsigned long stack_guard_placement(vm_flags_t vm_flags) {
  if (vm_flags & VM_SHADOW_STACK)
    return PAGE_SIZE;

  return 0;
}

static inline unsigned long
__malign_get_unmapped_area(unsigned long addr, unsigned long align_mask,
                           unsigned long len, unsigned long pgoff,
                           unsigned long flags, unsigned long vm_flags) {
  struct mm_struct *mm = current->mm;
  struct vm_area_struct *vma, *prev;
  struct vm_unmapped_area_info info = {};
  const unsigned long mmap_end = arch_get_mmap_end(addr, len, flags);

  if (len > mmap_end - malign_mmap_min_addr)
    return -ENOMEM;

  if (flags & MAP_FIXED)
    return addr;

  if (addr) {
    addr = PAGE_ALIGN(addr);
    vma = malign_find_vma_prev(mm, addr, &prev);
    if (mmap_end - len >= addr && addr >= malign_mmap_min_addr &&
        (!vma || addr + len <= malign_vm_start_gap(vma)) &&
        (!prev || addr >= vm_end_gap(prev)))
      return addr;
  }

  info.length = len;
  info.align_mask = align_mask;
  info.start_gap = stack_guard_placement(vm_flags);

  if (mm_flags_test(MMF_TOPDOWN, current->mm)) {
    info.low_limit = PAGE_SIZE;
    info.high_limit = arch_get_mmap_base(addr, mm->mmap_base);
    return malign_unmapped_area_topdown(&info);
  } else {
    info.low_limit = mm->mmap_base;
    info.high_limit = mmap_end;
    return malign_unmapped_area(&info);
  }
}

int malign_set_align(struct file *filp, u8 align) {
  if (align > MALIGN_MAX_ALIGN || align < MALIGN_MIN_ALIGN)
    return -EINVAL;

  malign_fdata data = {.align = align};
  filp->private_data = MALIGN_FDATA_PTR(data);

  return 0;
}

unsigned long malign_get_unmapped_area(struct file *filp, unsigned long addr,
                                       unsigned long len, unsigned long pgoff,
                                       unsigned long flags) {
  struct mm_struct *mm = current->mm;
  malign_fdata data = MALIGN_PTR_FDATA(filp->private_data);
  unsigned long align_mask = (1UL << data.align) - 1;
  unsigned long vm_flags =
      mm->def_flags | VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC;
  return __malign_get_unmapped_area(addr, align_mask, len, pgoff, flags,
                                    vm_flags);
}

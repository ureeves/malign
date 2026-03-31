#include <linux/cdev.h>
#include <linux/file.h>

#include "./include/malign.h"
#include "mm.h"

static int malign_fops_open(struct inode *inode, struct file *filp) {
  return malign_set_align(filp, PAGE_SHIFT);
}

static int malign_fops_release(struct inode *inode, struct file *file) {
  return 0;
}

static long malign_fops_ioctl_align(struct file *file, unsigned long arg) {
  malign_args args;

  if (copy_from_user(&args, (malign_args __user *)arg, sizeof(args)))
    return -EFAULT;

  return malign_set_align(file, args.align);
}

static long malign_fops_ioctl(struct file *file, unsigned int cmd,
                              unsigned long arg) {
  long ret;

  switch (cmd) {
  case MALIGN_IOCTL:
    ret = malign_fops_ioctl_align(file, arg);
    break;
  default:
    ret = -EINVAL;
  }

  return ret;
}

static int malign_fops_mmap(struct file *file, struct vm_area_struct *vma) {
  vma->vm_ops = NULL;

  fput(vma->vm_file);
  vma->vm_file = NULL;

  return 0;
}

static unsigned long malign_fops_get_unmapped_area(struct file *filp,
                                                   unsigned long addr,
                                                   unsigned long len,
                                                   unsigned long pgoff,
                                                   unsigned long flags) {
  return malign_get_unmapped_area(filp, addr, len, pgoff, flags);
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = malign_fops_open,
    .release = malign_fops_release,
    .unlocked_ioctl = malign_fops_ioctl,
    .mmap = malign_fops_mmap,
    .get_unmapped_area = malign_fops_get_unmapped_area,
};

static int malign_dev_uevent(const struct device *dev,
                             struct kobj_uevent_env *env) {
  add_uevent_var(env, "DEVMODE=%#o", 0666);
  return 0;
}

static int major;
static const struct class class = {
    .name = MALIGN_CDEV_NAME,
    .dev_uevent = malign_dev_uevent,
};

static int __init malign_init(void) {
  major = register_chrdev(0, MALIGN_CDEV_NAME, &fops);
  if (major < 0)
    return major;

  device_create(&class, NULL, MKDEV(major, 0), NULL, MALIGN_CDEV_NAME);

  return 0;
}

static void __exit malign_exit(void) {
  device_destroy(&class, MKDEV(major, 0));
  unregister_chrdev(major, MALIGN_CDEV_NAME);
}

module_init(malign_init);
module_exit(malign_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ureeves");
MODULE_DESCRIPTION("Aligned anonymous mappings");

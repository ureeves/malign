#ifndef MALIGN_MM_H_
#define MALIGN_MM_H_

#include <linux/fs.h>

int malign_set_align(struct file *filp, u8 align);

unsigned long malign_get_unmapped_area(struct file *filp, unsigned long addr,
                                       unsigned long len, unsigned long pgoff,
                                       unsigned long flags);

#endif /* MALIGN_MM_H_ */

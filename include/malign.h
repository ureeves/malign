#ifndef MALIGN_H_
#define MALIGN_H_

#include <linux/ioctl.h>
#include <linux/types.h>

#define MALIGN_CDEV_NAME "malign"

#define MALIGN_MAGIC 'm'
#define MALIGN_IOCTL _IOW(MALIGN_MAGIC, 1, malign_args)

typedef struct {
  __u8 align;
} malign_args;

#endif /* MALIGN_H_ */

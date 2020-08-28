//Стандартные заголовки для драйвера ядра.

#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/version.h>  //LINUX_VERSION_CODE, KERNEL_VERSION macros

#include <linux/miscdevice.h>  //misc_register

//Скопировано из dmatest.c
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/freezer.h>
#include <linux/init.h>
#include <linux/kthread.h>
//#include <linux/module.h>
//#include <linux/moduleparam.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/wait.h>

#include <linux/fs.h>  //fops

#include <linux/ioctl.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/atomic.h>  //atomic_t

#include <linux/highmem.h>

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/of.h>

#include <linux/string.h>

#include "mydefs.h"
#include "adr_map.h"
#include "dmadefsmy.h"
#include "supply.h"

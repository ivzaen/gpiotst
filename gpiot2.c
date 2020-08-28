#include <linux/init.h>
#include <linux/module.h>

#include <linux/delay.h>

#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>  //to use GPIO_LOOKUP

#include <linux/miscdevice.h>  //misc_register

#include <linux/fs.h>  //fops

#include <linux/irqflags.h>


#include "mydefs.h"
#include "adr_map.h"
#include "dmadefsmy.h"
#include "supply.h"

#define DEVICE_NAME "gpiotstDevice"
#define MODULE_NAME "gpiotstModule"
#define CLASS_NAME "gpiotstClass"

MODULE_LICENSE("GPL");

static int device_open(struct inode *inode, struct file *file)
{
  //При открытии выделяем структуру SPrivateData и записываем ее в file->private_data
  //void *privdata;
  ////prn("device_open(%p)\n", file);
  //privdata = kzalloc(sizeof(struct SPrivateData), GFP_KERNEL);
  //file->private_data = privdata;
  return OK;
}


struct gpiod_lookup_table gpios_table = {
  .dev_id = "gpiotst.0",
  .table = {
    GPIO_LOOKUP("gpio4", 12, "test", GPIOD_OUT_HIGH_OPEN_DRAIN),  //GPIO_ACTIVE_HIGH
//    GPIO_LOOKUP_IDX("gpio.0", 16, "led", 1, GPIO_ACTIVE_HIGH),
//    GPIO_LOOKUP_IDX("gpio.0", 17, "led", 2, GPIO_ACTIVE_HIGH),
//    GPIO_LOOKUP("gpio.0", 1, "power", GPIO_ACTIVE_LOW),
    {},
  },
};


static const struct file_operations fops = { 
  //.read = device_read,
  //.write = device_write,
  //.unlocked_ioctl = device_ioctl,
  .open = device_open,
  //.release = device_close    /* оно же close */
};

static struct miscdevice mydev = {
  MISC_DYNAMIC_MINOR, DEVICENAME, &fops
};

static int m_init(void)
{
  int ret;

  struct gpio_desc *t;

#if 0
  //See arch/x86/kernel/msr.c
  //arch/x86/kernel/cpuid.c

  // Now we will create class for this device
  pmyCharClass = class_create(THIS_MODULE, CLASS_NAME);
  if (IS_ERR(pmyCharClass))
  {
    printk(KERN_ALERT "Failed to Register Class\n");
    cdev_del(myChrDevCdev);
    kfree(myChrDevCdev);
    unregister_chrdev_region(myChrDevid, 1);
    return -1;
  }
  printk(KERN_INFO "Class created!\n");

  dev =           device_create(cpuid_class,  NULL, MKDEV(CPUID_MAJOR, cpu), NULL, "cpu%d", cpu);
  pmyCharDevice = device_create(pmyCharClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
  if (IS_ERR(pmyCharDevice))
  {
    printk(KERN_ALERT "Failed to Register Class\n");
    class_unregister(pmyCharClass);
    class_destroy(pmyCharClass);
    cdev_del(myChrDevCdev);
    kfree(myChrDevCdev);
    unregister_chrdev_region(myChrDevid, 1);
    return -1;
  }
#endif
  
  //Регистрация устройства как misc device.
  ret = misc_register(&mydev);
  if (ret != OK){
    prnerr("?misc_register ret=%d\n", ret);
    return ERR;
  } else printk(KERN_INFO "Device created!\n");


  prn("Aquire gpio.\n");

  //gpiod_add_lookup_table(&gpios_table);
  //t = gpiod_get(mydev.this_device, "test", GPIOD_OUT_HIGH);  //TODO test
  //gpiod_get_index(dev, "led", 2, GPIOD_OUT_HIGH);

  t = gpio_to_desc(108);  //W2 (in)	1-wire/DI	GPIO4_IO12	108	A3

  if(IS_ERR(t)){
    prn("?Err gpiod_get %ld\n", PTR_ERR(t));
  }else if(t==NULL){
    prn("?Got NULL gpiod.\n");
  }else{
    int i;
    unsigned long flags;
    prn("gpio get OK %p\n", t);

    ret = gpiod_direction_output(t, 1);  //direction, initial value.

    msleep(200);

    //local_irq_save(flags);
    for(i=0; i<100000; i++){
      gpiod_set_value(t, 1);
      udelay(10);
      gpiod_set_value(t, 0);
      udelay(10);
    }
    //local_irq_restore(flags);

  }

  return OK;
}

static void m_exit(void)
{
  prn("m_exit\n");
  misc_deregister(&mydev);
}

module_init(m_init);
module_exit(m_exit);
/***
Результаты измерения задержек gpio с отдельным драйвером.
Целевая задержка 10 мкс, реальная 13.0 высокий уровень, 12.92 низкий. Т.е. простой цикл, вызовы gpiod* и udelay вносят задержку около 3 мкс.
Разрешенные прерывания: дырки на 56, 92 мкс, период прерываний порядка 9 мс: w2_gpiotst_bad_irqen.png
Большие дырки по 50 мкс видны, а меньшей длины не встречаются, все ровно: w2_gpiotst_irqen_clear.png

Запрещение прерываний через local_irq_save убирает дырки полностью.












***/
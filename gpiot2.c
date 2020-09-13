#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>  //to use GPIO_LOOKUP
#include <linux/miscdevice.h>  //misc_register
#include <linux/fs.h>  //fops
#include <linux/irqflags.h>
#include <linux/timekeeping.h>  //ktime_get_ns
#include <linux/math64.h>  //деление u64

#include "mydefs.h"
#include "adr_map.h"
#include "dmadefsmy.h"
#include "supply.h"

#define DEVICE_NAME "gpiotstDevice"
#define MODULE_NAME "gpiotstModule"
#define CLASS_NAME "gpiotstClass"

MODULE_LICENSE("GPL");

//=== Фрагмент для вклейки

#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>  //to use GPIO_LOOKUP

static struct gpio_desc *t = NULL;

void SetupGpio(void){
  int ret;
  t = gpio_to_desc(108);  //W2 (in)	1-wire/DI	GPIO4_IO12	108	A3
  if (IS_ERR(t)){
    pr_err("?Err gpiod_get %ld\n", PTR_ERR(t));

  } else if (t == NULL){
    pr_err("?Got NULL gpiod.\n");

  } else{
    pr_info("gpio get OK %p\n", t);
    ret = gpiod_direction_output(t, 1);  //direction, initial value.
  }

}

void mark0(void){
  if (!t) SetupGpio();
  gpiod_set_value(t, 0);

}

void mark1(void){
  if (!t) SetupGpio();
  gpiod_set_value(t, 1);

}

//===
//=== Фрагмент 2

#include <asm/io.h>  //ioremap

static volatile u32 *gpio_dr = NULL;

//Маска
#define GPIO12 (1<<12)

int SetupGpioReg(void)
{
  if(gpio_dr == NULL) {
    gpio_dr = ioremap(0x20A8000, 4);
    if(gpio_dr != NULL) {
      pr_info("Remapped %p\n", gpio_dr);
      return -EIO;
    } else {
      pr_err("Failed remap.\n");
      return OK;
    }
  
  }
  return OK;
}

void unSetupGpio(void) {
  if(gpio_dr) iounmap(gpio_dr);
}

void mark0Reg(void) 
{
  //Будем быстрее чем writel.
  *gpio_dr = (*gpio_dr) & ~GPIO12;
}

void mark1Reg(void)
{
  //Будем быстрее чем writel.
  *gpio_dr = (*gpio_dr) | GPIO12;
}

//===

static int delayh=10, delayl=10, niter=100000;  //Задержка высокого уровня, низкого уровня, число итераций цикла.
module_param(delayh, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
MODULE_PARM_DESC(delayh, "Delay high, usec");
module_param(delayl, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
MODULE_PARM_DESC(delayl, "Delay low, usec");
module_param(niter, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
MODULE_PARM_DESC(niter, "niter - number of iterations");

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
    GPIO_LOOKUP("gpio4", 12, "test", GPIO_ACTIVE_HIGH),  //was: GPIOD_OUT_HIGH_OPEN_DRAIN  GPIO_ACTIVE_HIGH
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

#if 0
static struct miscdevice mydev = {
  MISC_DYNAMIC_MINOR, DEVICENAME, &fops
};
#endif

static int m_init(void)
{

#if 0
  int ret;

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
  
  //Регистрация устройства как misc device.
  ret = misc_register(&mydev);
  if (ret != OK){
    prnerr("?misc_register ret=%d\n", ret);
    return ERR;
  } else printk(KERN_INFO "Device created!\n");
#endif

  int i;
  u64 t1, t2; 
  unsigned v;

  //gpiod_add_lookup_table(&gpios_table);
  //t = gpiod_get(mydev.this_device, "test", GPIOD_OUT_HIGH);  //TODO test
  //gpiod_get_index(dev, "led", 2, GPIOD_OUT_HIGH);

  pr_info("SetupGpio()\n");

  SetupGpio();
  SetupGpioReg();

  msleep(20);

  t1= ktime_get_ns();
  //local_irq_save(flags);
  for (i = 0; i < niter; i++){
    mark1();
    udelay(delayh);
    mark0();
    udelay(delayl);
  }
  //local_irq_restore(flags);
  t2 = ktime_get_ns();
  v = (unsigned)div_u64(t2 - t1, niter) / 2;
  pr_info("giod_set_value + delay. niter %d, t1 %llx, t2 %llx, result %d\n", niter, t1, t2, v);

  t1 = ktime_get_ns();
  //local_irq_save(flags);
  for (i = 0; i < niter; i++){
    mark1Reg();
    udelay(delayh);
    mark0Reg();
    udelay(delayl);
  }
  //local_irq_restore(flags);
  t2 = ktime_get_ns();
  v = (unsigned)div_u64(t2 - t1, niter) / 2;
  pr_info("gio reg + delay. niter %d, t1 %llx, t2 %llx, result %d\n", niter, t1, t2, v);

  t1 = ktime_get_ns();
  //local_irq_save(flags);
  for (i = 0; i < niter; i++){
    mark1();
    mark0();
    mark1();
    mark0();
  }
  //local_irq_restore(flags);
  t2 = ktime_get_ns();
  v = (unsigned)div_u64(t2 - t1, niter) / 4;
  pr_info("giod_set_value. niter %d, t1 %llx, t2 %llx, result %d\n", niter, t1, t2, v);

  t1 = ktime_get_ns();
  //local_irq_save(flags);
  for (i = 0; i < niter; i++){
    mark1Reg();
    mark0Reg();
    mark1Reg();
    mark0Reg();
  }
  //local_irq_restore(flags);
  t2 = ktime_get_ns();
  v = (unsigned)div_u64(t2 - t1, niter) / 4;
  pr_info("giod reg. niter %d, t1 %llx, t2 %llx, result %d\n", niter, t1, t2, v);


  t1 = ktime_get_ns();
  {
  volatile unsigned int j=0;

    for (i = 0; i < niter; i++){
      for (j = 0; j < 1000; j++);
    }

  }
  t2 = ktime_get_ns();
  v = (unsigned)div_u64(t2 - t1, niter);
  pr_info("Integer operations. niter %d, t1 %llx, t2 %llx, result [ns/1000] %d\n", niter, t1, t2, v);


  return OK;
}

static void m_exit(void)
{
  prn("m_exit\n");

#if 0
  misc_deregister(&mydev);
#endif
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
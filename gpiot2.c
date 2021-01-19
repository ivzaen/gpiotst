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

static struct gpio_desc *gpio_a = NULL, *gpio_b = NULL;

int SetupGpio(void){
  int ret = OK;
  gpio_a = gpio_to_desc(11);  //W1 (in)		1-wire/DI	GPIO1_IO11	11	P14
  gpio_b = gpio_to_desc(108);  //W2 (in)	1-wire/DI	GPIO4_IO12	108	A3
  if (IS_ERR(gpio_a) || IS_ERR(gpio_b)){
    pr_err("?Err gpiod_get %ld\n", PTR_ERR(gpio_a));
    pr_err("?Err gpiod_get %ld\n", PTR_ERR(gpio_b));
  } else{
    pr_info("gpio's get OK: %p %p\n", gpio_a, gpio_b);
    ret = gpiod_direction_output(gpio_a, 1);  //direction, initial value.
    if(ret) pr_err("?Err gpiod_direction_output gpio_a\n");
    ret = gpiod_direction_output(gpio_b, 1);  //direction, initial value.
    if(ret) pr_err("?Err gpiod_direction_output gpio_b\n");
  }
  return ret;
}

void markA0(void){
  if (!gpio_a) SetupGpio();
  gpiod_set_value(gpio_a, 0);
}

void markA1(void){
  if (!gpio_a) SetupGpio();
  gpiod_set_value(gpio_a, 1);
}

void markB0(void){
  if (!gpio_b) SetupGpio();
  gpiod_set_value(gpio_b, 0);
}

void markB1(void){
  if (!gpio_b) SetupGpio();
  gpiod_set_value(gpio_b, 1);
}

//===

//=== Фрагмент 2

#include <asm/io.h>  //ioremap

//Регистры контроллера. gpio_dr_a[0]=GPIO1_DR, gpio_dr_a[1]=GPIO1_GDIR (направление, 1=output).
static volatile u32 *gpio_dr_a = NULL;
static volatile u32 *gpio_dr_b = NULL;

//Маска
#define GPIOA (1<<11)
#define GPIOB (1<<12)
#define OK 0

static int SetupGpioReg(void)
{
  int ret= OK;
  if(gpio_dr_a == NULL) {
    gpio_dr_a = ioremap(0x209C000, 8);  //GPIO1, 4 bytes. imx proc ref p.1141.
    gpio_dr_b = ioremap(0x20A8000, 8);  //GPIO4, 4 bytes.
    if(gpio_dr_a != NULL) {
      pr_info("Remapped A %p\n", gpio_dr_a);
      ret = OK;
    } else {
      pr_err("Failed remap A.\n");
      ret = -EIO;
    }
    if(ret) return ret;

    if (gpio_dr_b != NULL) {
      pr_info("Remapped B %p\n", gpio_dr_b);
      ret = OK;
    } else {
      pr_err("Failed remap B.\n");
      ret = -EIO;
    }
    gpio_dr_a[1] |= GPIOA;  //output direction
    gpio_dr_b[1] |= GPIOB;  //output.

  }
  return ret;
}

static void unSetupGpio(void) {
  if (gpio_dr_a) iounmap(gpio_dr_a);
  if (gpio_dr_b) iounmap(gpio_dr_b);
}

static void mark0RegA(void)
{
  if (!gpio_dr_a) SetupGpioReg();
  //Будем быстрее чем writel.
  *gpio_dr_a = (*gpio_dr_a) & ~GPIOA;
}

static void mark1RegA(void)
{
  if (!gpio_dr_a) SetupGpioReg();
  //Будем быстрее чем writel.
  *gpio_dr_a = (*gpio_dr_a) | GPIOA;
}
static void mark0RegB(void)
{
  if (!gpio_dr_b) SetupGpioReg();
  //Будем быстрее чем writel.
  *gpio_dr_b = (*gpio_dr_b) & ~GPIOB;
}

static void mark1RegB(void)
{
  if (!gpio_dr_b) SetupGpioReg();
  //Будем быстрее чем writel.
  *gpio_dr_b = (*gpio_dr_b) | GPIOB;
}

//===

void markA1writel(void){
  u32 val;
  if (!gpio_dr_b) SetupGpioReg();
  val = readl(gpio_dr_a);
  writel(val | GPIOA, gpio_dr_a);
}

void markA0writel(void){
  u32 val;
  if (!gpio_dr_a) SetupGpioReg();
  val = readl(gpio_dr_a);
  writel(val & ~GPIOA, gpio_dr_a);
}
void markB1writel(void){
  u32 val;
  if (!gpio_dr_b) SetupGpioReg();
  val = readl(gpio_dr_b);
  writel(val | GPIOB, gpio_dr_b);
}

void markB0writel(void){
  u32 val;
  if (!gpio_dr_b) SetupGpioReg();
  val = readl(gpio_dr_b);
  writel(val & ~GPIOB, gpio_dr_b);
}


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
//=== Взято из w1_therm.c

/**
 * w1_DS18B20_convert_temp() - temperature computation for DS18B20
 * @rom: data read from device RAM (8 data bytes + 1 CRC byte)
 *
 * Can be called for any DS18B20 compliant device.
 *
 * Return: value in millidegrees Celsius.
 */
static inline int w1_DS18B20_convert_temp(u8 rom[9])
{
	u16 bv;
	s16 t;

	/* Signed 16-bit value to unsigned, cpu order */
	bv = le16_to_cpup((__le16 *)rom);

	/* Config register bit R2 = 1 - GX20MH01 in 13 or 14 bit resolution mode */
	if (rom[4] & 0x80) {
		/* Insert two temperature bits from config register */
		/* Avoid arithmetic shift of signed value */
		bv = (bv << 2) | (rom[4] & 3);
		t = (s16) bv;	/* Degrees, lowest bit is 2^-6 */
		return (int)t * 1000 / 64;	/* Sign-extend to int; millidegrees */
	}
	t = (s16)bv;	/* Degrees, lowest bit is 2^-4 */
	return (int)t * 1000 / 16;	/* Sign-extend to int; millidegrees */
}

//===

struct testram {
	u8 ram[9];  //scratchpad ram
	int temp;  //correct temp value
};

static int m_init(void)
{
        #define N 20
	struct testram tram[N] = {
		//DS18B20. Младший бит дает 2^-4 *1000 = 62.5 миллиградусов.
		//Чтобы посчитать выходное значение темп-ры, умножаем число в битах (от любого бита до 2^-4 включительно) на миллиградусы одного бита. 2^-4*1000* <биты>
		{ { 0xD0, 0x7, 0, 0, 0, 0, 0, 0, 0 }, 125000 },
		{ { 0x50, 0x05, 0, 0, 0, 0, 0, 0, 0 }, 85000 },
		{ { 0x91, 0x01, 0, 0, 0, 0, 0, 0, 0 }, 25062 },
		{ { 0xA2, 0x00, 0, 0, 0, 0, 0, 0, 0 }, 10125 },
		{ { 0x8, 0x0, 0, 0, 0, 0, 0, 0, 0 }, 500 },
		{ { 0x0, 0x0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
	        { { 0xF8, 0xFF, 0, 0, 0, 0, 0, 0, 0 }, -500 },
		{ { 0x5E, 0xFF, 0, 0, 0, 0, 0, 0, 0 }, -10125 },
		{ { 0x6F, 0xFE, 0, 0, 0, 0, 0, 0, 0 }, -25062 },
		{ { 0x90, 0xFC, 0, 0, 0, 0, 0, 0, 0 }, -55000 },
		//GX20MH01. Младший бит дает 2^-6 *1000 = 15.625 миллиградусов.
		//Чтобы включить расчет по типу GX20MH01, ставим rom[4]|=0x80.
		{ { 0xD0, 0x7, 0, 0, 0x83, 0, 0, 0, 0 }, 125046 },
		{ { 0x50, 0x05, 0, 0, 0x82, 0, 0, 0, 0 }, 85031 },  //Формула для проверки  ((0x550<<2) +2) *2^-6*1000
		{ { 0x91, 0x01, 0, 0, 0x81, 0, 0, 0, 0 }, 25078 },
		{ { 0xA2, 0x00, 0, 0, 0x80, 0, 0, 0, 0 }, 10125 },
		{ { 0x8, 0x0, 0, 0, 0x83, 0, 0, 0, 0 }, 546 },
		{ { 0x0, 0x0, 0, 0, 0x82, 0, 0, 0, 0 }, 31 },
	        { { 0xF8, 0xFF, 0, 0, 0x83, 0, 0, 0, 0 }, -453 },	// ((0xFFF8<<2) +3 - 0x40000)*2^-6*1000
		{ { 0x5E, 0xFF, 0, 0, 0x80, 0, 0, 0, 0 }, -10125 },	// ((0xFFF8<<2) +3 - 0x40000)*2^-6*1000
		{ { 0x6F, 0xFE, 0, 0, 0x81, 0, 0, 0, 0 }, -25046 },	// ((0xFE6F<<2) +1 - 0x40000)*2^-6*1000
		{ { 0x90, 0xFC, 0, 0, 0x82, 0, 0, 0, 0 }, -54968 }	// ((0xFC90<<2) +2 - 0x40000)*2^-6*1000

	};
	const char msgfail[]= "FAIL!";
	const char msgok[]= "ok";
	int i, v;
	for(i=0; i<N; i++){
		v = w1_DS18B20_convert_temp(tram[i].ram);
		{
			const char *msg= tram[i].temp == v ? msgok : msgfail;
			pr_info("conv i=%d mustbe_temp=%d temp=%d - %s", i, tram[i].temp, v, msg);
		}

	}



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

#if 0
  int i;
  u64 t1, t2;
  unsigned v;

  //gpiod_add_lookup_table(&gpios_table);
  //t = gpiod_get(mydev.this_device, "test", GPIOD_OUT_HIGH);  //TODO test
  //gpiod_get_index(dev, "led", 2, GPIOD_OUT_HIGH);

  pr_info("SetupGpio()\n");

  if(SetupGpio()) {
    pr_err("?Err SetupGpio\n");
    return -EIO;
  }
  if(SetupGpioReg()) {
    pr_err("?Err SetupGpioReg\n");
    return -EIO;
  }

  for(i=0; i<3; i++){
    udelay(20);

    mark0RegB();
    mark1RegB();

    markA0();
    markA1();

    mark0RegB();
    mark1RegB();

    udelay(20);
  }
#endif

#if 0

  t1= ktime_get_ns();
  //local_irq_save(flags);
  for (i = 0; i < niter; i++){
    markB1();
    udelay(delayh);
    markB0();
    udelay(delayl);
  }
  //local_irq_restore(flags);
  t2 = ktime_get_ns();
  v = (unsigned)div_u64(t2 - t1, niter) / 2;
  pr_info("gpiod_set_value + delay. niter %d, t1 %llx, t2 %llx, result %d\n", niter, t1, t2, v);

  t1 = ktime_get_ns();
  //local_irq_save(flags);
  for (i = 0; i < niter; i++){
    mark1RegB();
    udelay(delayh);
    mark0RegB();
    udelay(delayl);
  }
  //local_irq_restore(flags);
  t2 = ktime_get_ns();
  v = (unsigned)div_u64(t2 - t1, niter) / 2;
  pr_info("Reg + delay. niter %d, t1 %llx, t2 %llx, result %d\n", niter, t1, t2, v);

  t1 = ktime_get_ns();
  //local_irq_save(flags);
  for (i = 0; i < niter; i++){
    //mark1RegB();
    gpiod_set_value_nocheck(gpio_a, 0);
    //mark0RegB();
    gpiod_set_value_nocheck(gpio_a, 1);
  }
  //local_irq_restore(flags);
  t2 = ktime_get_ns();
  v = (unsigned)div_u64(t2 - t1, niter) / 2;
  pr_info("gpiod_set_value_nocheck  niter %d, t1 %llx, t2 %llx, result %d\n", niter, t1, t2, v);

#endif

#if 0
  t1 = ktime_get_ns();
  //local_irq_save(flags);
  for (i = 0; i < niter; i++){
    markA1();
    markA0();
    markA1();
    markA0();
  }
  //local_irq_restore(flags);
  t2 = ktime_get_ns();
  v = (unsigned)div_u64(t2 - t1, niter) / 4;
  pr_info("gpiod_set_value. niter %d, t1 %llx, t2 %llx, result %d\n", niter, t1, t2, v);

  t1 = ktime_get_ns();
  for (i = 0; i < niter; i++){
    markA1writel();
    markA0writel();
  }
  t2 = ktime_get_ns();
  v = (unsigned)div_u64(t2 - t1, niter) / 2;
  pr_info("writel, niter %d, t1 %llx, t2 %llx, result %d\n", niter, t1, t2, v);

  t1 = ktime_get_ns();
  //local_irq_save(flags);
  for (i = 0; i < niter; i++){
    mark1RegA();
    mark0RegA();
    mark1RegA();
    mark0RegA();
  }
  //local_irq_restore(flags);
  t2 = ktime_get_ns();
  v = (unsigned)div_u64(t2 - t1, niter) / 4;
  pr_info("Reg. niter %d, t1 %llx, t2 %llx, result %d\n", niter, t1, t2, v);


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

  unSetupGpio();
#endif

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

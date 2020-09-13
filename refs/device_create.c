#include<linux/module.h>
#include<linux/init.h>
#include<linux/fs.h>
#include<linux/device.h>
#include<linux/kernel.h>
#include<linux/slab.h>
#include<linux/uaccess.h>
#include<linux/stat.h>
#include<linux/cdev.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/kdev_t.h>


#define DEVICE_NAME "myCharDevice"
#define MODULE_NAME "myCharDriver"
#define CLASS_NAME "myCharClass"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("YASH BHATT");
MODULE_VERSION(".01");

static char *bufferMemory;
static int bufferPointer;
static int bufferSize = 15;
static dev_t myChrDevid;
static struct cdev *myChrDevCdev;
static struct class *pmyCharClass;
static struct device *pmyCharDevice;
int majorNumber = 0;

static int charDriverOpen(struct inode *inodep, struct file *filep);
static int charDriverClose(struct inode *inodep, struct file *filep);
static ssize_t charDriverWrite(struct file *filep, const char *buffer, size_t len, loff_t *offset);
static ssize_t charDriverRead(struct file *filep, char *buffer, size_t len, loff_t *offset);
static int charDriverEntry(void);
static void charDriverExit(void);
static ssize_t attrShowData(struct device*, struct device_attribute*, char*);
static ssize_t attrStoreData(struct device*, struct device_attribute*, const char*, size_t);
static ssize_t attrShowBuffer(struct device*, struct device_attribute*, char*);
static ssize_t attrStoreBuffer(struct device*, struct device_attribute*, const char*, size_t);

/* The following function is called when the file placed on the sysfs is accessed for read*/
static ssize_t attrShowData(struct device* pDev, struct device_attribute* attr, char* buffer)
{
    printk(KERN_INFO "MESG: The data has been accessed through the entry in sysfs\n");
    if (bufferPointer == 0)
    {
        printk(KERN_WARNING "Thre is no data to read from  buffer!\n");
        return -1;
    }
    strncpy(buffer, bufferMemory, bufferPointer);
    /* Note : Here we can directly use strncpy because we are already in kernel space and do not need to translate address*/
    return bufferPointer;
}

static ssize_t attrStoreData(struct device* pDev, struct device_attribute* attr, const char* buffer, size_t length)
{
    printk(KERN_INFO "Writing to attribute\n");
    bufferPointer = length;
    strncpy(bufferMemory, buffer, length);
    return length;
}

static ssize_t attrShowBuffer(struct device* pDev, struct device_attribute* attr, char* buffer)
{
    int counter;
    int temp = bufferSize;
    char bufferSizeArray[4] = {0};
    counter = 3;
    //printk(KERN_INFO "Buffer = %d\n",bufferSize % 10);
    do
    {
        bufferSizeArray[counter] = '0' + (bufferSize % 10);
        //printk(KERN_INFO "Character at %d is : %c\n",counter,bufferSizeArray[counter]);
        bufferSize /= 10;
        counter--;
    }
    while(counter != -1);
    strncpy(buffer, bufferSizeArray, 4);
    bufferSize = temp;
    /* Note : Here we can directly use strncpy because we are already in kernel space and do not need to translate address*/
    return 4;
}

static ssize_t attrStoreBuffer(struct device* pDev, struct device_attribute* attr, const char* buffer, size_t length)
{
    int counter;
    bufferPointer = length;
    //printk(KERN_INFO "Length : %d With first char %c\n",length,buffer[0]);
    bufferSize = 0;
    for (counter = 0; counter < length-1 ; counter++)
    {
        bufferSize = (bufferSize * 10) + (buffer[counter] - '0') ;
    }
    //printk(KERN_INFO "Buffer size new : %d\n",bufferSize);
    return length;
}
/* These macros converts the function in to instances dev_attr_<_name>*/
/* Defination of the macro is as follows : DEVICE_ATTR(_name, _mode, _show, _store) */
/* Note the actual implementation of the macro makes an entry in the struct device_attribute. This macro does that for us */
static DEVICE_ATTR(ShowData, S_IRWXU, attrShowData, attrStoreData); // S_IRUSR gives read access to the user 
static DEVICE_ATTR(Buffer, S_IRWXU, attrShowBuffer, attrStoreBuffer);   // S_IRUSR gives read access to the user 

static struct file_operations fops = 
{
    .open = charDriverOpen,
    .release = charDriverClose,
    .read = charDriverRead,
    .write = charDriverWrite,
};


static int __init charDriverEntry()
{
    int returnValue;
    //majorNumber = register_chrdev(0, DEVICE_NAME, &fops);

    returnValue = alloc_chrdev_region(&myChrDevid, 0, 1, DEVICE_NAME); 
    /* This function takes 4 arguments - dev_t address, start of minor number, range/count of minor number, Name; Note - unlike register_chrdev fops have not
        yet been tied to the major number */

    if (returnValue < 0)
    {
        printk(KERN_ALERT "ERROR : can not aquire major number! error %d",returnValue);
        return -1;
    }
    printk(KERN_INFO "Aquired Major Number! : %d\n", MAJOR(myChrDevid));

    //cdev_init(&myChrDevCdev,&fops);
    myChrDevCdev = cdev_alloc();
    if (IS_ERR(myChrDevCdev))
    {
        printk(KERN_ALERT "Failed to allocate space for CharDev struct\n");
        unregister_chrdev_region(myChrDevid, 1);
        return -1;
    }
        cdev_init(myChrDevCdev,&fops);
        myChrDevCdev->owner = THIS_MODULE;
        //myChrDevCdev->ops = &fops;/* this function inits the c_dev structure with memset 0 and then does basic konject setup and then adds fops to cdev struct*/


    /* this function adds the cdev to the kernel structure so that it becomes available for the users to use it */


    // Now we will create class for this device
    pmyCharClass = class_create(THIS_MODULE,CLASS_NAME);
    if (IS_ERR(pmyCharClass))
    {
        printk(KERN_ALERT "Failed to Register Class\n");
        cdev_del(myChrDevCdev);
        kfree(myChrDevCdev);
        unregister_chrdev_region(myChrDevid, 1);
        return -1;
    }
    printk(KERN_INFO "Class created!\n");

    pmyCharDevice = device_create(pmyCharClass, NULL, MKDEV(majorNumber,0),NULL,DEVICE_NAME);
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
    printk(KERN_INFO "Device created!\n");

    returnValue = cdev_add(myChrDevCdev, myChrDevid, 1);
    if (returnValue < 0)
    {
        printk(KERN_ALERT "Failed to add chdev \n");
        return -1;
    }
    /* We now have created the class and we have aquired major numer. But we have not yet tied out created fileops with anything. 
        We will do that now */
    //returnValue = cdev_init(cdev)     
    printk(KERN_INFO "Now We will create the attribute entry in sysfs\n");
    /* the function used is device_create_file(struct device *, struct device_attribute*) */
    device_create_file(pmyCharDevice, &dev_attr_ShowData);      // The second argumnet is the structure created by the DEVICE_ATTR macro
    device_create_file(pmyCharDevice, &dev_attr_Buffer);
    return 0;
}

static void __exit charDriverExit()
{
    device_remove_file(pmyCharDevice, &dev_attr_Buffer);
    device_remove_file(pmyCharDevice, &dev_attr_ShowData);
    device_destroy(pmyCharClass, MKDEV(majorNumber,0));
    class_unregister(pmyCharClass);
    class_destroy(pmyCharClass);
    //unregister_chrdev(majorNumber,DEVICE_NAME);
    cdev_del(myChrDevCdev);
    unregister_chrdev_region(myChrDevid, 1);
    kfree(myChrDevCdev);
    printk(KERN_INFO "Unmounting module done !\n");
}

static int charDriverOpen(struct inode *inodep, struct file *filep)
{
    if ((filep->f_flags & O_ACCMODE) != O_RDWR)
    {
        printk(KERN_ALERT "WARNING : This driver can only be opened in both read and write mode\n");
        return -1;
    }
    printk(KERN_INFO "INFO : CHARATER DRIVER OPENED\n");
    bufferMemory = kmalloc(bufferSize,GFP_KERNEL);
    bufferPointer = 0;
    return 0;   
}

static int charDriverClose(struct inode *inodep, struct file *filep)
{
    kfree(bufferMemory);
    printk(KERN_INFO "INFO : CHARACTER DRIVER CLOSED\n");
    return 0;
}

static ssize_t charDriverWrite(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
    // Here we will only allow to write one byte of data 
    if (len > bufferSize)
    {
        printk(KERN_WARNING "Attempted to write data larger than 15 byte!\n");
        return 0;
    }
    //bufferMemory[bufferPointer] = *buffer;
    copy_from_user(bufferMemory, buffer, len);
    bufferPointer += len;
    return len;
}

static ssize_t charDriverRead(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
    if(len > bufferSize || len > bufferPointer)
    {
        printk(KERN_WARNING "Attempting to read more than buffer size ! Deny\n");
        return 0;
    }
    copy_to_user(buffer, bufferMemory, len);
//  buffer[0] = bufferMemory[0];
    bufferPointer -= len;
    return len;
}

module_init(charDriverEntry);
module_exit(charDriverExit);
module_param(bufferSize, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bufferSize, "Buffer Memory Size [15]");
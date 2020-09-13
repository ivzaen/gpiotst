//Основной файл драйвера ядра.
//Драйвер не должен обращаться к регистрам ПЛИС без команды приложения, т.к. модуля ПЛИС может и не быть, о наличии знает приложение.

#define IO_STRUCTS  //define variables
#include "drivinclude.h"

//=== Параметры модуля
//
//static char test_channel[20];
//module_param_string(channel, test_channel, sizeof(test_channel), S_IRUGO);
//MODULE_PARM_DESC(channel, "Bus ID of the channel to test (default: any)");
//
//static char test_device[20];
//module_param_string(device, test_device, sizeof(test_device), S_IRUGO);
//MODULE_PARM_DESC(device, "Bus ID of the DMA Engine to test (default: any)");

//=== Данные

//Надо иметь два отображения памяти, для S2MM и для MM2S,
//поэтому драйвер из приложения открывается дважды, а в device_open выделяется две структуры SPrivateData.
//Структура присваивается полям file->private_data, которая для таких целей предназначена.
//При дальнейших вызовах драйвера поле file->private_data кастится в SPrivateData*
struct SPrivateData {
  struct SMyDmaMemory ms; //Запрос от app. Здесь сохраняются нужные значения от Submit до Release
  struct page **pdesc, **pdest;  //Массивы struct page* для дескрипторов и данных.
  struct DmaDescSw *pswd;  //Программные дескрипторы ядра. Количество равно ms.dest_npage.
  int nswd; //Количество элементов в pswd, копируется из ms.dest_npage.
};

//--- Прерывания
atomic_t irq_mm2s_count = ATOMIC_INIT(0);  //Счетчик прерываний. See LDD3.pdf / Atomic Variables p.142.
atomic_t irq_s2mm_count = ATOMIC_INIT(0);  //Счетчик прерываний

ulong irq_mm2s_virt=0, irq_s2mm_virt=0;  //Виртуальные номера прерываний для вызова request_irq. Определяются относительно тех, что заданы в DTS.

static DECLARE_WAIT_QUEUE_HEAD(irq_mm2s_q);
static DECLARE_WAIT_QUEUE_HEAD(irq_s2mm_q);

static struct dma_chan *tx_chan=NULL;
static struct dma_chan *rx_chan=NULL;

//=== Функции

irqreturn_t irq_mm2s_handler(int irq, void *dev_id){
  //volatile struct DMAREGS_SG_MODE *dma= (volatile struct DMAREGS_SG_MODE *) io_axi_dma.access;
  //dma->MM2S_DMASR|=(IrqBits);
  atomic_inc(&irq_mm2s_count);  //Счетчик прерываний
  wake_up_interruptible(&irq_mm2s_q);  //Пробуждение ждущих потоков, если есть.
  return IRQ_HANDLED;
}

irqreturn_t irq_s2mm_handler(int irq, void *dev_id){
  //volatile struct DMAREGS_SG_MODE *dma= (volatile struct DMAREGS_SG_MODE *) io_axi_dma.access;
  //dma->S2MM_DMASR|=(IrqBits);  //Очищаем прерывание
  atomic_inc(&irq_s2mm_count);  //Счетчик прерываний
  wake_up_interruptible(&irq_s2mm_q);  //Пробуждение ждущих потоков, если есть.
  return IRQ_HANDLED;
}


//Создать отображение физ. памяти в память ядра.
//Через указатель access можно обращаться на чтение и запись к физ. памяти.
//Но:
//  Копирование .access структур struct1 = *(STRUCT1*)access не работает, выдает external imprecise abort 0x406. В .s видны ldmia, stmia размером ulong.
//  Копирование из *access через copy_to_user работает, применено. Сколько раз она читает - не проверял (char*4*n или ulong*1*n). Можно посм. через chipscope.
//  Копирование через memcpy работает, см. C:\2delete\USB2\Docs\ZYNQ\ARM_DOC\memcpy_on_arm_gcc.txt
//  Через ulong_ptr[i]=((ulong*)access)[i] должно работать, можно исп.
void IOCreateMapping(struct IOMAPPING *map){
  if(map->fMapOk) return;  //Уже есть маппинг.
  
  map->access = ioremap(map->physAddr, map->length);
  if(map->access){
    prn("MCreateMapping: phys=%lX length=%zu to addr=%p.\n", (unsigned long)map->physAddr, map->length, map->access);
    map->fMapOk=1; 
  }else{
    map->fMapOk=0;
    prnerr("?MCreateMapping error.\n");
  }
}

void IODeleteMapping(struct IOMAPPING *map){
  if(!map->fMapOk){
    prnerr("?IODeleteMapping: map->fMapOk=0\n");
  }else{
    iounmap(map->access);
    map->fMapOk=0; //deleted
  }
}

//==== Mytop. Должен быть сделан MCreateMapping.

//Передача значений через поля структуры SPrivateData *pv
//Вход: ms
//Выход: pdesc, pdest, pswd, nswd
//Прикалываются страницы пользователя, выделяется массив софт. дескрипторов pswd, делаются отображения для dma и для доступа к дескрипторам из ядра.
//Обнуляются апп. дескрипторы и заполняются их физ. адреса.
//Если будут появляться return ERR, то возникнут проблемы с невозвращенной памятью.
int DmaSubmitMemory(struct SPrivateData *pv){
  //AM struct device *dev= NULL; //Пока работает.
  struct device *dev=tx_chan?tx_chan->device->dev:NULL;
  int desc_npage, dest_npage, desc_npage_mustbe;
  int 
    i,   //Номер страницы данных
    jd,  //Номер дескриптора на странице, 0..NDESCPAGE-1
    jp;  //Номер страницы с аппаратными дескрипторами, 0..desc_npage-1
  struct DMADESC *kpd=NULL;  //указатель ядра на текущ. дескриптор
  struct DMADESC *kpdPrev; //указатель ядра на предыдущий дескриптор, или NULL в начале.
  dma_addr_t descPhy=0, destPhy=0; //bus addresses начала страницы с апп. деск. и начала стр. данных.
  //enum dma_data_direction dir= DMA_DEV_TO_MEM;

  //prn("DmaSubmitMemory: descbuf=%p - %lu pages, destbuf=%p - %lu pages\n", 
  //  pv->ms.descbuf, (unsigned long)pv->ms.desc_npage, pv->ms.destbuf, (unsigned long)pv->ms.dest_npage); 

  //Проверим, чтобы страниц для дескрипторов было, сколько надо.
  desc_npage_mustbe= ( pv->ms.dest_npage + NDESCPAGE-1 ) / NDESCPAGE;
  if(pv->ms.desc_npage != desc_npage_mustbe) return 5142; //Incorrect number of pages for descriptors

  pv->pdesc=kzalloc(pv->ms.desc_npage*sizeof(struct page*), GFP_KERNEL);
  pv->pdest=kzalloc(pv->ms.dest_npage*sizeof(struct page*), GFP_KERNEL);

  down_read(&current->mm->mmap_sem); //Так положено, LDD3 p.436.
  #if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)  //Новый прототип get_user_pages
    desc_npage= get_user_pages((unsigned long)pv->ms.descbuf, pv->ms.desc_npage, FOLL_WRITE, pv->pdesc, NULL);
    dest_npage= get_user_pages((unsigned long)pv->ms.destbuf, pv->ms.dest_npage, FOLL_WRITE, pv->pdest, NULL);
  #else  //старый прототип
    desc_npage= get_user_pages(current, current->mm, (unsigned long)pv->ms.descbuf, pv->ms.desc_npage, 1, 0, pv->pdesc, NULL);
    dest_npage= get_user_pages(current, current->mm, (unsigned long)pv->ms.destbuf, pv->ms.dest_npage, 1, 0, pv->pdest, NULL);
  #endif
  up_read(&current->mm->mmap_sem); //Вернуть.

  //prn("get_user_pages pinned %d desc pages, %d dest pages\n", desc_npage, dest_npage);

  if(desc_npage!=pv->ms.desc_npage || dest_npage!=pv->ms.dest_npage) return 8965;  //get_user_pages pinned invalid number of pages

  //Проверим размер дескриптора
  if(sizeof(struct DMADESC)!=DMADESCSIZE) return 9476;  //sizeof(struct DmaDescSw)=%d  !=DMADESCSWSIZE

  //Соотношение дескриптора и страницы
  if(DMADESCSIZE*NDESCPAGE!=PAGESIZEB) return 1849;  //prnerr("?DMADESCSWSIZE*NDESCPAGE==%d  !=DMADESCSWSIZE\n", DMADESCSIZE*NDESCPAGE!=PAGESIZEB);

  //выделяем память для программных дескрипторов.
  pv->nswd= pv->ms.dest_npage; //дескрипторов ровно столько, сколько юзер дал страниц.
  pv->pswd= kzalloc(pv->nswd*sizeof(*pv->pswd), GFP_KERNEL);


  ////HACK
  //for(i=0; i<desc_npage; i++){
  //  //__dma_page_cpu_to_dev(pdesc[i], 0, PAGESIZEB, DMA_MEM_TO_DEV);
  //  //dma_sync_single_for_device(dev, pswd[i*NDESCPAGE].phyDesc, PAGESIZEB, DMA_MEM_TO_DEV);
  //  unsigned long paddr = page_to_phys(pdesc[i]);
  //  prn("Desc page %d padr=%lX \n", i, paddr);
  //}

  jp=jd=0;  //сброс всех индексов
  kpdPrev= NULL; //начало

  //По всем страницам пользователя, выделяем память и делаем отображения.
  for(i=0; i<dest_npage; i++){ 
    //Если это нулевой дескриптор на странице дескрипторов, делаем физ.адрес страницы дескрипторов.
    if(jd==0){ 
      descPhy= /*AM не компилится для ARM64 arm_dma_ops.map_page*/dma_map_page(dev, pv->pdesc[jp], 0, PAGESIZEB, DMA_BIDIRECTIONAL/*AM, 0*/);
      if(dma_mapping_error(dev, descPhy)) return 8787;  //prnerr("?dma_map_page desc, i=%d\n", i);
      //dma_sync_single_for_cpu(dev, descPhy, PAGESIZEB, DMA_MEM_TO_DEV);
      kpd=kmap(pv->pdesc[jp]);  //доступ к дескриптору для ядра
      memset(kpd, 0, PAGESIZEB); //Обнулить всю страницу дескрипторов
      //set_page_dirty(pdesc[jp]); //Стр. изменена в памяти
    }
    //Страницу данных отображаем всегда.
    destPhy= /*AM не компилится для ARM64 arm_dma_ops.map_page*/dma_map_page(dev, pv->pdest[i], 0, PAGESIZEB, DMA_BIDIRECTIONAL/*AM, 0*/);
    if(dma_mapping_error(dev, destPhy)) return 8788;  //prnerr("?dma_map_page dest, i=%d\n", i);

    pv->pswd[i].kpDesc= kpd; //Указатель ядра для доступа к апп. деск.
    pv->pswd[i].phyDesc= descPhy + jd * DMADESCSIZE;  //Физ. адрес апп. деск.
    pv->pswd[i].phyDest= destPhy;

    //Адрес след. дескриптора известен только для предыдущего дескриптора, это физ.ад. данного дескриптора.
    if(kpdPrev!=NULL) kpdPrev->NXTDESC= pv->pswd[i].phyDesc;
    kpdPrev= kpd;  //Указ. ядра на текущий апп. дескриптор.

    //Заполняем аппаратный дескриптор
    kpd->BUF_ADR= pv->pswd[i].phyDest;
    if(pv->ms.fMM2S) kpd->CONTROL=0; else kpd->CONTROL= PAGESIZEB;  //Для MM2S лучше не присваивать полную страницу, иначе при ошибочной установке TAILDESC пойдет 4кб мусора наружу.

    //if(i<NDESCPAGE*2) printfvar(pswd[i].phyDesc, "%lX\n"); //HACK

    ++jd; 
    ++kpd; //Увел. указатель ядра, на дескриптор на странице.
    if(jd>=NDESCPAGE){
      jd=0; //Цикл по дескриптору на странице
      ++jp;
    }
  }

  //Для последнего дескриптора след. деск. - нулевой.
  pv->pswd[dest_npage-1].kpDesc->NXTDESC = pv->pswd[0].phyDesc;

  ////HACK
  ////Синхронизируем дескрипторы и буферы для DMA:
  //for(i=0; i<desc_npage; i++){
  //  //__dma_page_cpu_to_dev(pdesc[i], 0, PAGESIZEB, DMA_MEM_TO_DEV);
  //  //dma_sync_single_for_device(dev, pswd[i*NDESCPAGE].phyDesc, PAGESIZEB, DMA_MEM_TO_DEV);
  //  unsigned long paddr = page_to_phys(pdesc[i]);
  //  //prn("Desc page %d padr=%lX phyDesc=%lX\n", i, paddr, pswd[i*NDESCPAGE].phyDesc);
  //  outer_clean_range(paddr, paddr + PAGESIZEB);
  //}

  return OK;
}

//Передача значений через поля структуры SPrivateData *pv
//Вход: pdesc, pdest, pswd, nswd
//Выход: нет, входные могут меняться.
//Возвр. страницы пользователя, освоб массив софт. дескрипторов pswd, освоб. отображения для dma и для доступа к дескрипторам из ядра.
void DmaReleaseMemory(struct SPrivateData *pv){
  int 
    i,   //Номер страницы данных
    jd,  //Номер дескриптора на странице, 0..NDESCPAGE-1
    jp;  //Номер страницы с аппаратными дескрипторами, 0..desc_npage-1
  //AM struct device *dev= NULL; //Пока работает.
  struct device *dev=tx_chan?tx_chan->device->dev:NULL;

  //Освобождаем память данных и дескрипторов
  jp=jd=0;  //сброс всех индексов
  for(i=0; i<pv->nswd; i++){

    //Убираем отображение буфера данных
    /*AM не компилится для ARM64 arm_dma_ops.unmap_page*/dma_unmap_page(dev, pv->pswd[i].phyDest, PAGESIZEB, DMA_BIDIRECTIONAL/*AM, 0*/);
    put_page(pv->pdest[i]);  //Возвр. польз. страницу данных

    //Если это нулевой дескриптор на странице, ее начало, то освобождаем всю страницу с апп. дескрипторами
    if(jd==0){ 
      /*AM не компилится для ARM64 arm_dma_ops.unmap_page*/dma_unmap_page(dev, pv->pswd[i].phyDesc, PAGESIZEB, DMA_BIDIRECTIONAL/*, 0*/);
      kunmap(pv->pdesc[jp]);  //Убираем доступ ядра к странице с апп. деск.
      put_page(pv->pdesc[jp]);  //Возвр. польз. страницу дескрипторов
    }

    ++jd; 
    if(jd>=NDESCPAGE){
      jd=0; //Цикл по дескриптору на странице
      ++jp;
    }
  }
  kfree(pv->pswd);
  kfree(pv->pdesc); 
  kfree(pv->pdest);
}

static int device_open(struct inode *inode, struct file *file)
{
  //При открытии выделяем структуру SPrivateData и записываем ее в file->private_data
  void *privdata;
  //prn("device_open(%p)\n", file);
  privdata= kzalloc(sizeof(struct SPrivateData), GFP_KERNEL);
  file->private_data = privdata;
  return OK;
}

static int device_close(struct inode *inode, struct file *file)
{
  kfree(file->private_data);
  return OK;
}

  
static ssize_t device_read(struct file *file, /* см. include/linux/fs.h   */
                           char __user * buffer,             /* буфер для сообщения */
                           size_t length,                    /* размер буфера       */
                           loff_t * offset)
{
  prn("device_read(%p,%p,%zu)\n", file, buffer, length);
  return OK;
}

static ssize_t device_write(struct file *file,
                            const char __user * buffer, size_t length, loff_t * offset)
{
  prn("device_write(%p,%s,%zu)", file, buffer, length);
  return OK;
}

int TestCopyPages(struct page **psrc, struct page **pdst, int npages){
  int x;

  for(x=0; x<npages; x++){
    void *src=kmap(psrc[0]);  //Указатель kernel logical для доступа.
    void *dst=kmap(pdst[0]);
    prn("kmap src=%p dst=%p.\n", src, dst);

    memcpy(dst, src, PAGESIZEB);

    //SetPageDirty(pages[0]); 
    SetPageDirty(pdst[0]);  //Пишут, надо всегда юзать set_page_dirty()  http://comments.gmane.org/gmane.linux.kernel.mm/999

    //убрано, т.к. это #define put_page
    //page_cache_release(psrc[0]);
    //page_cache_release(pdst[0]);

    kunmap(psrc[x]);
    kunmap(pdst[x]);
  }
  return OK;
}

//file://localhost/C:/2delete/Справочное/LANGS/LinuxProgramming/_Man-pages_syscalls/man2/ioctl.2.html
//C:\2delete\Справочное\LANGS\LinuxProgramming\DriversKernel\_ioctl_to_unlocked_ioctl.mht
//if you return a negative value from a file_operations function, the kernel interprets it as a negative errno (i.e. an error return). User code then gets -1 as a return value, with errno set to the negation of your original return value. This has nothing to do with twos-complement.
//As an example, if you return -ENOTTY from unlocked_ioctl, the user program gets -1 from ioctl and errno = ENOTTY.
//
//struct file:
// http://www.tldp.org/LDP/tlk/ds/ds.html
// http://stackoverflow.com/questions/4653100/struct-file-in-linux-driver
//struct file {
//  mode_t f_mode;
//  loff_t f_pos;
//  unsigned short f_flags;
//  unsigned short f_count;
//  unsigned long f_reada, f_ramax, f_raend, f_ralen, f_rawin;
//  struct file *f_next, *f_prev;
//  int f_owner;         /* pid or -pgrp where SIGIO should be sent */
//  struct inode * f_inode;
//  struct file_operations * f_op;
//  unsigned long f_version;
//  void *private_data;  /* needed for tty driver, and maybe others */
//};
long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
  int ret;
  struct SPrivateData *pv = (struct SPrivateData *)file->private_data;
  volatile struct DMAREGS_SG_MODE *dmaregs= (volatile struct DMAREGS_SG_MODE *) io_axi_dma.access;
  switch(ioctl_num){
  case IOCTL_MEM_SUBMIT:
    {
      ret=copy_from_user(&pv->ms, (struct SMyDmaMemory*)ioctl_param, sizeof(struct SMyDmaMemory));
      if(ret!=OK) return 4669;
      ret= DmaSubmitMemory(pv);
      return ret;
    }
  case IOCTL_MEM_RELEASE:
    {
      DmaReleaseMemory(pv);
      return OK;
    }
  case IOCTL_MEM_GETSWD:
    {
      ret=copy_to_user((void*)ioctl_param, pv->pswd, pv->ms.dest_npage*sizeof(struct DmaDescSw));
      if(ret!=OK) return 8911;
      return OK;
    }

  case IOCTL_MM2S_IRQGETCNT:
    {
      int retval = atomic_read(&irq_mm2s_count);
      atomic_sub(retval, &irq_mm2s_count);
      return retval;
    }
  case IOCTL_S2MM_IRQGETCNT:
    {
      int retval = atomic_read(&irq_s2mm_count);
      atomic_sub(retval, &irq_s2mm_count);
      return retval;
    }                      
  case IOCTL_MM2S_IRQWAIT:
    {                                    
      int remain= wait_event_interruptible_timeout(irq_mm2s_q, (dmaregs->MM2S_DMASR & IrqBits), usecs_to_jiffies(ioctl_param));
      return jiffies_to_usecs(remain);
    }

  case IOCTL_S2MM_IRQWAIT:
    {
      int remain= wait_event_interruptible_timeout(irq_s2mm_q, (dmaregs->S2MM_DMASR & IrqBits), usecs_to_jiffies(ioctl_param));
      return jiffies_to_usecs(remain);
    }

  case IOCTL_GET_BUILDINFO:
    return BuildInfoFill((struct SBUILDINFO __user *)ioctl_param);


  default: return ENOTTY; //так положено.
  }
}

static const struct file_operations fops = {
  .read = device_read,  
  .write = device_write,
  .unlocked_ioctl = device_ioctl,
  .open = device_open,
  .release = device_close    /* оно же close */
};

static struct miscdevice mydev = {
  MISC_DYNAMIC_MINOR, DEVICENAME, &fops
};

static int dmadriv_of_probe(struct platform_device *ofdev)
{
  struct resource *res;
  int n;
  int ret;

  prn("of_probe\n");

  //Определяем виртуальные номера и устанавливаем обработчики прерываний.
  //расчет номера в 0notes/ ---Прерывания
  for(n=0; n<2; n++){
    res = platform_get_resource(ofdev, IORESOURCE_IRQ, n);  //http://stackoverflow.com/questions/22961714/what-is-platform-get-resource-in-linux-driver
    if (!res) {
      prnerr("?could not get platform IRQ resource.\n");
      return ERR;
    }
    if(n==0){  //Первым по порядку в DTS идет irq S2MM
      irq_s2mm_virt = res->start;
      prn("S2MM irq in DTS: %lu\n", irq_s2mm_virt);
      ret = request_irq(irq_s2mm_virt, irq_s2mm_handler, 0 , "dmadriv_s2mm", NULL);
      if(ret!=OK){
        prnerr("?request_irq s2mm = %d\n", ret);
        return ERR;
      }
    }else if(n==1){  //Вторым по порядку в DTS идет irq MM2S.
      irq_mm2s_virt = res->start;
      prn("MM2S irq in DTS: %lu\n", irq_mm2s_virt);
      ret = request_irq(irq_mm2s_virt, irq_mm2s_handler, 0 , "dmadriv_mm2s", NULL);
      if(ret!=OK){
        prnerr("?request_irq mm2s = %d\n", ret);
        return ERR;
      }
    }
  }

  if(PAGESIZEB!=PAGE_SIZE){
    prnerr("?PAGESIZEB=%d but real PAGE_SIZE=%d.\n", (int)PAGESIZEB, (int)PAGE_SIZE);
    return ERR;
  }

  //if(platform_driver_register(&zynq_gpio_driver)!=OK){
  //  prnerr("?platform_driver_register\n");
  //  return ERR;
  //}

  IOCreateMapping(&io_mytop);
  IOCreateMapping(&io_axi_dma);

  //убрано, т.к. не делается mapping для отсутствующих корок.
  //if(!io_mytop.fMapOk || !io_axi_dma.fMapOk){
  //  prn("Failed some MCreateMapping\n");
  //  return ERR;
  //}

  //Регистрация устройства по misc способу.
  ret = misc_register(&mydev);
  if(ret!=OK){
    prnerr("?misc_register ret=%d\n", ret);
    return ERR;
  }

//AM - создаем каналы TX и RX
  tx_chan = dma_request_slave_channel(&ofdev->dev, "dma-channel-s2mm");
  //rx_chan = dma_request_slave_channel(&ofdev->dev, "dma-channel-mm2s");

  if(!tx_chan)
    prn("dma_request_slave_channel(dma-channel-s2mm) error\n");

  return OK;
}

static int dmadriv_of_remove(struct platform_device *of_dev)
{
  prn("of_remove\n");

  free_irq(irq_mm2s_virt, NULL);
  free_irq(irq_s2mm_virt, NULL);

  IODeleteMapping(&io_mytop);
  IODeleteMapping(&io_axi_dma);

  misc_deregister( &mydev );

  //AM - освобождаем каналы TX и RX
  if(tx_chan)
    {dma_release_channel(tx_chan);
     tx_chan=NULL;
    }

  if(rx_chan)
    {dma_release_channel(rx_chan);
     rx_chan=NULL;
    }

  return OK;
}



//===Соответствие драйвера узлу в DeviceTree

static const struct of_device_id dmadriv_of_match[] = {
  { .compatible = "ircos,dmadriv", },
  {},  //end of list
};
MODULE_DEVICE_TABLE(of, dmadriv_of_match);

static struct platform_driver dmadriv_of_driver = {
  .probe      = dmadriv_of_probe,
  .remove     = dmadriv_of_remove,
  .driver = {
    .name = "dmadriv",
    .owner = THIS_MODULE,
    .of_match_table = dmadriv_of_match,
  },
};

module_platform_driver(dmadriv_of_driver);

MODULE_AUTHOR( "ivz" );
MODULE_DESCRIPTION("AXI DMA MM2S and S2MM driver");
MODULE_LICENSE("GPL v2");
//MODULE_ALIAS("ircos:dmadriv");

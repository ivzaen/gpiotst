//Определения, общие для драйвера и либы.
//Включается в драйвер и либу.

////Функция вывода в ядре и app разная
//#ifdef __KERNEL__ 
//#define prn printk
//#else
//#define prn printf
//#endif

#define MJR MISC_MAJOR //misc_register регистрирует все с одним major=10.
//Major обычно уникален для драйвера. Пара major,minor должна быть уникальна.
//MNR не используется.
#define DEVICENAME "dmadriv"  //Имя устройства для struct miscdevice и misc_register.
#define DEVICEPATH "/dev/dmadriv"  //Имя special file для открытия в приложении

//Буфер для работы с dma S2MM (потом и MM2S) из user app.
struct SMyDmaMemory{
  int fMM2S;  //Память какой операции выделяется, 1=MM2S, 0=S2MM.
  //Буфер дескрипторов
  struct DMADESC *descbuf;
  size_t desc_npage;

  //Буфер данных
  uint32_t *destbuf; //??AM - проверить
  size_t dest_npage;
};
 
//Касаемо буферов.
#define PATCNT 0x0000FFFF //Маска для поля счетчика
#define PATDESC 0xAABB0000 //Заполнение src
#define PATDEST 0xCCDD0000 //Заполнение dst

//_IOR делает чтение устройства (copy_to_user), _IOW запись, _IOWR двунаправленную передачу.
//See G:\rep\linux-xlnx\Documentation\ioctl\ioctl-number.txt
//При успехе IOCTL могут возвращать OK=0 или положительное значение.
//При ошибке наши IOCTL могут возвращать коды breakpt, положительные.
//  Отрицательное значение при возврате преобразуется в -1, а само значение пойдет в errno (ldd3 p.157).
//prn в IOCTL убираем.

#define IOCTL_MEM_SUBMIT       _IOW(MJR,0, struct MyMemSubmit*)  //Регистрация памяти для dma
#define IOCTL_MEM_RELEASE      _IO(MJR,1)  //Освобождение памяти dma
#define IOCTL_MEM_GETSWD       _IOR(MJR,2, struct DmaDescSw*) //Получить прогр. дескрипторы из ядра. Количество элементов в массиве равно ms.dest_npage.

//Во всех этих стоит _IO, а не _IOR, т.к. значение возвращается в коде возврата, а не в параметре.
#define IOCTL_MM2S_IRQGETCNT  _IO(MJR,105)  //Вернуть в коде возврата количество прерываний, произошедших с прошлого вызова или с начала работы.
#define IOCTL_S2MM_IRQGETCNT  _IO(MJR,106)  //Вернуть в коде возврата количество прерываний, произошедших с прошлого вызова или с начала работы.
#define IOCTL_MM2S_IRQWAIT  _IOW(MJR,107, ulong)  //Ждать прерывание от MM2S заданное количество мксек. Вернуть оставшееся время ожидания в мксек, или 0, если наступил таймаут.
#define IOCTL_S2MM_IRQWAIT  _IOW(MJR,108, ulong)  //Ждать прерывание от S2MM заданное количество мксек. Вернуть оставшееся время ожидания в мксек, или 0, если наступил таймаут.
               
#define IOCTL_GET_BUILDINFO  _IOWR(MJR,120, struct SBUILDINFO*)  //Ждать прерывание от S2MM заданное количество мксек. Вернуть оставшееся время ожидания в мксек, или 0, если наступил таймаут.

//===Аппаратные структуры

//Регистры AXI DMA - Single channel Scatter-Gather Mode.
struct DMAREGS_SG_MODE {
  uint32_t MM2S_DMACR;         //0x00
  uint32_t MM2S_DMASR;         //0x04
  uint64_t MM2S_CURDESC;       //0x08, 0x0C
  uint64_t MM2S_TAILDESC;      //0x10, 0x14
  uint32_t rsrv2[5];           //0x18, 0x1C, 0x20, 0x24, 0x28
  uint32_t SG_CTL;             //0x2C
  uint32_t S2MM_DMACR;         //0x30
  uint32_t S2MM_DMASR;         //0x34
  uint64_t S2MM_CURDESC;       //0x38, 0x3C
  uint32_t S2MM_TAILDESC;      //0x40, 0x44
};

//Аппаратный дескриптор DMA S2MM, MM2S - Single channel Scatter-Gather Mode.
struct DMADESC { //size= 52 bytes; Дескрипторы должны быть выровнены по 64-байтной границе, биты адреса B5:0=0.
  uint64_t NXTDESC;  //0x00, 0x04 Next Descriptor phys. addr.
  uint64_t BUF_ADR;  //0x08, 0x0C Buffer phys. addr., must be data width aligned.
  uint32_t rsrv2[2]; //0x10, 0x14
  uint32_t CONTROL;  //0x18 set length of data, bytes, маска bmLENGTH; флаги bmSOF, bmEOF.
  uint32_t STATUS;   //0x1C transferred length, в байтах, маска bmLENGTH, bmERRS, bmCMPLT.
  uint32_t APP[5];   //0x20-0x30 порт M_AXI_SG не читает эти слова, если они не используются.
  uint32_t dummy[3]; //Добить размер до 64 байт, чтобы дескрипторы, идущие подряд, выравнивались правильно.
};

//MM2S_DMASR S2MM_DMASR layout
//
//Проблема в том, что сразу после bmRS=1 Halted пропадает, но Idle не появляется, и в S2MM, и в MM2S.
//Поэтому хотя движок ничего не передает, состояние показывает, что идет передача.
//Значит комбинация Halted=Idle=0 не говорит нам ничего о состоянии: DMA либо передает данные, либо ждет настройки регистров после включения.
enum DMASR {
  Halted=B0,
  Idle=B1,
  SGIncl=B3,
  ErrBits=(B4|B5|B6 |B8|B9 |B10),
  IrqBits=(B12|B13|B14)
};

//Эти биты идут в CONTROL field для MM2S, в STATUS field для S2MM
#define bmEOF B26  //End of frame for MM2S and S2MM (TXEOF or RXEOF)
#define bmSOF B27  //Start of frame for MM2S S2MM (TXSOF or RXSOF)

//STATUS field для MM2S и S2MM
#define bmCMPLT B31  //Descriptor completed
#define bmERRS  (B30|B29|B28) //Биты ошибок (OR)
#define bmLENGTH (0x7FFFFF) //B22:0 поле длины

//=== MYTOP Наша корка

//Регистры mysamp. Каждый модуль отсчета выборки содержит одинаковый блок регистров.
//!Размер структуры должен совпадать с C_NREGSMYSAMP*4 из файла mytop.v
struct MYSAMP_REGS {

  uint32_t control;       //R0: control. Общее управление mysamp
#define b_ms_SoftReset B0    //  w  1=Сбрасывает модуль отсчета выборки и значения всех регистров в исходное сост. (reg) или в 0 - wire.
#define b_ms_StartStrobe B1  //  w  1=Запуск серии пакетов axi dma IN. Сбрасывать не надо, формируется строб.
#define b_ms_StopStrobe B2   //  w  1=Останов серии пакетов axi dma IN. Сбрасывать не надо, формируется строб.
#define b_ms_fMypktReady B3  //r    1= все операции на mypkt закончены.
#define b_ms_fMyhdrReady B4  //r    1=Idle (fifod & fifop empty, автомат остановлен), 0=Busy (есть данные в fifo или автомат работает).
#define b_ms_fNeprRezh B5    //r w  1=Непрерывный режим отсчета выборки, 0=блочный
#define b_ms_fFifodEmpty B6  //r    1=fifod empty
#define b_ms_fFifopEmpty B7  //r    1=fifop empty                                                                                      
#define b_ms_fTestCounter B8 //r w  1= на данных mysamp идет счетчик, 0= данные AD_DATA.
#define b_ms_PostDspReady B9 //r    сигнал PostDspReady от модуля PostDSP.
#define b_ms_PostDspStop B10 //r    сигнал PostDspStop на модуль PostDsp.
#define b_ms_D_TVALID B11    //r    сигнал разрешения записи в fifod
#define b_ms_D_TREADY B12    //r    сигнал готовности к записи от fifod (!FF)
#define b_ms_AD_TVALID B13   //r    сигнал валидности данных от АЦП к PostDsp или mysamp
#define b_ms_AD_TREADY B14   //r    сигнал готовности к приему данных от mysamp

  uint32_t cntin;         //R1 r cntin. Текущий счетчик слов в одном пакете выборки. Синхр. по ADCLK. Читать, только когда есть fReady.
  uint32_t cntin_lim;     //R2 r w cntin_lim. Задает длину одного пакета в выборке.
  uint32_t cntpacket;     //R3 r cntpacket. Текущий счетчик пакетов в выборке. Синхр. по ADCLK. Читать, только когда есть fReady.
  uint32_t cntpacket_lim; //R4 r w  cntpacket_lim. Задает количество пакетов в блочной выборке.

  uint32_t fErrBits;      //R5 fErrBits Биты ошибок mysamp. Появление любого бита =1 означает ошибку. Ошибки снимаются по SoftReset.
//B7:0 Ошибки mypkt:
#define b_ms_err_fifop_ff B0     //  B0 r    err_fifop_ff, 1=Переполнение fifop.
#define b_ms_err_mypkt_state B1  //  B1 r    1=Ошибка перехода по состояниям.
//B15:8 r Ошибки myhdr:
#define b_ms_err_fifod_underf B8 //  B8 r    err_fifod_underf, 1=Неожиданное опустошение fifod. В ней должно быть гарантированное число слов PQ_PKTLEN.
#define b_ms_err_myhdr_state  B9 //  B9 r    1=Ошибка перехода по состояниям.

  uint32_t ur6, ur7;  //пока свободны.
};

enum EMYSAMPS {MYSAMPTST, NMYSAMP};

//Регистры кольцевого буфера в ОЗУ.
//Размер структуры должен быть C_RINGBUF_REGSIZE/8, ringbuf.v
struct RINGBUF_REGS {
  //R0 control
  //  B0 rw  SoftReset. Запись 1 сбрасывает модуль. 0 следом за 1 писать не нужно. Читается 1, пока генерируется сброс (в т.ч. внешний), затем 0. Сброс длинный, надо ждать.
  //  B1 rw  fWriteDisable. 1=Запретить работу автомата записи для отладки.
  //  B2 rw  fReadDisable. 1=Запретить работу автомата чтения для отладки.
  //  B3 rw  rClearBuf. Запись: 1=Запрос на очистку буфера без остановки потока данных. Чтение: 1= идет выполнение очистки. 0=выполнено.
  uint32_t control;

#define b_rb_softReset B0
#define b_rb_fWriteDisable B1
#define b_rb_fReadDisable B2
#define b_rb_rClearBuf B3

  //R1 r  status
  //  B3:0 r  statew  Состояние автомата записи
  //  B7:4 r  stater  Состояние автомата записи
  //  B11:8 r RLState Состояние модуля hdrchk в канале чтения.
  //  B15:12  WLState Состояние модуля hdrchk в канале записи.
  //  B16 r  F_EF  Empty Flag. Буфер пуст.
  //  B17 r  F_AF  Almost Full. Буфер почти полон, по смыслу - Full Flag. Если флаг равен 1, буфер перестает принимать пакеты на запись до освобождения места.
  //  B18 r  RLReady. Автомат длины чтения свободен.
  //  B19 r  RLGotLen. Автомат длины чтения получил длины пакета.
  //  B20 r  WLReady. Автомат длины записи свободен.
  //  B21 r  WLGotLen. Автомат длины записи получил длины пакета.
  uint32_t status;

#define b_rb_F_EF B16
#define b_rb_F_AF B17
#define b_rb_RLReady B18
#define b_rb_RLGotLen B19
#define b_rb_WLReady B20
#define b_rb_WLGotLen B21

  //R2 rw  error
  // Любая запись в регистр сбрасывает биты ошибок от нашего модуля. Ошибки от dm сбрасываются следующей командой dm.
  //  B31:24 r WLErr  Ошибки модуля hdrchk в канале записи
  //  B23:16 r RLErr  Ошибки модуля hdrchk в канале чтения
  //  B15:4 резерв
  //  B3 r  ErrWInvPktlen  Длина пакета на запись от datamover не равна длине пакета по его заголовкам.
  //  B2 r  ErrRXfer2  Принята ошибка от datamover во время второй операции чтения.
  //  B1 r  ErrRXfer1  Принята ошибка от datamover во время первой операции чтения.
  //  B0 r  ErrWXfer  Принята ошибка от datamover во время записи пакета в буфер.
  uint32_t error;

  //R3 r  WADDR
  uint32_t WADDR;

  //R4 r  RADDR
  uint32_t RADDR;

  //R5 r  wstsCount  Счетчик выполненных команд datamover S2MM
  uint32_t wstsCount;

  //R6 r  rstsCount  Счетчик выполненных команд datamover MM2S
  uint32_t rstsCount;

  //R7 r  wstsTDATA
  uint32_t wstsTDATA;

  //R8 r  rstsTDATA.
  uint32_t rstsTDATA;

  //R9 r  RLSEHdrLen - Длины заголовков SHdrLen и EHdrLen пакета в канале чтения.
  //  B31:16 RLSHdrLen
  //  B15:0  RLEHdrLen
  uint32_t RLSEHdrLen;

  //R10 r  RLDataLen - Поле DataLen пакета в канале чтения.
  uint32_t RLDataLen;

  //R11 r  RLPktLen - Посчитанная длина пакета в канале чтения, байт.
  uint32_t RLPktLen;

  //R12 r  WLSEHdrLen - Длины заголовков SHdrLen и EHdrLen пакета в канале записи.
  //  B31:16 WLSHdrLen
  //  B15:0  WLEHdrLen
  uint32_t WLSEHdrLen;

  //R13 r  WLDataLen - Поле DataLen пакета в канале записи.
  uint32_t WLDataLen;

  //R14 r  RLPktLen - Посчитанная длина пакета в канале записи, байт.
  uint32_t WLPktLen;

  //R15 r  buildReg hex: VVRRBBBB  VV=version, RR=release, BBBB=build number (sequential). Значения двоично-десятичные.
  uint32_t buildReg;

  //R16 r  cDdrBase - Начальный адрес буфера в ОЗУ (адрес байта), константа.
  uint32_t cDdrBase;

  //R17 r  cDdrSize - Размер буфера в ОЗУ, байт, константа.
  uint32_t cDdrSize;

  //R18 r  cDdrMaxPktlen - Пакет, идущий в буфер, должен иметь длину меньше этой величины, в байтах; константа.
  uint32_t cDdrMaxPktlen;

  //R19 r  ddrFree - Количество свободных байт в буфере. Меняется от (cDdrSize-cDdrMaxPktlen) до 0. Если ddrFree<cDdrMaxPktlen то это означает F_AF, буфер не принимает пакеты на запись.
  uint32_t ddrFree;

  uint32_t R20, R21, R22, R23;  //Пока свободны.
};

//Регистры mytop
#pragma pack(push,4)
struct MYTOP_REGS {
  uint32_t control;       //R0: control. Общее управление mytop.
#define b_mt_SoftReset B0                     //r w  1=Сбрасывает модуль mytop и значения всех регистров в исходное сост. (reg в mytop) или в 0 (wire в mytop = reg в axi_lite_my.RW).
#define b_mt_select_adclk_ad9361_n_wizard B1  //r w  1=ADCLK подключен на WIZCLK, 0=на axi_ad9361_clk.
#define b_mt_dis_fclk0 B2  //r w 1=отключение тактов FCLK0 в ПЛИС, только для xenon1_0.

  uint32_t fErrBits;      //R1: r fErrBits Биты ошибок mytop. Появление любого бита в слове означает ошибку. Ошибки снимаются по soft reset. При появлении ошибки выдать значение fErrBits в лог и дать SoftReset.
  
  uint32_t fclk0_cnt;  //R2 r w fclk0_cnt r:Счетчик для частоты FCLK0. w:Сброс всех счетчиков по любой записи.
  uint32_t wizclk_cnt; //R3 wizclk_cnt Счетчик для частоты WIZCLK. Только чтение.
  uint32_t fclk1_cnt;  //R4 fclk1_cnt Счетчик для частоты FCLK1. Только чтение.
  uint32_t adclk_cnt;  //R5 adclk_cnt Счетчик для частоты ADCLK. Только чтение.

  uint32_t ad9361_ctl_st; //R6: ad9361_ctl_st AD9361 control and status pins
#define b_mt_adi_reset B0    //r w  Сигнал на ножке инвертируется, чтобы 0 при сбросе регистра дал N=H на ножке.
#define b_mt_enable_muxed B1 //r w  r=enable_muxed, w=enable_soft
#define b_mt_txnrx_muxed B2  //r w  r=txnrx_muxed, w=txnrx_soft

  uint32_t FPGADateTime; //R7: BULDREGLO, Дата билда, только чтение
  //Чтение: DDMMYYYY, двоично-десятичный код.
  uint32_t FPGAVersion; //R8: BULDREGHI, номер версии, релиза, билда. Только чтение.
  //Чтение: VVRRBBBB  VV=version, RR=release, BBBB=build number (sequential). Значения двоичные.

  uint32_t CombTValid;  //R9: r CombTValid биты TVALID разных портов
  uint32_t CombTReady;  //R10: r CombTReady биты TREADY разных портов
  //см. CombBoth ниже.

  //R11 r w dac_level Установка уровня каналов ЦАП i,q для передачи на шину AXI Stream.
  //  B15:0 r w  dac_data_i0
  //  B31:0 r w  dac_data_q0
  uint32_t dac_data;

  //R12: Идентификационная информация ПЛИС: исполнение платы и  МПО
  //чтение:RR(резерв) RR(резерв) CC(исполнение платы) FF(исполнение МПО)
  uint32_t FPGAIdentInfo;  //R12: r FPGAIdentInfo

  uint32_t softSpi;  //R13 rw softSpi  AM - регистр софтового SPI
  uint32_t PSM2Ctrl; //R14 rw PSM2Ctrl AM - управление ПСМ2. Уточнить
  uint32_t MSP430Ctrl; //R15 w LOP - управление MSP430

  //R16 и далее 1 блок по C_NREGSMYSAMP регистров для модулей отсчета выборки.
  //Регистры mysamptst, только в tstviva1. В xenon1_0 и Vivado_BFPS_AD9361 нет.
  struct MYSAMP_REGS msr[NMYSAMP];  //!Смещение этого поля в байтах от начала структуры должно быть равно MS0_RBASE/8 из файла mytop.v

  uint32_t ref_sel;  // R24 w LOP - регистр выбора источника тактового сигнала для трансивера:0 - внутренний 12.8 МГц, 1 - с передней панели, 3 - с кросс-платы
  uint32_t mask; // R25 w LOP - регистр записи маски
  uint32_t spi_sel; // R26 w LOP - регистр записи номера SPI
  uint32_t u27_u89[63];  //Регистры 27..89 свободны.

  struct RINGBUF_REGS rb;  //Регистры ringbuf. Начало регистров C_RINGBUF_RBASE, количество C_RINGBUF_REGSIZE - в mytop.v.

  uint32_t mm2s_irq_cnt;  //R114: r mm2s_irq_cnt Счетчик прерываний AXI DMA MM2S
  uint32_t s2mm_irq_cnt;  //R115: r s2mm_irq_cnt Счетчик прерываний AXI DMA S2MM
  uint32_t irq_f2p;  //R116: r B15:0 Значение шины IRQ_F2P. Раскладка битов mytop.v "assign IRQ_F2P"
  uint32_t CombBoth;  //R117: r CombBoth - Биты TValid & ~TReady разных портов. 1= задержка передачи по неготовности приемника.

  //Далее идут свободные регистры, поля в данной структуре выделяются по мере необходимости.
  //Последний регистр R127. В mytop.v создается 2^C_NADBITS 32-битных регистров.
};
#pragma pack(pop)

#define PAGESIZEB 4096 //Страница в линуксе, также страница данных.
#define PAGESIZEL (PAGESIZEB/4)  //Длина страницы в ulong словах. //??AM - проверить, мб (PAGESIZEB/sizeof(ulong)) ?
#define DMADESCSIZE 64 //Размер DmaDescSw в байтах
#define NDESCPAGE  (PAGESIZEB/DMADESCSIZE) //Должно нацело делиться
#define DESCMASK_PHYADDR 4095  //Маска физ. адреса дескриптора, выделяющая смещение дескриптора в байтах от начала страницы памяти в буфере descbuf.

//Данные о дескрипторе, запоминаемые в драйвере.
struct DmaDescSw {
  struct DMADESC *kpDesc;  //Указатель ядра для доступа к дескриптору из драйвера. kmap-ed.
  uintptr_t phyDesc; //физ. адрес начала дескриптора
  uintptr_t phyDest; //физ. адрес буфера данных
};

//Состояние axi dma, пока только регистры. Для IOCTL_DMA_GET_STATUS
struct DmaStatus {
  uint32_t S2MM_DMASR;
  uint32_t MM2S_DMASR;
  uint64_t S2MM_CURDESC;
  uint64_t S2MM_TAILDESC;
};

//Стандартный заголовок пакета данных
//#pragma pack(push) 
//#pragma pack(1)    //Не требуется. Размер в любом случае равен 48 байт.
struct SPktHdr {
  uint16_t SHdrLen;  //Длина стандартного заголовка, байт. В данной версии 48.
  uint16_t EHdrLen;  //Длина расширенного заголовка, байт
  uint32_t DataLen;  //Длина полезных данных пакета, байт
  uint32_t Flags;  //Битовые флаги, раскладка ниже.
  uint32_t StreamId;  //Идентификатор потока пакетов
  uint16_t CoreType;  //Тип источника данных, создавшего пакет
  uint16_t CoreNum;  //Номер источника данных, создавшего пакет
  uint16_t PktType;  //Тип пакета
  uint16_t Tag;  //Тэг команды, в ответ на которую создан пакет
  uint32_t Time_nsec;  //Метка времени пакета, наносекунды
  uint32_t TimeSecLo;  //Метка времени пакета, секунды, биты 31:0
  uint16_t TimeSecHi;  //Метка времени пакета, секунды, биты 47:32
  uint16_t TimeFlags;  //Битовые флаги метки времени
  uint32_t PktNum;  //Счетчик пакетов от одного источника (CoreType)
  uint32_t KeyPartLo;  //Поле ключа поиска в буфере предвыборки, биты 31:0
  uint32_t KeyPartHi;  //Поле ключа поиска, биты 63:32
};
//#pragma pack(pop)

//Поле Flags
#define SHDRF_FSTART     B0  //1=Первый пакет в серии, модуль начинает передачу. 0=пакет в середине серии
#define SHDRF_FSTOP      B1  //1=Последний пакет в серии, модуль останавливается. 0=пакет в середине серии
#define SHDRF_FERRGAP    B2  //1=Нарушена непрерывность передаваемых данных, в т.ч., из-за переполнения буферов
#define SHDRF_FMAIN      B3  //1=Пакет основного потока (Main), 0=пакет предварительного потока (Preview)
#define SHDRF_FERRTOUT   B4  //1=Ошибка: таймаут в аппаратуре. Данные пакета могут быть некорректными
#define SHDRF_FOVR_LMT   B5  // переполнение во входном усилителе
#define SHDRF_FOVR_LADC  B6  // переполнение большое в АЦП
#define SHDRF_FOVR_SADC  B7  // переполнение малое в АЦП
#define SHDRF_FERRGAPLEV B8  //1=Нарушена непрерывность передаваемых данных из-за снижения уровня сигнала ниже порога

#define SHDRF_ERRMASK (B4|B2)  //Маска битов ошибок

//Поле StreamId
#define SHDRSTREAMID_USB  B0  //Назначение потока - USB, для Ксенон.
#define SHDRSTREAMID_SD   B1  //Назначение потока - SD, для Ксенон.
#define SHDRSTREAMID_ARM  B2  //Назначение потока - ARM
#define SHDRSTREAMID_SD_2 B3  //Назначение потока - SD_2, для Ксенон/Неон-18.
#define SHDRSTREAMID_LAN_1 B4  //Назначение потока - LAN_1
#define SHDRSTREAMID_LAN_2 B5  //Назначение потока - LAN_2
#define SHDRSTREAMID_DAC B6  //Назначение потока - DAC

//Поле CoreType
#define C_CORETYPE_PLIS            0x0000  //ПЛИС
#define C_CORETYPE_CYPRESS_SD_SSD  0x0008  //Контроллер Cypress/SD-карта/SSD-диск
#define C_CORETYPE_ARM             0x0010  //Процессор ARM
#define C_CORETYPE_TST             0x0020  //Тестовый источник данных

//Поле PktType
#define C_PKTTYPE_TST 0x10  //Пакет с тестовым счетчиком

//SLCRs: FPGA0_CLK_CTRL, FPGA0_THR_CTRL; Clock output control, Clock throttle.
#define b_fclk_srcsel_iopll  0
#define b_fclk_srcsel_armpll  B5
#define b_fclk_srcsel_ddrpll  (B5|B4)
#define b_fclk_div0_shift 8  //shift to get divisor0, ширина поля 6 бит
#define b_fclk_div1_shift 20 //shift to get divisor1, ширина поля 6 бит

//=== PacketFilter - модуль фильтрации пакетов

//В модуле может быть до PKTF_NFILTERMAX независимых фильтров, каждый задает свое условие фильтрации.
//Реальное количество фильтров читается из регистра и переменную nFilters.
#define PKTF_NFILTERMAX 16  //Максимальное количество фильтров в одном модуле.

//Регистры компаратора PacketFilter
struct PKTF_COMPARATOR_REGS {
  uint32_t STREAM_ID;  //adr=0  Значение поля StreamId.
  uint32_t SRC_TYPE;   //adr=4  B31..16 -значение поля CoreNum. B15..0 -значение поля CoreType
  uint32_t PACKET_TYPE;  //adr=8  Значение поля PktType
  uint32_t STATUS;  //adr=0xC, только для компаратора0, для остальных пусто.
  uint32_t STREAM_ID_DC;  //adr=0x10  Маска игнорируемых битов в поле StreamId.
  uint32_t SRC_TYPE_DC;  //adr=0x14  B31..16 маска игнорир. битов для поля CoreNum, Биты B15..0 - маска битов для поля CoreType.
  uint32_t PACKET_TYPE_DC;  //adr=0x18  Маска игнорир. битов для поля PktType.
  uint32_t CONTROL;  //adr=0x1C  B0=reset; только для компаратора0, для остальных пусто.
};

#define MAXPATH 256  //Макс. длина имени файла с путем к нему.

//Режимы энергопотребления аппаратных модулей.
//Используются в функциях SetPowerMode, GetPowerMode.
//Разрешается повторный вызов SetPowerMode.
enum POWERMODES {
  PW_NOINIT,   //Модуль не инициализирован, режим потребления неизвестен.
  PW_ON,       //Модуль готов к выполнению любых команд без задержки включения.
  PW_STANDBY,  //Модуль может быть переведен в состояние PW_ON за относительно короткое время. Некоторые функции модуля могут не выполняться.
  PW_OFF       //Модуль выключен, выполнение всех или почти всех функций невозможно. Перевод в состояние PW_ON или PW_STANDBY может быть длительным.
};

//=== Регистры контроллера PS SPI0

/* SPI Status Register  drivers/spips/src/xspips_hw.h
*
* This register holds the interrupt status flags for an SPI device. Some
* of the flags are level triggered, which means that they are set as long
* as the interrupt condition exists. Other flags are edge triggered,
* which means they are set once the interrupt condition occurs and remain
* set until they are cleared by software. The interrupts are cleared by
* writing a '1' to the interrupt bit position in the Status Register.
* Read/Write.
*
* <b>SPI Interrupt Enable Register</b>
*
* This register is used to enable chosen interrupts for an SPI device.
* Writing a '1' to a bit in this register sets the corresponding bit in the
* SPI Interrupt Mask register.  Write only.
*
* <b>SPI Interrupt Disable Register </b>
*
* This register is used to disable chosen interrupts for an SPI device.
* Writing a '1' to a bit in this register clears the corresponding bit in the
* SPI Interrupt Mask register. Write only.
*
* <b>SPI Interrupt Mask Register</b>
*
* This register shows the enabled/disabled interrupts of an SPI device.
* Read only.
*
* All four registers have the same bit definitions. They are only defined once
* for each of the Interrupt Enable Register, Interrupt Disable Register,
* Interrupt Mask Register, and Channel Interrupt Status Register
*/
struct PSSPI_REGS {  //p.1753 TRM ug585.
  uint32_t config;  //p.1753 TRM ug585.
  uint32_t intr_status;
  uint32_t intrpt_en;
  uint32_t intrpt_dis;
  uint32_t intrpt_mask;
  uint32_t en;
  uint32_t delay;
  uint32_t tx_data;
  uint32_t rx_data;
  uint32_t slave_idle_count;
  uint32_t tx_thres;
  uint32_t rx_thres;  //offset 0x2C
  uint32_t rsrv[51];  //offsets 0x30-0xF8 reserved
  uint32_t mod_id;  //offset 0xFC, reads 0x90106
};

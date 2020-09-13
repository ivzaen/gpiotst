#ifdef IO_STRUCTS  //Надо ли создавать definition для адресов?
  #define ifextern  //пусто
  #define ifinit(arg...) arg  //Макро с перем. числом параметров. Взято по http://stackoverflow.com/questions/1433204/how-do-i-use-extern-to-share-variables-between-source-files-in-c
  //Переделано под gcc в ядре (C89) по https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
#else  //Только declaration
  #define ifextern extern
  #define ifinit(arg...)  //пусто
#endif

//Структура для отображения диапазона i/o адресов, одинаковая для драйвера и либы.
struct IOMAPPING {
  ulong physAddr; //Физ. адрес начала. Должен быть кратен PAGESIZEB.
  size_t length; //Длина отображения (в байтах!)
  int fMapOk; //Отображение сделано успешно
  volatile ulong *access; //указатель для доступа к памяти (в ulong, размер менять нельзя, неявно использ. при арифметике!)
};

//Структуры, задающие карту адресов i/o. Только declarations во всех заголовках, и только для одного модуля - definitions.
//Физ. адрес начала области должен быть кратен 0x1000 (PAGESIZEB).

ifextern struct IOMAPPING io_gpio_leds ifinit(={0x41220000, 0xFFFF});  //gpio leds




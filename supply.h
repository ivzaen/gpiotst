//Заголовок используется драйвером и либой ЭАПИ.
//Включает объявления, общие для драйвера и либы.

#ifndef SUPPLY_H
#define SUPPLY_H
#ifdef __cplusplus
extern "C" {
#endif

//Функции вывода определяются по-разному. Для ядра - макро, для эапи - честные функции.
#ifdef __KERNEL__ 
  #define prn pr_info
  #define prnerr pr_info
#else
  int prn(const char *fmt,...);
  int prnerr(const char *fmt,...);
  enum ATTRS {AT_NORMAL, AT_ERROR, AT_GREEN, AT_LIGHTGREEN, AT_BLUE, AT_NORMONBLUE};
void prnAttr(enum ATTRS attr);

#endif

// переставляет count байт в *buf из LeSBian в big-endian и обратно
void ReverseBytes(void *buf, unsigned count);
// то же макро для простого вызова
#define Reverse(u) ReverseBytes(&(u), sizeof(u))

//Вывод дампа в строку или в prn
int sdump(char* str, const void *buf, int len, int startaddr);

//Дамп в строку c ascii-символами справа.
//Вызывать с новой строки.
void sDumpWithAscii(char* str, void *buf, int len, ulong saddr);

//Дамп в prn
void dump(const void *buf, int len, int startaddr);

void DumpWithAscii(void *buf, int len, ulong saddr);

//Напечатать "mask", если ХОТЬ ОДИН бит из mask ==1, иначе ничего. //was: "~mask".
#define printMaskAny(var, mask)  if((var&(mask))!=0) prn(#mask " "); //else prn("~" #mask " ");

//Напечатать "mask", если ВСЕ биты из mask ==1, иначе ничего. //was: "~mask".
#define printMaskAll(var, mask)  if((var&(mask))==mask) prn(#mask " "); //else prn("~" #mask " ");

//Напечатать номера битов ulong, равные 1, добавить строчку eol в конец.
void printUint32Bits(const char *name, uint32_t var, const char *eol);
              
//Печать названия переменной и ее значения по формату formspec
//Если в конце надо \r\n то указываем их в formspec
#define printfvar(var, formspec) prn(#var "=" formspec , var )

//=== AXI dma ===

void prnDmasr(const char *str, ulong dmasr);

//Ссылки на ф-и, которые нужны для диагностики во внешних (не наших) модулях.
void breakpt(unsigned long code);

#ifndef __KERNEL__ 

//was buildinfo

#endif

#ifdef __cplusplus
}
#endif
#endif

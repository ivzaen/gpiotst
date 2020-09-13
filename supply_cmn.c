//Фрагмент include, включается модулями supply_lkm и supply_app драйвера и либы ЭАПИ.
//Включает определения, нужные одновременно драйверу и либе.

// переставляет count байт в *buf из LeSBian в big-endian и обратно
void ReverseBytes(void *buf, unsigned count){
  unsigned char *b2= (unsigned char*)buf;
  unsigned x;
  unsigned char c;
  for(x=0; x<count/2; x++){
    c=b2[x];
    b2[x]= b2[count-x-1];
    b2[count-x-1]=c;
  }
}

//возвр. сколько символов записано, не считая последнего 0 байта, (как sprintf)
int sdump(char* str, const void *buf, int len, int startaddr){
  unsigned char *b= (unsigned char*)buf;
  register int x;
  int pos=0;
  for(x=0; x<len; x++){ 
    if(x%16 ==0){
      if(x) pos+=sprintf(&str[pos],"\r\n");
      if(len>16) pos+=sprintf(&str[pos],"%04X: ", x+startaddr);
    }
    if(x%8==0) pos+=sprintf(&str[pos]," ");
    pos+=sprintf(&str[pos],"%02X ", b[x]);
  }
  pos+=sprintf(&str[pos],"\r\n");
  return pos; 
}

//Дамп в строку c ascii-символами справа.
//Вызывать с новой строки.
void sDumpWithAscii(char* str, void *buf, int len, ulong saddr){
  int clen=0;   //Текущая длина, увеличиваем только по окончании строки.
  char* s= str; //Куда печатаем.
  int ip=0; //смещение в строке s
  uchar* cbuf=(uchar*)buf; //Дамп этого буфера

  while(clen<len){
    int x;  //0..15, индекс байта в строке
    ip+=sprintf(s+ip, "%08lu: ", saddr+(ulong)clen); //Печатаем адрес
    //Выводим hex-числа, 16шт или частью пробелы в последней строке.
    for(x=0; x<16; x++){ 
      if(clen+x>=len){ //Последняя строчка, в конце заменяем hex-числа пробелами.
        ip+=sprintf(s+ip, "   ");
      }else{
        ip+=sprintf(s+ip, "%02X ", cbuf[clen+x]);
      }
      //Пробелы после 8 чисел всегда.
      if(x==7) ip+=sprintf(s+ip, "  "); //Доп. пробелы после 8 hex-чисел.
    }
    ip+=sprintf(s+ip, "  "); //Между hex и ascii.
    //Выводим ascii, 16шт или частью пробелы в послед. строке.
    for(x=0; x<16; x++){ 
      if(clen+x>=len) break; //Последняя строчка, в конце выход.
      else{
        if(cbuf[clen+x]<=0x1F) ip+=sprintf(s+ip, "."); //Control char
        else ip+=sprintf(s+ip, "%c", cbuf[clen+x]); //Норм символ
      }
    }
    //Вывели строку в 16 или меньше. В последней строке индексы выйдут за границы, ничего.
    clen+=16; 
    ip+=sprintf(s+ip, "\r\n"); //в конце строки <CR>
  }
}

void dump(const void *buf, int len, int startaddr){
  char *str;
#ifdef __KERNEL__
  str=(char*)kmalloc(50+len*10, GFP_KERNEL);
#else
  str=(char*)malloc(50+len*10);
#endif
  sdump(str,buf,len,startaddr);
  prn("%s",str);

#ifdef __KERNEL__
  kfree(str);
#else
  free(str);
#endif
}

void DumpWithAscii(void *buf, int len, ulong saddr){
  char *str;

#ifdef __KERNEL__
  str=(char*)kmalloc(50+len*10, GFP_KERNEL);
#else
  str=(char*)malloc(50+len*10);
#endif

  sDumpWithAscii(str,buf,len,saddr);
  prn("%s",str);

#ifdef __KERNEL__
  kfree(str);
#else
  free(str);
#endif
}

void printUint32Bits( const char *name, uint32_t val, const char *eoln ){
  enum states {zero, one1, one2, oneseq} st=zero, newst=zero;
  int i, bit;

  prn("%s=(", name);
  if(val==0){  //Без этого выйдет пустота после =
    prn("0"); 
  }else{ 
    for(i=31; i>=-1; i--){  //Цикл включает нулевой бит
      if(i!=-1) bit= (val>>i)&1; else bit=0;  //Или бит, или 0, если прошли шаг за B0 (для печати конца).
      if(st==zero){        if(bit) newst=one1; else newst=zero; }
      else if(st==one1){   if(bit) newst=one2; else newst=zero; }
      else if(st==one2){   if(bit) newst=oneseq; else newst=zero; }
      else if(st==oneseq){ if(bit) newst=oneseq; else newst=zero; }

      //newst обозначает сост. на текущем бите
      //st обозначает состояние на прошлом бите
      //Если st==zero && newst!=zero то печатаем B(текущ)
      //Иначе  если st==zero && newst==zero то пропуск
      //Иначе  если st!=zero и newst!=zero то пропуск
      //Иначе  если st!=zero и newst==zero то это переход на 0, печатаем прошлый бит с точкой или без:
      //  если st==oneseq то печатаем ..B(прош)
      //  иначе (значит, st!=oneseq && st!=zero) печатаем _B(прош)

      if     (st==zero && newst!=zero){ prn(" B%d", i); }
      else if(st==zero && newst==zero){}
      else if(st!=zero && newst!=zero){}
      else if(st!=zero && st!=one1 && newst==zero){
        if(st==oneseq) prn("-B%d", i+1);
        else prn(" B%d", i+1);
      }
      st=newst;
    }
  }
  prn(")"); prn("%s", eoln);
}

//=== AXI dma ===

void prnDmasr(const char *str, ulong dmasr){
  enum DMASR m= (enum DMASR)dmasr;
  printUint32Bits(str, dmasr, " (");
  printMaskAny(m, IrqBits);
  printMaskAny(m, ErrBits);
  printMaskAny(m, SGIncl);
  printMaskAny(m, Idle);
  printMaskAny(m, Halted);
  prn(")\n");
}

//=== Buildinfo ===
//Определено здесь, т.к. в buildinfo_*.c только автоматически записанный код.




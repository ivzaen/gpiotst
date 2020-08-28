//Самые общие определения.
//Включается и в драйвер, и в либу.

#ifndef MYDEFS_H
#define MYDEFS_H

typedef unsigned char  uchar;
typedef unsigned short  ushort;
typedef unsigned long  ulong;
#ifndef B0
  #define B0 1
  #define B1 2
  #define B2 4
  #define B3 8
  #define B4 0x10
  #define B5 0x20
  #define B6 0x40
  #define B7 0x80
  #define B8  0x100
  #define B9  0x200
  #define B10 0x400
  #define B11 0x800
  #define B12 0x1000
  #define B13 0x2000
  #define B14 0x4000
  #define B15 0x8000
  #define B16 0x10000
  #define B17 0x20000
  #define B18 0x40000
  #define B19 0x80000
  #define B20 0x100000
  #define B21 0x200000
  #define B22 0x400000
  #define B23 0x800000
  #define B24 0x1000000
  #define B25 0x2000000
  #define B26 0x4000000
  #define B27 0x8000000
  #define B28 0x10000000
  #define B29 0x20000000
  #define B30 0x40000000
  #define B31 0x80000000
#endif
                       
#ifndef BYTE0
  #define BYTE0(v) ((unsigned char)((v)&0xFF))
  #define BYTE1(v)  ((unsigned char)((v>>8)&0xFF))
  #define BYTE2(v)  ((unsigned char)((v>>16)&0xFF))
  #define BYTE3(v)  ((unsigned char)((v>>24)&0xFF))
#endif

#define ERR (-1)  //Ошибка, как принято в линуксе.
#define OK 0    //Обычно так.

#endif  //MYDEFS_H

#ifndef __JPEG_H
#define __JPEG_H

#define MAX_C 4 // JPEG max components

struct THuffRec
{
   unsigned char len;
   unsigned char value;
   unsigned short code;
};

struct TFramePar
{
   unsigned type;
   unsigned P;
   unsigned Y;
   unsigned X;
   unsigned Nf;

   unsigned mcu;
   unsigned mcuW;
   unsigned mcuH;

   unsigned n_mcu_X;
   unsigned n_mcu_Y;

   struct Item
   {
      unsigned char C; // component id
      unsigned char H;
      unsigned char V;
      unsigned char Tq;
      unsigned char off;
      unsigned char N;
      unsigned char _reserved[2];
   } item[MAX_C];
};

struct TScanPar
{
   unsigned Ns; // number of components

   unsigned Ss; // start Z-coef
   unsigned Se; // end Z-coef
   unsigned Ah;
   unsigned Al;

   struct Item
   {
      unsigned char Cs; // component id
      unsigned char Td;
      unsigned char Ta;
      unsigned char i; // index in TFramePar::item[]
   } pars[MAX_C];
};

int make_hufftab(unsigned char lens[], THuffRec hufftab[]);


class THuffTab
{
public:
   THuffTab(int _Tc=0, int _Th=0) : nItems(0), hufftab(0),
      Tc(_Tc), Th(_Th) {}

   ~THuffTab()
   {
      delete [] hufftab;
   }

   void Init(int _Tc, int _Th)
   {
      delete [] hufftab;
      hufftab = 0;
      nItems = 0;

      Tc = _Tc;
      Th = _Th;
   }

   void Create( unsigned char[] );

   int Tc, Th;

   int nItems;
   THuffRec* hufftab;

   int maxcode[17];
   int index2[16];
   int mincode[16];
   int index[16];
};

#endif//__JPEG_H


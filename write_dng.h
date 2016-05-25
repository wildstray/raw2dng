#ifndef __WRITE_DNG_H
#define __WRITE_DNG_H

#include "jpeg.h"

class THuffTab2
{
public:
   THuffTab2() : used(0) {}
   THuffTab2(int _Tc, int _Th) { init(_Tc, _Th); }

   void generate_codes();
   void init(int _Tc, int _Th);
   void init(unsigned char bits[], unsigned char code[], int n);
   bool operator == ( THuffTab2 const & a ) const;

   int used;
   int Tc, Th;
   int nCode;
   unsigned char bits[16];
   unsigned char code[256];

   THuffRec hufftab[256];
   mutable unsigned count[257];
};

class TScanData
{
public:
   TScanData(int W, int H);
   TScanData(int W, int H, int Pt, int predictor);
   ~TScanData();

   THuffTab2 tabs[MAX_C];
   TFramePar frame;
   TScanPar scan;
};

typedef unsigned short color_t;
typedef color_t const* color_buf;
typedef color_t const* color_ptr;

// 0 - original function
// 1 - loop optimized
// 2 - x86 assembler function
// 3 - x86 assembler naked __fastcall function

#ifndef GET_LEN_VAR 
#define GET_LEN_VAR 1
#endif

#if GET_LEN_VAR == 3
int __fastcall get_len( int x );
#else
int get_len( int x );
#endif

#endif//__WRITE_DNG_H

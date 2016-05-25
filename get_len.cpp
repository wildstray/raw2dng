#include <stdio.h>
#include <string.h>
#pragma hdrstop

/*
   Coolpix RAW to DNG converter
   by paul69, v0.1, 2006-01-25, <e2500@narod.ru>

   variants of get_len function implementations
*/

#include "write_dng.h"

#if GET_LEN_VAR == 3
   // MSVC++ specific 'naked' __fastcall
   #ifdef _MSC_VER
      // argument in ECX
      __declspec( naked ) int __fastcall get_len( int x )
      {
         __asm mov eax, ecx
         __asm cdq
         __asm xor eax, edx
         __asm sub eax, edx
         __asm jz short m_zero
         __asm bsr eax, eax
         __asm inc eax
      m_zero:
         __asm ret
      }
   #else
      // argument in EAX
      __declspec( naked ) int __fastcall get_len( int )
      {
         __asm cdq
         __asm xor eax, edx
         __asm sub eax, edx
         __asm jz short m_zero
         __asm bsr eax, eax
         __asm inc eax
      m_zero:
         __asm ret
      }
   #endif
#elif GET_LEN_VAR == 2
// no MSVC++ specific
int get_len( int x )
{
   int len = 0;
   __asm mov eax, x
   __asm cdq
   __asm xor eax, edx
   __asm sub eax, edx
   __asm jz short m_zero
   __asm bsr eax, eax
   __asm inc eax
   __asm mov len, eax
m_zero:
   return len;
}
#elif GET_LEN_VAR == 1
// no loop
int get_len( int x )
{
   if( x < 0 )
      x = -x;

   int i = 0;
   if( x >= 0x10000 ) { i = 16; x >>= 16; }
   if( x >= 0x100 ) { i += 8; x >>= 8; }
   if( x >= 0x10 ) { i += 4; x >>= 4; }
   if( x >= 4 ) { i += 2; x >>= 2; }
   if( x >= 2 ) { i += 1; x >>= 1; }
   if( x >= 1 ) { ++i; }
   return i;
}
#elif GET_LEN_VAR == 0
// original code for testing
int get_len( int x )
{
   if( x == 0 )
      return 0;

   if( x < 0 )
      x = -x;

   int m = 1;
   int i=0;
   while( m )
   {
      if( x < m )
         return i;

      m <<= 1;
      ++i;
   }
   return 31;
}
#endif

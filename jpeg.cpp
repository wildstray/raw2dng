#include "jpeg.h"

int make_hufftab(unsigned char lens[], THuffRec hufftab[])
{
   THuffRec* p = hufftab;
   unsigned code = 0;
   for(int i = 0; i < 16; ++i)
   {
      unsigned char size = (unsigned char)(i+1);
      int n = lens[i];
      for( int j = 0; j < n; ++j )
      {
         p->len = size;
         p->code = (unsigned short)code;
         ++code;
         ++p;
      }
      code += code;
   }

   return p - hufftab;
}

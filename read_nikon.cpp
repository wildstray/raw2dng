#include <stdio.h>
#include <string.h>
#pragma hdrstop

/*
   Coolpix RAW to DNG converter
   by paul69, v0.1, 2006-01-02, <e2500@narod.ru>

   Reading NIKON compressed file
*/

#include "tiff.h"
#include "raw2nef.h"
#include "jpeg.h"
#include "file.h"

#define TRACE(x) { ; }

////////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#define check_throw(cond, msg) { if( cond ) throw "(" #cond ")"; }
#else
#define check_throw(cond, msg) {}
#endif

class TBitReaderN
{
public:
   TBitReaderN(FILE* _in) : in(_in), bbuf(0), cnt(0), marker(0) {}

   unsigned bbuf;
   int cnt;
   FILE* in;
   unsigned marker;

   int load_bit();
   int load_bits(int n);

   int is_bit()
   {
      return cnt >= 1 ? 1 : load_bit();
   }

   int is_bits(int n)
   {
      return cnt >= n ? n : load_bits(n);
   }

   int is_ok()
   {
      if( feof(in) ) return 0;
      return 1;
   }

   unsigned get_bit()
   {
      --cnt;
      check_throw(cnt < 0, "error")
      return (bbuf >> cnt)&1;
   }

   unsigned get_bits(int n)
   {
      check_throw(n > 16, "error")
      cnt -= n;
      check_throw(cnt < 0, "error")
      return (bbuf >> cnt)&(bit_mask[n]);
   }

   int get_int(int n)
   {
      check_throw(n > 16, "error")
      check_throw(n == 0, "error")
      check_throw(cnt < n, "error")

      if( bbuf & (1<<(cnt-1)) )
      {
         return int( (bbuf >> (cnt -= n)) & bit_mask[n] );
      }
      //return int( (bbuf >> (cnt -= n)) | ~bit_mask[n] )+1;
      return int( (bbuf >> (cnt -= n)) | (~0 << n) )+1;
   }

   static unsigned bit_mask[17];
};

unsigned TBitReaderN::bit_mask[17] = { 0x0000,
   0x0001, 0x0003, 0x0007, 0x000F,
   0x001F, 0x003F, 0x007F, 0x00FF,
   0x01FF, 0x03FF, 0x07FF, 0x0FFF,
   0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF,
   };

   int TBitReaderN::load_bit()
   {
      if( marker )
         return 0;

      bbuf = fgetc(in);

      if( bbuf == -1 ) // eof
      {
         marker = -1;
         return 0;
      }

      return (cnt = 8);
   }

   int TBitReaderN::load_bits(int n)
   {
      if( marker )
         return 0;

      do
      {
         unsigned b1 = fgetc(in);
         if( b1 == -1 ) // eof
         {
            marker = -1;
            return cnt;
         }

         bbuf = (bbuf << 8) | b1;
         cnt += 8;
      }
      while( cnt < n );

      return cnt;
   }

////////////////////////////////////////////////////////////////////////////////

int nikon_unpack( TBitReaderN& bb, THuffTab const* huff )
{
   if( !bb.is_ok() ) return 0;

   if( !bb.is_bit() ) return 0;

   int code = bb.get_bit();

   int i=0;
   while( code > huff->maxcode[i] )
   {
      ++i;
      check_throw(i >= 16, "too long")
      if( !bb.is_bit() ) return 0;

      code += code + bb.get_bit();
   }

   i = huff->index2[i] + code;

   int t = (unsigned)huff->hufftab[i].value;

   if( t )
   {
      if( t == 16 )
         return 32768; 

      if( bb.is_bits(t) < t )
         return 0;

      return bb.get_int(t);
   }

   return 0;
}
////////////////////////////////////////////////////////////////////////////////

bool nikon_unpack(FILE* in, THuffTab* tab, TFramePar& frame, int bias, color_t picture[] )
{
   TBitReaderN bb(in);

   int W = frame.n_mcu_X;
   int H = frame.n_mcu_Y;
   int dy = frame.n_mcu_X*frame.mcu;

   color_t* line = picture;

   for( int i_line = 0; i_line < 2; ++i_line, line += dy )
   {
      color_t* pix = line;

      pix[0] = bias + nikon_unpack(bb, tab);
      pix[1] = bias + nikon_unpack(bb, tab);
      
      for(int i_mcu=1; i_mcu < W; ++i_mcu)
      {
         pix += 2;
         pix[0] = pix[-2] + nikon_unpack(bb, tab);
         pix[1] = pix[1-2] + nikon_unpack(bb, tab);
      }
   }

   for( int i_line = 2; i_line < H; ++i_line, line += dy )
   {
      color_t* pix = line;

      pix[0] = pix[-2*dy] + nikon_unpack(bb, tab);
      pix[1] = pix[1-2*dy] + nikon_unpack(bb, tab);

      for(int i_mcu=1; i_mcu < W; ++i_mcu)
      {
         pix += 2;
         pix[0] = pix[-2] + nikon_unpack(bb, tab);
         pix[1] = pix[1-2] + nikon_unpack(bb, tab);
      }
   }

   return true;
}

////////////////////////////////////////////////////////////////////////////////

bool safe_read_compressed_nikon(FILE* in, unsigned w, unsigned h, int bias, color_t picture[])
{
   const int n_comp = 2;
   TFramePar frame;

   frame.type = 3;
   frame.P = 16;
   frame.Y = h;
   frame.X = w/n_comp;

   frame.Nf = n_comp;
   frame.mcu = n_comp;
   frame.mcuW = 1;
   frame.mcuH = 1;

   frame.item[0].C = 0;
   frame.item[0].H = 1;
   frame.item[0].V = 1;
   frame.item[0].Tq = 0;
   frame.item[0].off = 0;
   frame.item[0].N = 1;

   frame.item[1].C = 1;
   frame.item[1].H = 1;
   frame.item[1].V = 1;
   frame.item[1].Tq = 0;
   frame.item[1].off = 1;
   frame.item[1].N = 1;

   frame.n_mcu_X = (frame.X + frame.mcuW - 1)/frame.mcuW;
   frame.n_mcu_Y = (frame.Y + frame.mcuH - 1)/frame.mcuH;

   THuffTab tabs[1];

   byte lens[16] = { 0, 1, 5, 1, 1, 1, 1, 1, 1, 2 };
   byte vals[] = { 5, 4, 3, 6, 2, 7, 1, 0, 8, 9, 11, 10, 12, 13 };

   THuffTab* tab = tabs;
   tab->Init(0, 0);
   tab->Create( lens );

   THuffRec* p = tab->hufftab;
   for(int i=0; i<tab->nItems; ++i)
   {
      p->value = vals[i];
      ++p;
   }

   nikon_unpack(in, tab, frame, bias, picture);
   return true;
}

bool read_compressed_nikon(FILE* in, unsigned w, unsigned h, int bias, color_t picture[])
{
   __try
   {
      return safe_read_compressed_nikon( in, w, h, bias, picture );
   }
   __except(1)
   {
      printf("error: except\n");
   }
   return true;
}


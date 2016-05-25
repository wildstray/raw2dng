#include <stdio.h>
#include <string.h>
#pragma hdrstop

/*
   Coolpix RAW to DNG converter
   by paul69, v0.1, 2006-01-02, <e2500@narod.ru>

   Reading DNG file
*/

#include "tiff.h"
#include "raw2nef.h"
#include "jpeg.h"
#include "file.h"

#define TRACE(x) { ; }

void THuffTab::Create(unsigned char lens[])
{
   int i, j;

   nItems = 0;
   for(i=0; i<16; ++i)
      nItems += lens[i];

   hufftab = new THuffRec[nItems];

   make_hufftab(lens, hufftab);

   for(i=j=0; i<16; ++i)
   {
      if( lens[i] == 0 )
      {
         maxcode[i] = -1;
         continue;
      }
      index[i] = j;
      mincode[i] = hufftab[j].code;
      j += lens[i];
      maxcode[i] = hufftab[j-1].code;
      index2[i] = index[i] - mincode[i];
   }
   maxcode[i] = 0x7FFFFFFF;
}

////////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#define check_throw(cond, msg) { if( cond ) throw "(" #cond ")"; }
#else
#define check_throw(cond, msg) {}
#endif

class TBitReader
{
public:
   TBitReader(FILE* _in) : in(_in), bbuf(0), cnt(0), marker(0) {}

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

unsigned TBitReader::bit_mask[17] = { 0x0000,
   0x0001, 0x0003, 0x0007, 0x000F,
   0x001F, 0x003F, 0x007F, 0x00FF,
   0x01FF, 0x03FF, 0x07FF, 0x0FFF,
   0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF,
   };

   int TBitReader::load_bit()
   {
      if( marker )
         return 0;

      bbuf = fgetc(in);

      if( bbuf == -1 ) // eof
      {
         marker = -1;
         return 0;
      }

      if( bbuf == 0xFF )
      {
         unsigned b2 = fgetc(in);

         if( b2 == -1 ) // eof
         {
            marker = -1;
            return 0;
         }

         if( b2 != 0x00 )
         {
            marker = b2;
            return 0;
         }
      }
      return (cnt = 8);
   }

   int TBitReader::load_bits(int n)
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

         if( b1 == 0xFF )
         {
            unsigned b2 = fgetc(in);
            if( b2 == -1 ) // eof
            {
               marker = -1;
               return cnt;
            }
            if( b2 != 0x00 )
            {
               marker = b2;
               return cnt;
            }
         }
         bbuf = (bbuf << 8) | b1;
         cnt += 8;
      }
      while( cnt < n );

      return cnt;
   }

////////////////////////////////////////////////////////////////////////////////
int unpack( TBitReader& bb, THuffTab const* huff, int Pt, color_t pix[],
   int predictor, int dx, int dy, bool bug )
{
   if( !bb.is_ok() ) return -1;

   if( !bb.is_bit() ) return -1;
   int code = bb.get_bit();

   int i=0;
   while( code > huff -> maxcode[i] )
   {
      ++i;
      check_throw(i >= 16, "too long")
      if( !bb.is_bit() ) return -1;
      code += code + bb.get_bit();
   }
   i = huff->index2[i] + code;

   int t = (unsigned)huff->hufftab[i].value;

   if( t )
   {
      if( t == 16 && !bug )
         t = 32768; // Pt = 0
      else
      {
         if( bb.is_bits(t) < t )
            return -1;

         t = bb.get_int(t);
      }
   }

   int Px;
   switch( predictor )
   {
   default:
   case -1: Px = 0x8000 >> Pt; break;
   case 0: Px = 0; break;
   case 1: Px = pix[-dx]; break;
   case 2: Px = pix[-dy]; break;
   case 3: Px = pix[-dx-dy]; break;
   case 4: Px = pix[-dx] + pix[-dy] - pix[-dx-dy]; break;
   case 5: Px = pix[-dx] + ((int)(pix[-dy] - pix[-dx-dy])>>1); break;
   case 6: Px = pix[-dy] + ((int)(pix[-dx] - pix[-dx-dy])>>1); break;
   case 7: Px = (int)(pix[-dx] + pix[-dy])>>1; break;
   }

   pix[0] = (color_t)(Px + t);

   return 0;
}
////////////////////////////////////////////////////////////////////////////////

int lossless_unpack(FILE* in, THuffTab tabs[],
   TFramePar& frame, TScanPar& scan, color_t picture[], bool bug)
{
   int i, k;
   THuffTab* HT[MAX_C];
   int offset[MAX_C];

   int scan_mcu_size = 0;
   for(i=k=0; i<(int)scan.Ns; ++i)
   {
      TScanPar::Item& sc = scan.pars[i];
      TFramePar::Item& fp = frame.item[sc.i];

      int N = fp.N;
      scan_mcu_size += N;

      //int off = fp.off;
      int off = i >= 2 ? frame.n_mcu_X*frame.mcu/2 + i-2 : i;

      for(int j = 0; j < N; ++j)
      {
         HT[k] = &tabs[ sc.Td ];
         offset[k] = off;
         ++k; ++off;
      }
   }

   int dx = 1;
   int dy = frame.n_mcu_X*frame.mcu;

   if( scan.Ns == 2 )
      dx *= 2;

   if( scan.Ns == 4 )
      dx *= 2;

   TBitReader bb(in);

   // Ss - predictor
   // Al - point transform

   color_t* line = picture;
   for( int i_line = 0; i_line < (int)frame.n_mcu_Y; ++i_line, line += dy )
   {
      int predictor = i_line == 0 ? -1 : 2;
      int predictor2 = i_line == 0 ? 1 : scan.Ss;
      color_t* pix = line;

      for(int i_mcu=0; i_mcu < (int)frame.n_mcu_X; ++i_mcu, pix += dx )
      {
         // decode MCU
         for(int i=0; i<scan_mcu_size; ++i)
         {
            int ret = unpack(bb, HT[i], scan.Al, pix + offset[i], predictor, dx, dy, bug );

            if( ret < 0 )
               return bb.marker;
         }

         predictor = predictor2;
      }
   }

   // shift samples
   if( scan.Al )
   {
      int Pt = scan.Al;
      if( Pt > 4 )
      {
         Pt -= 4;

         int n = frame.n_mcu_Y*frame.n_mcu_X*frame.mcu;
         color_t* pix = picture;

         for(int i=0; i<n; ++i)
            pix[i] <<= Pt;
      }
   }
   return bb.marker;
}

////////////////////////////////////////////////////////////////////////////////
bool safe_read_jpeg(FILE* in, color_t picture[], bool bug)
{
   try
   { 

   TFramePar frame;
   THuffTab tabs[MAX_C];

   int marker;
   int marker_count = 0;
   unsigned segment_pos, segment_size;

   for(;; fseek( in, segment_pos + segment_size, SEEK_SET ) )
   {
      // wait for marker
   next_marker:
      if( feof(in) )
      {
         TRACE(("JPEG error: EOF\n"))
         return false;
      }

      marker = fgetc(in);
      if (marker == -1)
      {
         TRACE(("JPEG error: EOF\n"))
         return false;
      }

      if ( marker != 0xFF )
      {
         TRACE(("JPEG error: marker != 0xFF\n", marker))
         return false;
      }

      do
      {
         marker = fgetc(in);
         if (marker == -1)
         {
            TRACE(("JPEG error: EOF\n"))
            return false;
         }
      }
      while (marker == 0xFF);

parse_marker:
      // store start of marker segment
      segment_pos = ftell(in);

      if( marker_count == 0 && marker != 0xD8 )
      {
         TRACE(("JPEG error: marker (0x%02X) != 0xD8\n", marker))
         return false;
      }

      ++marker_count;

      if( marker == 0x00 ) 
         goto next_marker;

      if( marker == 0xD8 ) // SOI
         goto next_marker;

      if( marker == 0xD9 ) // EOI
         break;

      segment_size = fgetw(in);
      if( feof(in) ) { break; }

      if ( marker == 0xC4 ) // DHT
      {
         int sz = segment_size - 2;

         for(int t=0; sz >= 17; ++t)
         {
            t;
            unsigned temp = fgetc(in);
            unsigned Tc = temp >> 4; // table class 0-DC, 1-AC
            unsigned Th = temp & 15;

            if( Tc >= 2 )
            {
               TRACE(("JPEG DHT error: Tc=%d\n", Tc))
               return false;
            }
            if( Th >= MAX_C )
            {
               TRACE(("JPEG DHT error: Th=%d\n", Th))
               return false;
            }

            unsigned char lens[16];

            fread( lens, 16, 1, in );

            THuffTab* tab = &tabs[Th];

            tab->Init(Tc, Th);

            tab->Create( lens );

            sz -= tab->nItems;

            THuffRec* p = tab->hufftab;

            for(int i=0; i<tab->nItems; ++i)
            {
               p->value = (unsigned char)fgetc(in);
               ++p;
            }
            sz -= 17;
         }
      }
      else
      if ( (marker & 0xF0) == 0xC0 ) //SOF
      {
         frame.type = marker & 15;
         frame.P = fgetc(in);
         frame.Y = fgetw(in);
         frame.X = fgetw(in);
         frame.Nf = fgetc(in);
         frame.mcu = 0;
         frame.mcuW = 0;
         frame.mcuH = 0;

         if( frame.Nf > MAX_C )
         {
            TRACE(("JPEG error: frame.Nf (%u) > 4\n", frame.Nf))
            return false;
         }

         unsigned maxH = 0;
         unsigned maxV = 0;

         for(int i=0; i<(int)frame.Nf; ++i)
         {
            unsigned Ci = fgetc(in);
            unsigned temp = fgetc(in);
            unsigned Hi = temp >> 4;
            unsigned Vi = temp & 15;
            unsigned Tq = fgetc(in);

            frame.item[i].C = (unsigned char)Ci;
            frame.item[i].H = (unsigned char)Hi;
            frame.item[i].V = (unsigned char)Vi;
            frame.item[i].Tq = (unsigned char)Tq;
            frame.item[i].off = (unsigned char)frame.mcu;
            unsigned N = Hi*Vi;
            frame.item[i].N = (unsigned char)N;
            frame.mcu += N;

            if( Hi > maxH )
               maxH = Hi;
            if( Vi > maxV )
               maxV = Vi;
         }

         if( frame.Nf == 1 )
         {
            frame.mcu = 1;
            maxH = 1;
            maxV = 1;

            frame.item[0].H = (unsigned char)1;
            frame.item[0].V = (unsigned char)1;
            frame.item[0].off = 0;
            frame.item[0].N = 1;
         }

         if( frame.type <= 2 )
         {
            frame.mcuW = 8*maxH;
            frame.mcuH = 8*maxV;
         }
         else
         {
            frame.mcuW = maxH;
            frame.mcuH = maxV;
         }
         
         frame.n_mcu_X = (frame.X + frame.mcuW - 1)/frame.mcuW;
         frame.n_mcu_Y = (frame.Y + frame.mcuH - 1)/frame.mcuH;
      }
      else
      if( marker == 0xDA ) // SOS
      {
         TScanPar scan;
         scan.Ns = fgetc(in);

         if( scan.Ns > MAX_C )
         {
            TRACE(("JPEG error: scan.Ns (%u) > 4\n", scan.Ns))
            return false;
         }

         for(int i=0; i<(int)scan.Ns; ++i)
         {
            unsigned Csi = fgetc(in);
            unsigned temp = fgetc(in);
            unsigned Tdi = temp >> 4;
            unsigned Tai = temp & 15;

            scan.pars[i].Cs = (unsigned char)Csi;
            scan.pars[i].Td = (unsigned char)Tdi;
            scan.pars[i].Ta = (unsigned char)Tai;
            scan.pars[i].i = (unsigned char)0;

            for(int k=0; k<(int)frame.Nf; ++k)
            {
               if( frame.item[k].C == (unsigned char)Csi )
                  scan.pars[i].i = (unsigned char)k;
            }
         }

         scan.Ss = fgetc(in);
         scan.Se = fgetc(in);
         unsigned temp = fgetc(in);
         scan.Ah = temp >> 4;
         scan.Al = temp & 15;

         if( frame.type == 3 ) // lossless
         {
            marker = lossless_unpack(in, tabs, frame, scan, picture, bug);

            if( marker )
               goto parse_marker;

            goto next_marker;
         }
         else
         {
            TRACE(("JPEG error: unsupported frame type %u\n", frame.type))
            return false;
         }
      }
   }
   return true;

   }
   catch(const char* x)
   {
      printf("error: %s\n", x);
   }
   catch(...)
   {
      printf("error: exception\n");
   }

   return false;
}

bool read_jpeg( FILE* in, color_t picture[], bool bug )
{
   __try
   {
      return safe_read_jpeg( in, picture, bug );

   }
   __except(1)
   {
      printf("error: except\n");
   }
   return true;
}



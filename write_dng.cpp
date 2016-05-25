#include <stdio.h>
#include <string.h>
#pragma hdrstop

/*
   Coolpix RAW to DNG converter
   by paul69, v0.2, 2006-01-24, <e2500@narod.ru>

   Writting DNG file
*/

#include "tiff.h"
#include "raw2nef.h"
#include "write_dng.h"
#include "file.h"

// thumbnail size
#define tn_H 120
#define tn_W 160
// shift from 12 to 16 bits (!compatible)
#define SHIFT (16 - ccd_pars.bits_per_sample)
#define TRANSFORM_POINT 1

bool write(IFDir& ifd, FILE* out, bool endian, unsigned tiff_start, unsigned next_ifd);
bool write_word(FILE* out, bool endian, unsigned w);
bool write_dword(FILE* out, bool endian, unsigned dw);
bool write_thumbnail(FILE*, color_t const[], int w, int h, TCCDParam const&);
////////////////////////////////////////////////////////////////////////////////
void get_tile(TCCDParam& ccd_pars, color_t const* ccd, int tile_i,
   int tile_w, int tile_h, color_t tile[])
{
   unsigned tile_x = ((ccd_pars.raw_width+tile_w-1)/tile_w);

   if( !tile_x ) return;

   int x = (tile_i % tile_x)*tile_w;
   int w = tile_w;
   if( x + tile_w >= (int)ccd_pars.raw_width )
      w = ccd_pars.raw_width - x;

   int y = (tile_i / tile_x)*tile_h;
   int h = tile_h;
   if( y + tile_h >= (int)ccd_pars.raw_height )
      h = ccd_pars.raw_height - y;

   int i, j;
   color_t* t = tile;
   color_t const* c = &ccd[ y*ccd_pars.raw_width + x ];
   for(i = 0; i<h; ++i, t += tile_w, c += ccd_pars.raw_width )
   {
      for(j=0; j<w; ++j)
      {
         t[j] = c[j];
      }
      // optimum for predictor=1
      for(j; j<tile_w; ++j)
         t[j] = t[j-2];
   }

   // optimum for predictor=1
   for(i; i<tile_h; ++i, t += tile_w)
   {
      for(j=0; j<tile_w; ++j)
         t[j] = 0;
   }
}

// returns Pt - point transformation
int get_tile(TCCDParam& ccd_pars, color_t const* ccd, int tile_i,
   int tile_w, int tile_h, color_t tile[], int shift)
{
   unsigned tile_x = ((ccd_pars.raw_width+tile_w-1)/tile_w);

   if( !tile_x ) return 0;

   int x = (tile_i % tile_x)*tile_w;
   int w = tile_w;
   if( x + tile_w >= (int)ccd_pars.raw_width )
      w = ccd_pars.raw_width - x;

   int y = (tile_i / tile_x)*tile_h;
   int h = tile_h;
   if( y + tile_h >= (int)ccd_pars.raw_height )
      h = ccd_pars.raw_height - y;

   unsigned _or = 0;
   int i, j;
   color_t* t = tile;
   color_t const* c = &ccd[ y*ccd_pars.raw_width + x ];
   for(i = 0; i<h; ++i, t += tile_w, c += ccd_pars.raw_width )
   {
      for(j=0; j<w; ++j)
      {
         unsigned sample = c[j] << shift;
         t[j] = (color_t)sample;
         _or |= sample;
      }
      for(j; j<tile_w; ++j)
         t[j] = t[j-2];
   }

   // optimum for predictor > 1
   int offset = 2*tile_w;
   for(i; i<tile_h; ++i, t += tile_w)
   {
      for(j=0; j<tile_w; ++j)
         t[j] = t[j - offset];
   }

   for(i=0; i<16; ++i)
   {
      if( _or & (1<<i) )
         return i;
   }
   return 0;
}

void shift_samples(int tile_width, int tile_height, color_t tile[], int Pt)
{
   int n = tile_width * tile_height;
   color_t* p = tile;
   for(int i=0; i<n; ++i)
      p[i] >>= Pt;
}
///////////////////////////////////////////////////////////////////////////////

void THuffTab2::init(int tc, int th)
{
   used = 1;
   Tc = tc;
   Th = th;
   memset( count, 0, sizeof(count) );
}

bool THuffTab2::operator == ( THuffTab2 const & a ) const
{
   if( !used ) return false;
   if( !a.used ) return false;
   
   if( nCode != a.nCode ) return false;
   if( memcmp( bits, a.bits, 16 ) != 0 ) return false;
   if( memcmp( code, a.code, nCode ) != 0 ) return false;
   return true;
}

void THuffTab2::init( unsigned char lens[], unsigned char value[], int nItems )
{
   nCode = nItems;
   memcpy( bits, lens, 16 );
   memcpy( code, value, nCode );

   THuffRec* temp_hufftab = new THuffRec[nItems];

   make_hufftab(lens, temp_hufftab);

   for(int i=0; i<nItems; ++i)
   {
      temp_hufftab[i].value = value[i];
      hufftab[ value[i] ] = temp_hufftab[i];
   }
   
   delete [] temp_hufftab;
}

void THuffTab2::generate_codes()
{
   unsigned char lens[17];
   unsigned char code[256];

   int i, j;

   for(i=0; i<256; ++i)
   {
      code[i] = (unsigned char)i;
   }

   int n_codes = 256;
   for(i=0; i<256; ++i)
   {
      unsigned max_i = i;
      unsigned max_count = count[ code[max_i] ];
      for(j=i+1; j<256; ++j)
      {
         if( count[code[j]] > max_count )
         {
            max_i = j;
            max_count = count[code[j]];
         }
      }

      if( max_count == 0 ) {
         n_codes = i;
         break; }

      if( i != max_i )
      {
         unsigned char temp = code[i];
         code[i] = code[max_i];
         code[max_i] = temp;
      }
   }

   count[256] = 1;

   int next[257];
   memset( next, -1, sizeof( next ) );

   unsigned code_size[257];
   memset( code_size, 0, sizeof( code_size ) );

   for(;;)
   {
      unsigned min_count = 0;
      int v1 = -1;
      for(i=0; i<257; ++i)
      {
         if( !count[i] ) continue;
         if( min_count == 0 || count[i] < min_count )
         {
            min_count = count[i];
            v1 = i;
         }
      }
      min_count = 0;
      int v2 = -1;
      for(i=0; i<257; ++i)
      {
         if( i == v1 ) continue;
         if( !count[i] ) continue;
         if( min_count == 0 || count[i] < min_count )
         {
            min_count = count[i];
            v2 = i;
         }
      }

      if( v2 == -1 )
         break;

      count[v1] += count[v2];
      count[v2] = 0;

      for(;;)
      {
         ++code_size[v1];
         if( next[v1] == -1 ) break;
         v1 = next[v1];
      }
      next[v1] = v2;

      for(;;)
      {
         ++code_size[v2];
         if( next[v2] == -1 ) break;
         v2 = next[v2];
      }
   }

   unsigned bits[40];
   memset( bits, 0, sizeof( bits ) );
   for(i=0; i<257; ++i)
   {
      if( code_size[i] && code_size[i] < 40 )
         ++bits[ code_size[i] ];
   }

   for(i=40; --i > 16; )
   {
      for(;;)
      {
         if( bits[i] == 0 ) break;

         int j = i-1;
         for(;;)
         {
            if( bits[--j] ) break;
            if( j == 0 ) break;
         }
         bits[i] -= 2;
         bits[i-1] += 1;
         bits[j+1] += 2;
         bits[j] -= 1;
      }
   }

   for(i=16; i >= 0; --i )
   {
      if( bits[i] )
      {
         --bits[i];
         break;
      }
   }

   // copy result
   int k=0;
   for(i=0; i<16; ++i)
   {
      lens[i] = (unsigned char)bits[i+1];

      // and sort codes
      int j = k;
      k += lens[i];

      for(j; j<k; ++j )
      {
         for(int l=j+1; l<k; ++l)
         {
            if( code[l] < code[j] )
            {
               unsigned char temp = code[l];
               code[l] = code[j];
               code[j] = temp;
            }
         }   
      }
   }

   init( lens, code, n_codes );
}

///////////////////////////////////////////////////////////////////////////////

TScanData::~TScanData()
{
}

TScanData::TScanData(int Width, int Height)
{
   tabs[0].init(0, 0);
   tabs[1].init(0, 1);

   frame.type = 3;
   frame.P = 16;
   frame.Y = Height;
   frame.X = Width/2;
   frame.Nf = 2;
   frame.mcu = 0;

   scan.Ns = 2; // number of components

   scan.Ss = 1;
   scan.Se = 0;
   scan.Ah = 0;
   scan.Al = 0;

   unsigned maxH = 0;
   unsigned maxV = 0;

   for(int i=0; i<(int)frame.Nf; ++i)
   {
      frame.item[i].C = (byte)i;
      frame.item[i].H = 1;
      frame.item[i].V = 1;
      frame.item[i].Tq = (byte)i;

      frame.item[i].off = (unsigned char)frame.mcu;
      unsigned N = frame.item[i].H*frame.item[i].V;
      frame.item[i].N = (unsigned char)N;
      frame.mcu += N;

      if( frame.item[i].H > maxH )
         maxH = frame.item[i].H;
      if( frame.item[i].V > maxV )
         maxV = frame.item[i].V;

      scan.pars[i].Cs = (byte)i; // component id
      scan.pars[i].Td = (byte)i;
      scan.pars[i].Ta = 0;
      scan.pars[i].i = (byte)i; // index in TFramePar::item[]
   }

   frame.mcuW = maxH;
   frame.mcuH = maxV;

   frame.n_mcu_X = (frame.X + frame.mcuW - 1)/frame.mcuW;
   frame.n_mcu_Y = (frame.Y + frame.mcuH - 1)/frame.mcuH;
}

TScanData::TScanData(int Width, int Height, int Pt,
   int predictor)
{
   tabs[0].init(0, 0);
   tabs[1].init(0, 1);

   if( predictor > 1 )
   {
      tabs[2].init(0, 2);
      tabs[3].init(0, 3);
   }

   frame.type = 3;
   frame.P = 16;
   frame.Y = predictor > 1 ? Height/2 : Height;
   frame.X = Width/2;
   frame.Nf = predictor > 1 ? 4 : 2;
   frame.mcu = 0;

   scan.Ns = predictor > 1 ? 4 : 2; // number of components

   scan.Ss = predictor;
   scan.Se = 0;
   scan.Ah = 0;
   scan.Al = Pt; // point transformation

   unsigned maxH = 0;
   unsigned maxV = 0;

   for(int i=0; i<(int)frame.Nf; ++i)
   {
      frame.item[i].C = (byte)i;
      frame.item[i].H = 1;
      frame.item[i].V = 1;
      frame.item[i].Tq = (byte)i;

      frame.item[i].off = (unsigned char)frame.mcu;
      unsigned N = frame.item[i].H*frame.item[i].V;
      frame.item[i].N = (unsigned char)N;
      frame.mcu += N;

      if( frame.item[i].H > maxH )
         maxH = frame.item[i].H;
      if( frame.item[i].V > maxV )
         maxV = frame.item[i].V;

      scan.pars[i].Cs = (byte)i; // component id
      scan.pars[i].Td = (byte)i;
      scan.pars[i].Ta = 0;
      scan.pars[i].i = (byte)i; // index in TFramePar::item[]
   }

   frame.mcuW = maxH;
   frame.mcuH = maxV;

   frame.n_mcu_X = (frame.X + frame.mcuW - 1)/frame.mcuW;
   frame.n_mcu_Y = (frame.Y + frame.mcuH - 1)/frame.mcuH;
}

////////////////////////////////////////////////////////////////////////////////
unsigned get_tile_size( color_buf tile, int W, int H, int Pt, int predictor );
unsigned get_tile_size( color_buf tile, int W, int H );
void write_tile( FILE* out, color_buf tile, int W, int H, int Pt, int predictor );
void write_tile( FILE* out, color_buf tile, int W, int H );

bool TIFF_Content::write_dng(FILE* out, bool endian, bool compatible, bool optimize)
{
   // hide members to prevent changes
   IFDir IFD0(this->IFD1);
   IFDir EXIF(this->EXIF); 

   // add to IFD0;
   unsigned ifd0_start = 8;
   unsigned ifd0_data_size = tn_W*tn_H*3; //57600;
   IFD0.add_DWORD( TIFF::NewSubfile, 1 ); // thumbnail
   IFD0.add_DWORD( TIFF::ImageWidth, tn_W );
   IFD0.add_DWORD( TIFF::ImageHeight, tn_H );
   const int samples_per_pixel = 3;
   word bits_per_sample[samples_per_pixel] = {8,8,8};
   IFD0.add_tag( TIFF::BitsPerSample, TIFF::type_WORD, samples_per_pixel, bits_per_sample );
   IFD0.add_WORD( TIFF::Compression, 1 );
   IFD0.add_WORD( TIFF::PhotometricInterpretation, 2 ); // RGB

   int image_W = ccd_pars.raw_width;
   int image_H = ccd_pars.raw_height;

   IFD0.add_DWORD( TIFF::StripOffsets, 0 );
   IFD0.add_WORD( TIFF::SamplesPerPixel, samples_per_pixel );
   IFD0.add_DWORD( TIFF::RowsPerStrip, tn_H);
   IFD0.add_DWORD( TIFF::StripByteCounts, ifd0_data_size );

   IFD0.add_RATIONAL( TIFF::XResolution, Rational(300,1) );
   IFD0.add_RATIONAL( TIFF::YResolution, Rational(300,1) );
   IFD0.add_WORD( TIFF::PlanarConfiguration, 1 ); // chunky
   IFD0.add_WORD( TIFF::ResolutionUnit, 2 ); // inch

   if( !IFD0.get_tag(TIFF::Orientation) )
      IFD0.add_WORD( TIFF::Orientation, 1 ); //  TopLeft

   dword sub_ifd = 0;
   IFD0.add_DWORD( TIFF::SubIDFs, sub_ifd ); // SubIDF's offsets
   IFD0.add_DWORD( 0x8769, 0 ); // EXIF offset

   EXIF.remove( TIFF::MakerNote );
   EXIF.remove( TIFF::ColorFilterArrayPattern );

   byte exif_ver[4] = {'0','2','2','0'};
   EXIF.add_tag( TIFF::ExifVersion, TIFF::type_UNDEFINED, 4, exif_ver );

   // move ISOSpeedRating from MakerNote to EXIF
   if( !EXIF.get_tag(0x8827) && Is_NIKON() && MakerNote.get_count() > 0 )
   {
      IFDir::Tag* iso_setting = MakerNote.get_tag( 0x002 );
      if( iso_setting && iso_setting->count == 2 )
         EXIF.add_WORD( 0x8827, (word)iso_setting->get_value(1) );
   }

#if defined(DNG_VER) && (DNG_VER == 0x01010000)
   byte dng_ver[4] = {1,1,0,0};
#else
   byte dng_ver[4] = {1,0,0,0};
#endif
   IFD0.add_tag( TIFF::DNGVersion, TIFF::type_BYTE, 4, dng_ver );
   IFD0.add_tag( TIFF::DNGBackwardVersion, TIFF::type_BYTE, 4, dng_ver );

   if( (ccd_pars.cfa_colors & TCCDParam::PrimaryColorsBit) )
   {
      rational ColorMatrix1[9] = {
         { 9490,10000}, {-3814,10000}, { -225,10000},
         {-6649,10000}, {13741,10000}, { 3236,10000},
         { -627,10000}, {  796,10000}, { 7550,10000}, };
      IFD0.add_tag( 0xC621, TIFF::type_SRATIONAL, 9, ColorMatrix1 ); // ColorMatrix1

      rational ColorMatrix2[9] = {
         { 8489,10000}, {-2583,10000}, {-1036,10000},
         {-8051,10000}, {15583,10000}, { 2643,10000},
         {-1307,10000}, { 1407,10000}, { 7354,10000} };
      IFD0.add_tag( 0xC622, TIFF::type_SRATIONAL, 9, ColorMatrix2 ); // ColorMatrix2

      rational AnalogBalance[3] = {{1,1},{1,1},{1,1}};
      IFD0.add_tag( 0xC627, TIFF::type_RATIONAL, 3, AnalogBalance ); // AnalogBalance

      rational AsShotNeutral[3] = {{434635,1000000},{1000000,1000000},{768769,1000000}};
      IFD0.add_tag( 0xC628, TIFF::type_RATIONAL, 3, AsShotNeutral ); // AsShotNeutral
   }
   else
   {
      rational ColorMatrix1[12] = {
         {-3749,10000}, { 9365,10000}, { 3019,10000},
         { 6661,10000}, {-1607,10000}, { 3712,10000},
         {-3375,10000}, { 8120,10000}, { 6222,10000},
         { 2313,10000}, { 6603,10000}, { 1061,10000}, };
      IFD0.add_tag( 0xC621, TIFF::type_SRATIONAL, 12, ColorMatrix1 ); // ColorMatrix1

      rational ColorMatrix2[12] = {
         {-5368,10000}, {11478,10000}, { 2368,10000},
         { 5537,10000}, { -113,10000}, { 3148,10000},
         {-4969,10000}, {10021,10000}, { 5782,10000},
         {  778,10000}, { 9028,10000}, {  211,10000} };
      IFD0.add_tag( 0xC622, TIFF::type_SRATIONAL, 12, ColorMatrix2 ); // ColorMatrix2

      rational AnalogBalance[4] = {{1,1},{1,1},{1,1},{1,1}};
      IFD0.add_tag( 0xC627, TIFF::type_RATIONAL, 4, AnalogBalance ); // AnalogBalance
   }

   IFD0.add_SRATIONAL( 0xC62A, Rational(-100,100) ); // BaselineExposure
   IFD0.add_RATIONAL( 0xC62B, Rational(200,100) ); // BaselineNoise
   IFD0.add_RATIONAL( 0xC62C, Rational(300,100) ); // BaselineSharpness
   IFD0.add_RATIONAL( 0xC62E, Rational(100,100) ); // LinearResponseLimit
   IFD0.add_RATIONAL( 0xC633, Rational(1,1) ); // ShadowScale
   IFD0.add_WORD( 0xC65A, 17 ); // CalibrationIlluminant1
   IFD0.add_WORD( 0xC65B, 21 ); // CalibrationIlluminant2

   IFDir IFD;
   // add to main IFD;
   IFD.add_DWORD( TIFF::NewSubfile, 0 );
   IFD.add_DWORD( TIFF::ImageWidth, image_W );
   IFD.add_DWORD( TIFF::ImageHeight, image_H );
   IFD.add_WORD( TIFF::BitsPerSample, 16 );
   IFD.add_WORD( TIFF::Compression, 7 ); // JPEG
   IFD.add_WORD( TIFF::PhotometricInterpretation, 0x8023 ); // CFA

   unsigned tile_width = 256;
   unsigned tile_height = 256;

   if( compatible && !(ccd_pars.cfa_colors & TCCDParam::PrimaryColorsBit))
   {
      tile_width = 240;
   }

   unsigned tile_x = ((image_W+tile_width-1)/tile_width);
   unsigned tile_y = ((image_H+tile_height-1)/tile_height);

   if( !compatible )
   {
      tile_width = (image_W/2+tile_x-1)/tile_x*2;
      tile_x = ((image_W+tile_width-1)/tile_width);

      tile_height = (image_H/2+tile_y-1)/tile_y*2;
      tile_y = ((image_H+tile_height-1)/tile_height);

      if( optimize )
      {
         // optimize tile size
         dword file_size = 0;

         unsigned n_tile_x = tile_x;
         unsigned n_tile_y = tile_y;
         for( unsigned i_tile_x = 3; i_tile_x <= n_tile_x; ++i_tile_x )
         for( unsigned i_tile_y = 2; i_tile_y <= n_tile_y; ++i_tile_y )
         {
            unsigned _tile_width = (image_W/2+i_tile_x-1)/i_tile_x*2;
            unsigned _tile_x = ((image_W+_tile_width-1)/_tile_width);

            unsigned _tile_height = (image_H/2+i_tile_y-1)/i_tile_y*2;
            unsigned _tile_y = ((image_H+_tile_height-1)/_tile_height);

            buff_t<color_t> _tile(_tile_width*_tile_height);

            unsigned _tile_count = _tile_x*_tile_y;

            dword picture_size = 0;
            for(int i=0; i<(int)_tile_count; ++i)
            {   
               int Pt = get_tile(ccd_pars, picture, i, _tile_width, _tile_height, _tile, SHIFT)*TRANSFORM_POINT;
               if( Pt ) shift_samples(_tile_width, _tile_height, _tile, Pt);

               unsigned tile_size = get_tile_size( _tile, _tile_width, _tile_height, Pt, 1 );

               // select best predictor: not standard
               for(int p = 2; p<=7; ++p)
               {
                  unsigned size = get_tile_size( _tile, _tile_width, _tile_height, Pt, p );
                  if( size < tile_size )
                  {
                     tile_size = size;
                  }
               }

               picture_size += tile_size;
            }
#ifdef __CONSOLE__
   printf("%3u x %-3u %5u x %-5u = %u (%d)\n", _tile_x, _tile_y, _tile_width, _tile_height, picture_size,
      _tile_x*_tile_width*_tile_y*_tile_height - image_W*image_H);
#endif
            if( file_size == 0 || picture_size < file_size )
            {
               file_size = picture_size;

               tile_width = _tile_width;
               tile_x = _tile_x;

               tile_height = _tile_height;
               tile_y = _tile_y;
            }
         }
      }
   }

   unsigned tile_count = tile_x * tile_y;

   IFD.add_WORD( TIFF::SamplesPerPixel, 1 );
   IFD.add_DWORD( TIFF::TileWidth, tile_width);
   IFD.add_DWORD( TIFF::TileLength, tile_height);
   IFD.add_tag(TIFF::TileByteCounts, TIFF::type_DWORD, tile_count, (dword*)0);
   IFD.add_tag(TIFF::TileOffsets, TIFF::type_DWORD, tile_count, (dword*)0);

   IFD.add_WORD( TIFF::PlanarConfiguration, 1 ); // chunky

   word CFARepeatPatternDim[2] = { 2, 2 };
   byte CFAPattern[4];
   switch( ccd_pars.cfa_colors )
   {
   case 0: // GMYC
      CFAPattern[0] = 1;
      CFAPattern[1] = 4;
      CFAPattern[2] = 5;
      CFAPattern[3] = 3;
      break;
   case 1: // MGCY
      CFAPattern[0] = 4;
      CFAPattern[1] = 1;
      CFAPattern[2] = 3;
      CFAPattern[3] = 5;
      break;
   case 2: // YCGM
      CFAPattern[0] = 5;
      CFAPattern[1] = 3;
      CFAPattern[2] = 1;
      CFAPattern[3] = 4;
      break;
   case 3: // CYMG
      CFAPattern[0] = 3;
      CFAPattern[1] = 5;
      CFAPattern[2] = 4;
      CFAPattern[3] = 1;
      break;
   case 4: // BGGR
      CFAPattern[0] = 2;
      CFAPattern[1] = 1;
      CFAPattern[2] = 1;
      CFAPattern[3] = 0;
      break;
   case 5: // GBRG
      CFAPattern[0] = 1;
      CFAPattern[1] = 2;
      CFAPattern[2] = 0;
      CFAPattern[3] = 1;
      break;
   case 6: // GRBG
      CFAPattern[0] = 1;
      CFAPattern[1] = 0;
      CFAPattern[2] = 2;
      CFAPattern[3] = 1;
      break;
   case 7: // RGGB
      CFAPattern[0] = 0;
      CFAPattern[1] = 1;
      CFAPattern[2] = 1;
      CFAPattern[3] = 2;
      break;

   default:
      CFAPattern[0] = (byte)((ccd_pars.cfa_colors >> 16) & 0x0F);
      CFAPattern[1] = (byte)((ccd_pars.cfa_colors >> 12) & 0x0F);
      CFAPattern[2] = (byte)((ccd_pars.cfa_colors >> 8) & 0x0F);
      CFAPattern[3] = (byte)((ccd_pars.cfa_colors >> 4) & 0x0F);
   }
   
   IFD.add_tag( TIFF::CFARepeatPatternDim, TIFF::type_WORD, 2, CFARepeatPatternDim );
   IFD.add_tag( TIFF::CFAPattern, TIFF::type_BYTE, 4, CFAPattern );

   if( !(ccd_pars.cfa_colors & TCCDParam::PrimaryColorsBit) )
   {
      byte CFAPlaneColor[4] = {1,4,3,5};
      IFD.add_tag( 0xC616, TIFF::type_BYTE, 4, CFAPlaneColor ); // CFAPlaneColor
   }
   IFD.add_WORD( 0xC617, 1 ); // CFALayout

   //if( this->linear_tab.size() )
   //{
   //   IFD.add_tag( 0xC618, TIFF::type_WORD, this->linear_tab.size(), (word*)this->linear_tab ); // LinearizationTable
   //}

   word BlackLevelRepeatDim[2] = { 1, 1 };
   IFD.add_tag( 0xC619, TIFF::type_WORD, 2, BlackLevelRepeatDim ); // BlackLevelRepeatDim
   IFD.add_RATIONAL( 0xC61A, Rational(0,256) ); // BlackLevel
   IFD.add_WORD( 0xC61D, compatible ? (color_t)4095 : (color_t)((1<<(SHIFT+12))-1) ); // WhiteLevel

   rational DefaultScale[2] = {{1,1},{1,1}};
   IFD.add_tag( 0xC61E, TIFF::type_RATIONAL, 2, DefaultScale ); // DefaultScale

   rational DefaultCropOrigin[2] = {{8,1},{2,1}};
   IFD.add_tag( 0xC61F, TIFF::type_RATIONAL, 2, DefaultCropOrigin ); // DefaultCropOrigin

   rational DefaultCropSize[2] = {{image_W-16,1},{image_H-4,1}};
   IFD.add_tag( 0xC620, TIFF::type_RATIONAL, 2, DefaultCropSize ); // DefaultCropSize

   IFD.add_RATIONAL( 0xC632, Rational(100,100) ); // AntiAliasStrength
   IFD.add_RATIONAL( 0xC65C, Rational(1,1) ); // BestQualityScale

   unsigned ifd0_size = IFD0.get_size();
   unsigned ifd_size = IFD.get_size();

   sub_ifd = ifd0_start + ifd0_size;

   unsigned exif_start = sub_ifd + ifd_size;
   unsigned exif_size = EXIF.get_size();

   unsigned image0_offset = exif_start + exif_size;
   unsigned image_offset = image0_offset + ifd0_data_size;

   IFD0.add_DWORD( TIFF::SubIDFs, sub_ifd ); // SubIDF's offsets
   IFD0.add_DWORD( 0x8769, exif_start ); // EXIF offset

   IFD0.add_DWORD( TIFF::StripOffsets, image0_offset );

   buff_t<color_t> tile(tile_width*tile_height);
   buff_t<int> predictor(tile_count);
   buff_t<dword> tile_size( tile_count );
   buff_t<dword> tile_offsets( tile_count );

   if( compatible )
   {
      dword tile_offset = image_offset;
      for(int i=0; i<(int)tile_count; ++i)
      {
         get_tile(ccd_pars, picture, i, tile_width, tile_height, tile);
         predictor[i] = 1;
         tile_size[i] = get_tile_size( tile, tile_width, tile_height );

         tile_offsets[i] = tile_offset;
         tile_offset += tile_size[i];
      }
   }
   else
   {
      dword tile_offset = image_offset;
      for(int i=0; i<(int)tile_count; ++i)
      {
         int Pt = get_tile(ccd_pars, picture, i, tile_width, tile_height, tile, SHIFT)*TRANSFORM_POINT;
         if( Pt ) shift_samples(tile_width, tile_height, tile, Pt);

         predictor[i] = 1;
         tile_size[i] = get_tile_size( tile, tile_width, tile_height, Pt, predictor[i] );

         // select best predictor: not standard
         for(int p = 2; p<=7; ++p)
         {
            unsigned size = get_tile_size( tile, tile_width, tile_height, Pt, p );
            if( size < tile_size[i] )
            {
               tile_size[i] = size;
               predictor[i] = p;
            }
         }

         tile_offsets[i] = tile_offset;
         tile_offset += tile_size[i];
      }
   }

   IFD.add_tag(TIFF::TileByteCounts, TIFF::type_DWORD, tile_count, (dword*)tile_size);
   IFD.add_tag(TIFF::TileOffsets, TIFF::type_DWORD, tile_count, (dword*)tile_offsets);

   // writing
   if( !write_word(out, endian, *(word*)(endian ? "II" : "MM")) )
      return false;

   write_word(out, endian, '*');
   write_dword(out, endian, 8);

   if( !write(IFD0, out, endian, 0, 0) )
      return false;

   if( (unsigned)ftell(out) != sub_ifd )
   {
      printf("error: sub_ifd=%08X curr=%08X\n", sub_ifd, ftell(out));
      return false;
   }

   if( !write(IFD, out, endian, 0, 0) )
      return false;

   if( (unsigned)ftell(out) != exif_start )
   {
      printf("error: exif_start=%08X curr=%08X\n", exif_start, ftell(out));
      return false;
   }

   if( !write(EXIF, out, endian, 0, 0) )
      return false;

   if( (unsigned)ftell(out) != image0_offset )
   {
      printf("error: image0_offset=%08X curr=%08X\n", image0_offset, ftell(out));
      return false;
   }

   // write tiff color thumbnail
   write_thumbnail(out, picture, tn_W, tn_H, ccd_pars );

   if( (unsigned)ftell(out) != image_offset )
   {
      printf("error: image_start=%08X curr=%08X\n", image_offset, ftell(out));
      return false;
   }

   for(int i=0; i<(int)tile_count; ++i)
   {
      if( (unsigned)ftell(out) != tile_offsets[i] )
      {
         printf("error: tile_offset[%d]=%08X curr=%08X\n", i, tile_offsets[i], ftell(out));
         return false;
      }

      if( compatible )
      {
         get_tile(ccd_pars, picture, i, tile_width, tile_height, tile);
         write_tile( out, tile, tile_width, tile_height );
      }
      else
      {
         int Pt = get_tile(ccd_pars, picture, i, tile_width, tile_height, tile, SHIFT)*TRANSFORM_POINT;
         if( Pt ) shift_samples(tile_width, tile_height, tile, Pt);

         write_tile( out, tile, tile_width, tile_height, Pt, predictor[i] );
      }
   }

   return true;
}

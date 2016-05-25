#include <stdio.h>
#include <string.h>
#pragma hdrstop

/*
   Coolpix RAW to Coolpix NEF/DNG converter
   by paul69, v0.02, 2006-01-11, <e2500@narod.ru>

   Reading CFA raw file
   Supports:
      Lossless JPEG compressed DNG (upto 16-bits)
      Uncompressed interlaced NEF (12-bits)
      Uncompressed interlaced ORF (12-bits)
*/

#include "tiff.h"
#include "raw2nef.h"

bool read_jpeg( FILE* in, color_t Pix[], bool bug );
bool read_compressed_nikon(FILE* in, unsigned w, unsigned h, int bias, color_t Pix[]);

int read_cfa( FILE* in, IFDir* IFD, TIFF_Content& NEF )
{
   IFDir::Tag* PhotometricInterpretation = IFD->get_tag(TIFF::PhotometricInterpretation);
   if( !PhotometricInterpretation ) return e_error_format;
   if( PhotometricInterpretation->get_value() != 0x8023 ) return e_error_format;

   IFDir::Tag* CFARepeatPatternDim = IFD->get_tag(TIFF::CFARepeatPatternDim);
   if( !CFARepeatPatternDim ) return e_error_format;
   if( CFARepeatPatternDim->count != 2 ) return e_error_format;
   if( CFARepeatPatternDim->get_value(0) != 2 ) return e_error_format;
   if( CFARepeatPatternDim->get_value(1) != 2 ) return e_error_format;

   IFDir::Tag* CFAPattern = IFD->get_tag(TIFF::CFAPattern);
   if( !CFAPattern ) return e_error_format;
   if( CFAPattern->count != 4 ) return e_error_format;

   unsigned cfa = (CFAPattern->get_value(0) << 16)
      | (CFAPattern->get_value(1) << 12)
      | (CFAPattern->get_value(2) << 8)
      | (CFAPattern->get_value(3) << 4);

   TCCDParam& ccd_pars = NEF.ccd_pars;
   memset( &ccd_pars, 0, sizeof(TCCDParam) );

   switch(cfa)
   {
   case 0x14530: ccd_pars.cfa_colors = TCCDParam::GMYC; break;
   case 0x41350: ccd_pars.cfa_colors = TCCDParam::MGCY; break;
   case 0x53140: ccd_pars.cfa_colors = TCCDParam::YCGM; break;
   case 0x35410: ccd_pars.cfa_colors = TCCDParam::CYMG; break;
   case 0x21100: ccd_pars.cfa_colors = TCCDParam::BGGR; break;
   case 0x12010: ccd_pars.cfa_colors = TCCDParam::GBRG; break;
   case 0x10210: ccd_pars.cfa_colors = TCCDParam::GRBG; break;
   case 0x01120: ccd_pars.cfa_colors = TCCDParam::RGGB; break;
   default: ccd_pars.cfa_colors = cfa;
      if( ((cfa >> 16) & 0x0F) <= 2
       && ((cfa >> 12) & 0x0F) <= 2
       && ((cfa >>  8) & 0x0F) <= 2
       && ((cfa >>  4) & 0x0F) <= 2 )
         ccd_pars.cfa_colors |= TCCDParam::PrimaryColorsBit;
      break;
   }

   IFDir::Tag* CFALayout = IFD->get_tag(0xC617);
   if( CFALayout && CFALayout->get_value() != 1 )
      return e_not_supported;

   IFDir::Tag* LinearizationTable = IFD->get_tag(0xC618);
   if( LinearizationTable )
   {
      int linear_tab_count = LinearizationTable->count;
      if( linear_tab_count > 0 )
      {
         NEF.linear_tab.alloc( linear_tab_count );
         if( !(word*)NEF.linear_tab ) return e_not_enough_memory;

         for(int i=0; i<linear_tab_count; ++i)
            NEF.linear_tab[i] = (word)LinearizationTable->get_value(i);
      }
   }

   unsigned Width = IFD->get_tag(TIFF::ImageWidth)->get_value();
   unsigned Height = IFD->get_tag(TIFF::ImageHeight)->get_value();

   ccd_pars.raw_width = Width;
   ccd_pars.raw_height = Height;
   ccd_pars.file_size = 0;
   ccd_pars.row_length = 0;
   ccd_pars.bits_per_sample = 0;
   ccd_pars.camera = 0;
   ccd_pars.data_offset = 0;
   ccd_pars.flags = 0;

   IFDir::Tag* Compression = IFD->get_tag(TIFF::Compression);
   if( !Compression ) return e_error_format;

   if( Compression->get_value() == 7 )
   {
      // JPEG
      bool bug = false;
      IFDir::Tag* DNGVersion = NEF.IFD1.get_tag(TIFF::DNGVersion);
      if( DNGVersion )
      {
         unsigned version = (DNGVersion->get_value(0) << 24)
          | (DNGVersion->get_value(1) << 16)
          | (DNGVersion->get_value(2) << 8)
          | (DNGVersion->get_value(3));

         if( version == 0x01000000 )
            bug = true;
      }

      IFDir::Tag* BitsPerSample = IFD->get_tag(TIFF::BitsPerSample);
      if( !BitsPerSample ) return e_error_format;
      if( BitsPerSample->count != 1 ) return e_error_format;
      ccd_pars.bits_per_sample = BitsPerSample->get_value();

      IFDir::Tag* StripOffsets = IFD->get_tag(TIFF::StripOffsets);
      if( StripOffsets )
      {
         IFDir::Tag* StripByteCounts = IFD->get_tag(TIFF::StripByteCounts);
         if( !StripByteCounts ) return e_error_format;
         if( StripOffsets->count != StripByteCounts->count ) return e_error_format;

         IFDir::Tag* RowsPerStrip = IFD->get_tag(TIFF::RowsPerStrip);
         if( !RowsPerStrip ) return e_error_format;

         return e_not_supported;
      }

      IFDir::Tag* TileOffsets = IFD->get_tag(TIFF::TileOffsets);
      if( !TileOffsets ) return e_error_format;

      IFDir::Tag* TileByteCounts = IFD->get_tag(TIFF::TileByteCounts);
      if( !TileByteCounts ) return e_error_format;
      if( TileOffsets->count != TileByteCounts->count ) return e_error_format;

      IFDir::Tag* TileWidth = IFD->get_tag(TIFF::TileWidth);
      if( !TileWidth ) return e_error_format;

      IFDir::Tag* TileLength = IFD->get_tag(TIFF::TileLength);
      if( !TileLength ) return e_error_format;

      unsigned tile_w = TileWidth->get_value();
      unsigned tile_h = TileLength->get_value();
      unsigned tile_size = tile_w * tile_h;
      if( !tile_size ) return e_error_format;

      NEF.picture.alloc(Width*Height);
      if( !(color_t*)NEF.picture ) return e_not_enough_memory;

      buff_t<color_t> tile( tile_size );
      if( !(color_t*)tile ) return e_not_enough_memory;

      int tiles = TileOffsets->count;
      int tile_x = 0;
      int tile_y = 0;
      for(int tile_i=0; tile_i<tiles; ++tile_i)
      {
         fseek(in, TileOffsets->get_value(tile_i), SEEK_SET); 

         if( !read_jpeg( in, tile, bug ) )
            return e_unexpected_eof;

         int h = tile_h;
         if( tile_y + tile_h > Height )
            h = Height - tile_y;

         color_t* p = &NEF.picture[tile_y*Width + tile_x];

         int w = tile_w;
         if( tile_x + tile_w > Width )
            w = Width - tile_x;

         color_t* t = tile;

         for(int i=0; i<h; ++i, p += Width, t += tile_w )
         {
            for(int j=0; j<w; ++j )
            {
               p[j] = t[j];
            }
         }

         tile_x += tile_w;
         if( tile_x >= (int)Width )
         {
            tile_x = 0;
            tile_y += tile_h;
         }
      }
      return e_success;
   }
   else
   if( Compression->get_value() == 1 )
   {
      // Uncompressed
      IFDir::Tag* BitsPerSample = IFD->get_tag(TIFF::BitsPerSample);
      if( !BitsPerSample ) return e_error_format;
      if( BitsPerSample->count != 1 ) return e_error_format;

      IFDir::Tag* StripOffsets = IFD->get_tag(TIFF::StripOffsets);
      if( !StripOffsets )
         return e_error_format;

      IFDir::Tag* StripByteCounts = IFD->get_tag(TIFF::StripByteCounts);
      if( !StripByteCounts ) return e_error_format;
      if( StripOffsets->count != StripByteCounts->count ) return e_error_format;

      IFDir::Tag* RowsPerStrip = IFD->get_tag(TIFF::RowsPerStrip);
      if( !RowsPerStrip ) return e_error_format;

      IFDir::Tag* Make = NEF.IFD1.get_tag(TIFF::Make);
      if( !Make ) return e_error_format;

      IFDir::Tag* Model = NEF.IFD1.get_tag(TIFF::Model);

      unsigned picture_size = Width*Height;
      ccd_pars.bits_per_sample = BitsPerSample->get_value();

      if( ccd_pars.bits_per_sample == 12 )
      {
         if( NEF.is_COOLPIX_NEF )
         {
            NEF.picture.alloc( picture_size );
            if( !(color_t*)NEF.picture ) return e_not_enough_memory;

            ccd_pars.flags |= TCCDParam::fInterlaced;
            ccd_pars.cfa_colors &= TCCDParam::PrimaryColorsBit; // set GMYC or BGGR
            ccd_pars.row_length = (Width * 12 + 8-1) / 8;;
            ccd_pars.data_offset = StripOffsets->get_value();
            ccd_pars.file_size = ccd_pars.data_offset + StripByteCounts->get_value();
            ccd_pars.camera = Model ? pack(Model->value) : 1;

            return read_raw(in, NEF.picture, ccd_pars);
         }

         if( NEF.Is_NIKON() )
         {
            if( !Model )
               return e_error_format;

            if( Model->count >= 7 && 0 == memcmp(Model->value, "NIKON D", 7) )
            {
               NEF.picture.alloc( picture_size );
               if( !(color_t*)NEF.picture ) return e_not_enough_memory;

               ccd_pars.row_length = (Width * 12 + 8-1) / 8;;
               ccd_pars.data_offset = StripOffsets->get_value();
               ccd_pars.file_size = ccd_pars.data_offset + StripByteCounts->get_value();
               ccd_pars.camera = Model ? pack(Model->value) : 1;

               return read_raw(in, NEF.picture, ccd_pars);
            }

            return e_not_supported;
         }

         if( NEF.Is_OLYMPUS() )
         {
            NEF.picture.alloc( picture_size );
            if( !(color_t*)NEF.picture ) return e_not_enough_memory;

            ccd_pars.flags |= TCCDParam::fInterlaced;
            ccd_pars.row_length = (Width * 12 + 8-1) / 8;
            ccd_pars.raw_height &= ~1; // force to be even
            ccd_pars.data_offset = StripOffsets->get_value();

            fseek( in, 0, SEEK_END );
            ccd_pars.file_size = ftell( in );
            ccd_pars.camera = Model ? pack(Model->value) : 1;
            return read_raw(in, NEF.picture, ccd_pars);
         }

         if( NEF.Is_PENTAX() )
         {
            NEF.picture.alloc( picture_size );
            if( !(color_t*)NEF.picture ) return e_not_enough_memory;

            ccd_pars.row_length = 2*Width;
            ccd_pars.data_offset = StripOffsets->get_value();

            ccd_pars.file_size = ccd_pars.data_offset + StripByteCounts->get_value();
            ccd_pars.camera = Model ? pack(Model->value) : 1;

            ccd_pars.bits_per_sample = 16;
            int res = read_raw(in, NEF.picture, ccd_pars);
            ccd_pars.bits_per_sample = 12;
            return res;
         }
         if( NEF.Is_MINOLTA() )
         {
            if( !Model )
               return e_error_format;

            NEF.picture.alloc( picture_size );
            if( !(color_t*)NEF.picture ) return e_not_enough_memory;

            ccd_pars.data_offset = StripOffsets->get_value();
            ccd_pars.file_size = ccd_pars.data_offset + StripByteCounts->get_value();
            ccd_pars.camera = pack(Model->value);

            if( Model->count >= 9 && 0 == memcmp( Model->value, "DiMAGE 7i", 9 ) )
            {
               ccd_pars.row_length = 2*Width;
               ccd_pars.bits_per_sample = 16;
               int res = read_raw(in, NEF.picture, ccd_pars);
               ccd_pars.bits_per_sample = 12;
               return res;
            }
            else
            {
               ccd_pars.row_length = Width*12/8;
               return read_raw(in, NEF.picture, ccd_pars);
            }
         }
      }
      if( ccd_pars.bits_per_sample == 14 )
      {
         if( NEF.Is_SONY() ) // R1 SR2
         {
            NEF.picture.alloc( picture_size );
            if( !(color_t*)NEF.picture ) return e_not_enough_memory;

            // byte order is motorola

            ccd_pars.row_length = 2*Width;
            ccd_pars.data_offset = StripOffsets->get_value();

            fseek( in, 0, SEEK_END );
            ccd_pars.file_size = ftell( in );
            ccd_pars.camera = Model ? pack(Model->value) : 1;

            ccd_pars.bits_per_sample = 16;
            ccd_pars.data_shift = 0;

            int res = read_raw(in, NEF.picture, ccd_pars);

            ccd_pars.bits_per_sample = 14;
            return res;
         }
      }
      if( ccd_pars.bits_per_sample == 16 )
      {
         if( NEF.Is_OLYMPUS() )
         {
            NEF.picture.alloc( picture_size );
            if( !(color_t*)NEF.picture ) return e_not_enough_memory;

            if( Model && Model->count >= 5
             && 0 == memcmp( Model->value, "E-300", 5 ) )
            {
               ccd_pars.flags |= TCCDParam::f10PixPer128bits;
               ccd_pars.row_length = Width/10*128/8;
               ccd_pars.data_offset = StripOffsets->get_value();

               ccd_pars.file_size = ~0;
               ccd_pars.camera = pack(Model->value);

               ccd_pars.bits_per_sample = 12;
               return read_raw(in, NEF.picture, ccd_pars);
            }
            else // E-1
            {
               if( IFD->byte_order == IFD->intel_byte_order )
                  ccd_pars.flags |= TCCDParam::fIntelByteOrder;

               ccd_pars.row_length = 2*Width;
               ccd_pars.data_offset = StripOffsets->get_value();

               fseek( in, 0, SEEK_END );
               ccd_pars.file_size = ftell( in );
               ccd_pars.camera = Model ? pack(Model->value) : 1;

               ccd_pars.data_shift = 4;

               int res = read_raw(in, NEF.picture, ccd_pars);

               ccd_pars.bits_per_sample = 12;
               return res;
            }
         }
      }
      return e_error_format;
   }
   else
   if( Compression->get_value() == 0x8799 ) // NIKON D-series compression
   {
      IFDir::Tag* BitsPerSample = IFD->get_tag(TIFF::BitsPerSample);
      if( !BitsPerSample ) return e_error_format;
      if( BitsPerSample->count != 1 ) return e_error_format;

      IFDir::Tag* StripOffsets = IFD->get_tag(TIFF::StripOffsets);
      if( !StripOffsets )
         return e_error_format;

      IFDir::Tag* StripByteCounts = IFD->get_tag(TIFF::StripByteCounts);
      if( !StripByteCounts ) return e_error_format;
      if( StripOffsets->count != StripByteCounts->count ) return e_error_format;

      IFDir::Tag* RowsPerStrip = IFD->get_tag(TIFF::RowsPerStrip);
      if( !RowsPerStrip ) return e_error_format;

      if( !NEF.Is_NIKON() )
         return e_error_format;

      unsigned picture_size = Width*Height;
      ccd_pars.bits_per_sample = BitsPerSample->get_value();

      if( ccd_pars.bits_per_sample != 12 )
         return e_error_format;

      IFDir::Tag* LinearizationTable = NEF.MakerNote.get_tag( 0x96 );
      if( !LinearizationTable ) return e_error_format;
      
      int init_bias = (LinearizationTable->get_value(2) << 8) + LinearizationTable->get_value(3);
      int linear_tab_count = (LinearizationTable->get_value(10) << 8) + LinearizationTable->get_value(11);
      if( linear_tab_count > 0 )
      {
         NEF.linear_tab.alloc( linear_tab_count );
         if( !(word*)NEF.linear_tab ) return e_not_enough_memory;

         for(int i=0; i<linear_tab_count; ++i)
            NEF.linear_tab[i] = (word)((LinearizationTable->get_value(12+i*2) << 8) + LinearizationTable->get_value(13+i*2));
      }

      NEF.picture.alloc(Width*Height);
      if( !(color_t*)NEF.picture ) return e_not_enough_memory;

      fseek(in, StripOffsets->get_value(), SEEK_SET); 

      if( !read_compressed_nikon( in, Width, Height, init_bias, NEF.picture ) )
         return e_unexpected_eof;

      return e_success;
   }
   return e_not_supported;
}

void TIFF_Content::Linearization()
{
   unsigned max_index = linear_tab.size()-1;
   word max_value = linear_tab[max_index];
   unsigned n = ccd_pars.raw_width * ccd_pars.raw_height;
   color_t* p = picture;
   word const* tab = linear_tab;

   for(unsigned i=0; i<n; ++i)
   {
      p[i] = p[i] <= max_index ? tab[ p[i] ] : max_value;
   }
}

int TIFF_Content::safe_read_raw(FILE* in, IFDir* CFA)
{
   try
   {
      int res = read_cfa( in, CFA, *this );

      if( res == e_success && linear_tab.size() > 0 )
         Linearization();

      return res;
   }
   catch(const char* x)
   {
      printf("error: %s\n", x);
   }
   catch(...)
   {
      printf("error: exception\n");
   }

   return e_exception;
}

int TIFF_Content::read_raw(FILE* in, IFDir* CFA)
{
   try
   {
      return safe_read_raw( in, CFA );
   }
   catch(...)
   {
      printf("error: except\n");
   }
   return e_exception;
}



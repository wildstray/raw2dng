#include <stdio.h>
#include <string.h>
#pragma hdrstop

/*
   Coolpix RAW to Coolpix NEF/DNG converter
   by paul69, v0.16.3, 2006-01-16, <e2500@narod.ru>

   Supported camera (see raw2nef.ini)
   Main function

*/

#include <stdlib.h>
#include "tiff.h"
#include "file.h"
#include "raw2nef.h"

extern const char* program_desc;

int main(int argc, char* argv[] )
{
   TRaw2Nef raw2nef;

   raw2nef.parse_args( argc, argv );

   return raw2nef.raw_to_nef( argc, argv );
}

extern const char* default_extension;
extern bool default_compatible;

void usage();

void TRaw2Nef::CreateExifFileName(const char* FilePath, int ExtOffset)
{
   if( !efn_buf )
      efn_buf = new char[_MAX_PATH];

   efn = efn_buf;
   memcpy(efn, FilePath, ExtOffset);
   strcpy(efn+ExtOffset, ".jpg");

   if( same_number )
   {
      int i = ExtOffset-8;
      while( i >= 0 )
      {
         if( (efn[i+4]|32) == 'c'
          && (efn[i+5]|32) == 'i'
          && (efn[i+6]|32) == 'm'
          && (efn[i+7]|32) == 'g'
          && (efn[i+0] >= '0' && efn[i+0] <= '9')
          && (efn[i+1] >= '0' && efn[i+1] <= '9')
          && (efn[i+2] >= '0' && efn[i+2] <= '9')
          && (efn[i+3] >= '0' && efn[i+3] <= '9')
          )
         {
            efn[i+4] = efn[i+0];
            efn[i+5] = efn[i+1];
            efn[i+6] = efn[i+2];
            efn[i+7] = efn[i+3];
            efn[i+0] = 'c';
            efn[i+1] = 'i';
            efn[i+2] = 'm';
            efn[i+3] = 'g';
            return;
         }
         --i;
      }
   }
   else
   {
      char* num = efn+ExtOffset-1;
      for(int i=0; i<8; ++i)
      {
         int d = num[-i];
         if( d < '0' || d > '9' ) break;
         if( d != '9' )
         {
            num[-i] = (char)(d+1);
            break;
         }
         num[-i] = '0';
      }
   }
}

TRaw2Nef::TRaw2Nef()
{
   endian = true;
   compatible = default_compatible;
   copy_exif = true;
   verbose = false;
   incomplete = false;
   optimize = false;
   same_number = false;
   quiet = false;

   append_table = false;
   black_pixels = false;
   bright_pixels = false;
   threashold = 100;

   ini_fn = 0;
   ifn = 0;
   ofn = 0;
   efn = 0;
   dfn = 0;

   ofn_buf = 0;
   efn_buf = 0;
   ini_buf = 0;
   dfn_buf = 0;
}

TRaw2Nef::~TRaw2Nef()
{
   delete [] ofn_buf;
   delete [] efn_buf;
   delete [] ini_buf;
   delete [] dfn_buf;
}

bool TRaw2Nef::ParseIniFile(TCCDParam& ccd_pars, unsigned camera, unsigned file_size)
{
   if( !ini_fn )
   {
      // use standard ini-file name
      // search it in program directory

      const char* eof_path = 0;
      const char* exe_slash = strrchr(exe_fn, '/');
      if( exe_slash )
         eof_path = exe_slash+1;

      const char* exe_bslash = strrchr(exe_fn, '\\');
      if( exe_bslash )
         eof_path = exe_bslash+1;

      ini_fn = "raw2nef.ini";
      
      if( eof_path )
      {
         if( !ini_buf )
            ini_buf = new char[_MAX_PATH];

         int len = eof_path-exe_fn;
         memcpy( ini_buf, exe_fn, len );
         strcpy( ini_buf+len, ini_fn );
         ini_fn = ini_buf;
      }
   }

   TFile ini = fopen( ini_fn, "rt");
   if( !ini )
   {
      printf("error: \"%s\" - cannot open\n", ini_fn); 
      return false;
   }
   else
   if( verbose )
   {
      printf("parse ini-file: \"%s\"\n", ini_fn); 
   }

   const int max_length = 256;
   char line[max_length];

   while( fgets( line, max_length, ini ) )
   {
      TCCDParam pars = {0};
      // field order
      enum
      {
         e_file_size,
         e_data_offset,
         e_raw_width,
         e_raw_height,
         e_cfa_colors,
         e_row_length,
         e_bits_per_sample,
         e_flags,
         e_camera,
      };

      char* field = line;
      int i_field = 0;
      for(int i=0; i<max_length; ++i)
      {
         if( line[i] == '\0' ) break;
         if( line[i] == ' ' )
         {
            line[i] = '\0';
            continue;
         }
         if( line[i] == ',' || line[i] == ';' || line[i] == '\n' )
         {
            bool end_of_line = (line[i] == ';' || line[i] == '\n');

            while( *field == '\0' ) ++field;
            line[i] = '\0';

            switch(i_field)
            {
            case e_file_size: pars.file_size = atoi( field );
               if( camera == 0 && file_size != pars.file_size )
                  goto next_line;
               break;

            case e_data_offset: pars.data_offset = atoi( field ); break;
            case e_raw_width: pars.raw_width = atoi( field ); break;
            case e_raw_height: pars.raw_height = atoi( field ); break;
            case e_row_length: pars.row_length = atoi( field ); break;
            case e_bits_per_sample: pars.bits_per_sample = atoi( field ); break;
            case e_cfa_colors: pars.cfa_colors = atoi( field ); break;
            case e_flags: pars.flags = atoi( field ); break;

            default:
            case e_camera:
               pars.camera = pack( field );
               if( camera == 0 || camera == pars.camera )
               {
                  if( pars.bits_per_sample == 16 )
                     pars.data_shift = 4;

                  ccd_pars = pars;
                  return true;
               }
               break;
            }

            if( end_of_line )
               goto next_line;

            ++i_field;
            field = &line[i+1];
         }
      }

   next_line:;
   }

   return false;
}

bool TRaw2Nef::ProcessFile( unsigned camera )
{
   if( verbose )
      printf("input: %s\n", ifn);

   TIFF_Content TiffContent;

   TFile in = fopen( ifn, "rb");
   if( !in ) { printf("error: \"%s\" - cannot open\n", ifn); return false; }

   if( !ofn || !efn )
   {
      const char* inpFileName = ifn;

      const char* ifn_slash = strrchr(inpFileName, '/');
      if( ifn_slash )
         inpFileName = ifn_slash+1;

      const char* ifn_bslash = strrchr(inpFileName, '\\');
      if( ifn_bslash )
         inpFileName = ifn_bslash+1;

      const char* inpFileExt = strrchr(inpFileName, '.');

      if( !inpFileExt )
         inpFileExt = ifn + strlen(ifn);

      if( !ofn )
      {
         if( !ofn_buf )
            ofn_buf = new char[_MAX_PATH];

         ofn = ofn_buf;
         int len = inpFileExt - ifn;
         memcpy(ofn, ifn, len);
         strcpy(ofn+len, default_extension);
      }

      if( !efn && copy_exif )
      {
         CreateExifFileName(ifn, inpFileExt-ifn);
         if( !efn && verbose )
            printf("warning: cannot get exif filename\n");
      }
   }

   if( TiffContent.read_tiff( in ) )
   {
      if( verbose )
         { printf("read exif info from \"%s\"\n", ifn); }
   }

   if( !TiffContent.EXIF.get_count() )
   {
      if( efn )
      {
         TFile exif = fopen( efn, "rb");

         if( !exif )
         {
            if( verbose )
               { printf("warning: \"%s\" - cannot open exif file\n", efn); }
         }
         else
         {
            if( TiffContent.read_exif( exif ) )
            {
               if( verbose )
                  { printf("read exif info from \"%s\"\n", efn); }
            }
            else
            {
               if( verbose )
                  { printf("warning: \"%s\" - cannot read exif file\n", efn); }
            }
         }
      }

      IFDir::Tag* Model = TiffContent.IFD1.get_tag( TIFF::Model );
      if( Model )
      {
         unsigned exif_camera = ParseExifCameraModel( (const char*)Model->value );

         if( exif_camera != 0 )
         {
            unpack_buff buf1, buf2;

            if( verbose )
               printf("exif camera model is %s\n", unpack(buf1, exif_camera) );

            if( camera == 0 )
               camera = exif_camera;
            else
            if( camera != exif_camera )
            {
               printf("warning: exif model %s and option %s are different\n", unpack(buf1, exif_camera), unpack(buf2, camera) );
            }
         }
      }
   }

   IFDir* CFA = TiffContent.Get_CFA();
   if( CFA )
   {
      if( verbose )
         { printf("read raw data from \"%s\"\n", ifn); }

      int result = TiffContent.read_raw( in, CFA );

      if( result != e_success )
      {
         if( result == e_unexpected_eof && incomplete )
         {
            if( verbose )
               printf("warning: file is incomplete\n");
         }
         else
         {
            if( result == e_unexpected_eof )
               printf("error: unexpected end of raw file\n");
            else
            if( result == e_not_supported )
               printf("error: unsupported file format\n");
            else
            if( result == e_error_format )
               printf("error: unexpected file format\n");
            else
            if( result == e_exception )
               printf("error: unexpected exception\n");
            else
            if( result == e_not_enough_memory )
               printf("error: not enough memory\n");
            else
               printf("error: cannot load raw data\n");

            return false;
         }
      }
   }

   if( !TiffContent.picture )
   {
      TCCDParam& ccd_pars = TiffContent.ccd_pars;
      if( !ccd_pars.camera )
      {
         fseek( in, 0, SEEK_END );
         unsigned file_size = ftell( in );
         rewind( in );

         if( !ParseIniFile( ccd_pars, camera, file_size ) )
         {
            printf("error: unknown format\n");
            return false;
         }
         else
         {
            if( verbose )
            {
               unpack_buff buf;
               printf("camera model %s is found in ini-file\n", unpack( buf, ccd_pars.camera ) );
            }
         }
      }
      
      unsigned picture_size = ccd_pars.raw_height*ccd_pars.raw_width;
      TiffContent.picture.alloc(picture_size);
      color_t* ccd = TiffContent.picture;

      if( !ccd )
      {
         printf("error: memory allocation fault (%u bytes)\n", picture_size*sizeof(color_t));
         return false;
      }

      if( incomplete )
         memset( ccd, 0, sizeof(color_t)*picture_size );

      if( verbose )
         printf("read raw data from \"%s\"\n", ifn);

      int result = read_raw( in, ccd, ccd_pars);

      if( result != e_success )
      {
         if( result == e_unexpected_eof && incomplete )
         {
            if( verbose )
               printf("warning: file is incomplete\n");
         }
         else
         {
            if( result == e_unexpected_eof )
               printf("error: unexpected end of raw file\n");
            else
            if( result == e_not_supported )
               printf("error: unsupported file format\n");
            else
            if( result == e_not_enough_memory )
               printf("error: not enough memory\n");
            else
               printf("error: cannot load raw data\n");

            return false;
         }
      }
   }

   in.close();

   process_data( TiffContent );

   return true;
}

void TRaw2Nef::parse_args(int n, char* a[])
{
   for(int i=1; i<n; ++i)
   {
      if( a[i][0] == '-' )
      {
         switch( a[i][1] )
         {
         case 'v': verbose = (a[i][2] != '-'); break;
         case 'c': compatible = (a[i][2] != '-'); break;
         case 'q': quiet = (a[i][2] != '-'); break;
         default: break;
         }
      }
   }
}

bool TRaw2Nef::raw_to_nef(int n, char* a[])
{
   unsigned camera = 0;
   int i_count = 0;
   bool batch_mode = true;

   if( n <= 1 )
   {
      usage();
      return false;
   }

   exe_fn = a[0];

   int i;
   for(i=1; i<n; ++i)
   {
      if( a[i][0] == '-' )
      {
         switch( a[i][1] )
         {
         case 'v': verbose = (a[i][2] != '-'); break;
         case 'c': compatible = (a[i][2] != '-'); break;
         case 'q': quiet = (a[i][2] != '-'); break;
         case 'n': same_number = (a[i][2] != '-'); break;
         case 'f': incomplete = (a[i][2] != '-'); break;
         case 'O': optimize = (a[i][2] != '-'); break;
         case 'b': bright_pixels = (a[i][2] != '-'); break;
         case 'w': black_pixels = (a[i][2] != '-'); break;
         case 't': threashold = atoi(a[i]+2); break;

         case 'e':
            batch_mode = false;
            if( i+1 < n && a[i+1][0] != '-' )
               efn = a[++i];
            else
               goto missing;
            break;

         case 'o':
            batch_mode = false;
            if( i+1 < n && a[i+1][0] != '-' )
               ofn = a[++i];
            else
               goto missing;
            break;

         case 'd':
            if( i+1 < n && a[i+1][0] != '-' )
            {
               if( a[i][2] == '+' ) append_table = true;
               dfn = a[++i];
            }
            else
               goto missing;
            break;

         case 'i':
            if( i+1 < n && a[i+1][0] != '-' )
               ini_fn = a[++i];
            else
               goto missing;
            break;

         case '?': usage(); return false;

         case 'a': camera = 0; break;

         default:
            if( a[i][1] >= '1' && a[i][1] <= '9' )
            {
               a[i][0] = 'E';
               camera = pack( a[i] );
            }
            else
            if( (a[i][1] >= 'A' && a[i][1] <= 'Z') && a[i][2] != '\0' )
               camera = pack( a[i]+1 );
            else
               printf("warning: unknown switch: %s\n", a[i]);
            break;

//         invalid:
//            printf("warning: invalid switch: %s\n", a[i]);
//            break;

         missing:
            printf("warning: missing argument for %s\n", a[i]);
            break;
         }
      }
      else
      {
         if( !ifn )
            ifn = a[i];

         ++i_count;
      }
   }

   if( verbose )
      printf( program_desc );

   if( !ifn )
   {
      printf("error: input file is missing\n\n");
      return false;
   }

   if( batch_mode && i_count > 1 )
   {
      for(i=1; i<n; ++i)
      {
         if( a[i][0] == '-' )
         {
            switch( a[i][1] )
            {
            case 'o':
            case 'e':
               if( i+1 < n && a[i+1][0] != '-' )
                  ++i;
               break;
            }
         }
         else
         {
            if( a[i][0] )
            {
               ifn = a[i];
               efn = 0;
               ofn = 0;

               if( !ProcessFile( camera ) )
                  printf("cannot process \"%s\"\n\n", a[i]);
            }
         }
      }
   }
   else
   {
      if( !ProcessFile( camera ) )
      {
         printf("cannot process \"%s\"\n", ifn);
         return false;
      }
   }
   return true;
}



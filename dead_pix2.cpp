#include <stdio.h>
#include <string.h>
#pragma hdrstop

/*
   Coolpix RAW to DNG converter
   by paul69, v0.2, 2006-01-23, <e2500@narod.ru>

   Search dead pixels
*/

#include "raw2nef.h"
#include "file.h"

bool find_black_pixels(FILE* out, color_t picture[], unsigned w, unsigned h);
bool find_bright_pixels(FILE* out, int level, color_t picture[], unsigned w, unsigned h);


#define PROGRAM_VERSION "dead_pix" CORE_VERSION
const char* default_extension = ".txt";
bool default_compatible = true;
const char* program = PROGRAM_VERSION;
const char* program_desc = PROGRAM_VERSION ", by paul69 <e2500@narod.ru>\n[compiled: " __DATE__ " " __TIME__ "]\n";

void usage()
{
   printf("Search black and luminous dead pixels\n");
   printf( program_desc );

   printf("\nargs: [options] input_file\n");

   printf("\noptions:\n");
   printf("   -auto - autodetect (default)\n");
   printf("   -<camera id> - see raw2nef.ini\n");

   printf("\n");
   printf("   -v - verbose\n");
   printf("   -b - search luminous pixels on black shot\n");
   printf("   -w - search black pixels on white shot\n");
   printf("   -t100 - bright pixel detection threashold\n");
   printf("   -d <filename> - dead pixels table output file\n");
   printf("   -d+ <filename> - append existing file\n");
   printf("   -i <filename> - ini-file name (default is raw2nef.ini)\n");
}

bool TRaw2Nef::process_data(TIFF_Content& TiffContent)
{
   if( verbose )
      printf("write dead pixels to: \"%s\"\n", dfn);

   if( black_pixels )
   {
      TFile dead_pixels = fopen( dfn, append_table ? "at" : "wt");

      find_black_pixels(dead_pixels, TiffContent.picture,
         TiffContent.ccd_pars.raw_width,
         TiffContent.ccd_pars.raw_height );
   }
   else
   if( bright_pixels )
   {
      TFile out = fopen( dfn, append_table ? "at" : "wt");

      find_bright_pixels(out, threashold, TiffContent.picture, 
         TiffContent.ccd_pars.raw_width,
         TiffContent.ccd_pars.raw_height );
   }

   if( verbose )
      printf("OK\n");

   return true;
}


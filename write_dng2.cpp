#include <stdio.h>
#include <string.h>
#pragma hdrstop

/*
   Coolpix RAW to DNG converter
   by paul69, v0.2, 2006-01-23, <e2500@narod.ru>

   Writting DNG file
*/

#include "tiff.h"
#include "raw2nef.h"
#include "jpeg.h"
#include "file.h"

#define PROGRAM_VERSION "raw2dng" CORE_VERSION

const char* default_extension = ".dng";
bool default_compatible = true;
const char* program = PROGRAM_VERSION;
const char* program_desc = PROGRAM_VERSION ", by paul69 <e2500@narod.ru>\n[compiled: " __DATE__ " " __TIME__ "]\n";
void usage()
{
   printf("Coolpix RAW data to DNG format converter\n");
   printf( program_desc );

   printf("\nargs: [options] input_file1 [input_file2...]\n");

   printf("\noptions:\n");
   printf("   -auto - autodetect (default)\n");
   printf("   -<camera id> - see raw2nef.ini\n");

   printf("\n");
   printf("   -v - verbose\n");
   printf("   -c - compatible with Adobe DNG Converter (default)\n");
   printf("   -n - EXIF/JPEG file has the same number as RAW file\n");
   printf("   -f - force loading incomplete raw files\n");
   printf("   -O - optimize DNG file size (very slow)\n");

   printf("   -e <filename> - EXIF/JPEG file name\n");
   printf("   -o <filename> - DNG output file name\n");
   printf("   -i <filename> - ini-file name (default is raw2nef.ini)\n");
   printf("   With -e and -o options only one input file is allowed\n");
}

bool TRaw2Nef::process_data(TIFF_Content& TiffContent)
{
   if( dfn )
   {
      TFile dead_pixels = fopen( dfn, "rt" );

      if( !dead_pixels ) { 
         printf("error: \"%s\" - cannot open\r\n", dfn); 
         return false; }

      if( verbose )
         printf("mask dead pixels from \"%s\"\n", dfn);

      TiffContent.mask_dead_pixels(dead_pixels);
   }

   if( verbose )
      printf("write picture to: \"%s\"\n", ofn);

   TFile out = fopen( ofn, "wb");
   if( !out ) { printf("error: \"%s\" - can not open\n", ofn); return false; }

   if( !TiffContent.write_dng(out, endian, compatible, optimize) )
      { printf("error: can not write DNG\n"); return false; }

   if( verbose )
      printf("OK\n");

   return true;
}


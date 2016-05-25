#include <stdio.h>
#include <string.h>
#pragma hdrstop

/*
   Coolpix RAW to Coolpix NEF converter
   by paul69, v0.14, 2005-01-23, <e2500@narod.ru>

   Writting NEF file
*/

#include "tiff.h"
#include "file.h"
#include "raw2nef.h"

const char* default_extension = ".nef";
bool default_compatible = false;
#define PROGRAM_VERSION "raw2nef" CORE_VERSION
const char* program = PROGRAM_VERSION;
const char* program_desc = PROGRAM_VERSION ", by paul69 <e2500@narod.ru>\n[compiled: " __DATE__ " " __TIME__ "]\n";
void usage()
{
   printf("Coolpix RAW data to E5400/E5700/E8700 NEF file converter\n");
   printf( program_desc );

   printf("\nargs: [options] input_file1 [input_file2...]\n");

   printf("\noptions:\n");
   printf("   -auto - autodetect (default)\n");
   printf("   -<camera id> - see raw2nef.ini\n");

   printf("\n");
   printf("   -v - verbose\n");
   printf("   -c - compatible with E5400/E5700/E8700\n");
   printf("   -n - EXIF/JPEG file has the same number as RAW file\n");
   printf("   -f - force loading incomplete raw files\n");

   printf("   -e <filename> - EXIF/JPEG file name\n");
   printf("   -o <filename> - NEF output file name\n");
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

   if( !TiffContent.write_nef(out, endian, compatible) )
      { printf("error: can not write NEF\n"); return false; }

   if( verbose )
      printf("OK\n");

   return true;
}


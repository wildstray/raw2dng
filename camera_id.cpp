#include <stdio.h>
#include <string.h>
#pragma hdrstop

/*
   Coolpix RAW to Coolpix NEF/DNG converter
   by paul69, v0.16.1, 2006-01-17, <e2500@narod.ru>

   camera_id

*/

#include <stdlib.h>
#include "raw2nef.h"

unsigned pack( const char* s )
{
   unsigned n = 0;
   for(int i=0; i<6; ++i)
   {
      unsigned char c = (unsigned char)*s++;
      if( c == '-' ) {
         // skip hyphen
         c = (unsigned char)*s++;
      }

      unsigned d;

      if( c >= '0' && c <= '9' ) {
         d = c - '0';
      }
      else
      if( c >= 'A' && c <= 'Z' ) {
         d = 10 + c - 'A';
      }
      else
      if( c >= 'a' && c <= 'z' ) {
         d = 10 + c - 'a';
      }
      else
         break;

      n = n*36+d;
   }
   return n;
}

const char* unpack( unpack_buff& buf, unsigned x )
{
   char* s = &buf.buf[11];
   *(s) = 0;
   do
   {
      unsigned d = x % 36;
      *(--s) = (char)(d > 9 ? 'A'+d-10 : '0'+d);
      x = x / 36;
   } while( x );

   return s;
}

unsigned ParseExifCameraModel( const char* model )
{
   if( model[0] == 'E' || model[0] == 'C' )
      return pack( model );

   if( 0 == memcmp( model, "DiMAGE", 6 ) )
      return pack( model + 7 ); // "Z2"

   if( 0 == memcmp( model, "DMC-", 4 ) )
      return pack( model + 4 ); // "FZ30"

   return 0;
}

#include <stdio.h>
#include <string.h>
#pragma hdrstop

/*
   Coolpix RAW to DNG converter
   by paul69, v0.1, 2006-01-09, <e2500@narod.ru>

   Search dead pixels
*/

#include "raw2nef.h"
#include "file.h"
#include <stdlib.h> // qsort

bool find_black_pixels(FILE* out, color_t picture[], unsigned w, unsigned h)
{
   // find black pixels in gray shot
   color_t* pix = picture;
   unsigned n_points = 0;
   for(unsigned y=0; y<h; ++y, pix += w)
   for(unsigned x=0; x<w; ++x)
   {
      if( pix[x] == 0 )
      {
         ++n_points;
      }
   }

   fprintf( out, "; %u x %u shot\n", w, h);
   fprintf( out, "; %u black points\n", n_points);

   pix = picture;
   for(unsigned y=0; y<h; ++y, pix += w)
   for(unsigned x=0; x<w; ++x)
   {
      if( pix[x] == 0 )
      {
         fprintf( out, "%u %u 0\n", x, y);
      }
   }
   return true;
}
#pragma pack(1)
struct pixel_t
{
   unsigned value;
   unsigned short x, y;
};
#pragma pack()

int compare(const void* a , const void* b )
{
   return ((pixel_t const*)b)->value -((pixel_t const*)a)->value;
}

bool find_bright_pixels(FILE* out, int level, color_t picture[], unsigned w, unsigned h)
{
   const int max_points = w*h;
   buff_t<pixel_t> pixels(max_points);
   int n_points = 0;
   bool sorted = false;
   color_t* pix = picture;

   for(unsigned y=0; y<h; ++y, pix += w)
   for(unsigned x=0; x<w; ++x)
   {
      unsigned vals[8];
      int i_vals = 0;

      color_t* pic = pix + x;
      if( x >= 2 )
      {
         vals[i_vals++] = pic[-2];

         if( y >= 2 )
            vals[i_vals++] = pic[-2 + (-2)*w];

         if( y < h-2 )
            vals[i_vals++] = pic[-2 + (2)*w];
      }
      if( x < w-2 )
      {
         vals[i_vals++] = pic[2];

         if( y >= 2 )
            vals[i_vals++] = pic[2 + (-2)*w];

         if( y < h-2 )
            vals[i_vals++] = pic[2 + (2)*w];
      }

      if( y >= 2 )
         vals[i_vals++] = pic[(-2)*w];

      if( y < h-2 )
         vals[i_vals++] = pic[(2)*w];

      // now have 3, 5, or 8 points around pixel
      for(int j=0; j<1; ++j)
      for(int i=j; i<i_vals; ++i)
      {
         if( vals[j] < vals[i] )
         {
            unsigned temp = vals[i];
            vals[i] = vals[j];
            vals[j] = temp;
         }
      }

      if( (int)(pic[0] - vals[0]) > level )
      {
         pixel_t p = { pix[x], (unsigned short)x, (unsigned short)y };

         if( n_points < max_points )
         {
            pixels[n_points++] = p;
         }
      }
   }

   if( !sorted )
      qsort( (pixel_t*)pixels, n_points, sizeof(pixel_t), compare );

   fprintf( out, "; %u x %u shot\n", w, h);
   fprintf( out, "; %u bright points\n", n_points);
   for(int i=0; i<n_points; ++i)
   {
      pixel_t const& p = pixels[i];

      fprintf( out, "%u %u %u\n", p.x, p.y, p.value);
   }
   return true;
}

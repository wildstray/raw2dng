#include <stdio.h>
#include <string.h>
#pragma hdrstop

/*
   Coolpix RAW to Coolpix NEF/DNG converter
   by paul69, v0.15, 2006-01-09, <e2500@narod.ru>

   Mask dead pixels
*/

#include "raw2nef.h"

struct TPoint
{
   unsigned x, y;
};

void read_dead_pixels(FILE* in,
   void (* callback_proc)(void*, TPoint&), void* callback_param)
{
   char buf[64];
   bool skip = false;

   for(;;)
   {
      if( fgets(buf, sizeof(buf), in ) == 0 ) break;

      int len = strlen(buf);
      if( len == 0 ) break;

      if( !skip )
      {
         if( buf[0] != ';' )
         {
            TPoint pt;
            if( sscanf( buf, "%u %u", &pt.x, &pt.y) == 2 )
            {
               callback_proc( callback_param, pt );
            }
         }
      }

      // if no 'end of line' mark at the end of line
      // skipping next fgets results

      skip = buf[len-1] != '\n';
   }
}

void inc_n_points(void* n_points, TPoint&)
{
   ++(*(unsigned*)n_points);
}

class TPoints
{
public:
   TPoints(int n) : i_point(0), max_point(n), p(n) {}

   int i_point, max_point;
   buff_t<TPoint> p;

   void add( TPoint const& pt )
   {
      if( i_point < max_point )
         p[ i_point++ ] = pt;
   }
};

void add_point(void* points, TPoint& pt)
{
   ((TPoints*)points)->add( pt );
}


bool TIFF_Content::mask_dead_pixels(FILE* in)
{
   unsigned n_points = 0;

   read_dead_pixels( in, inc_n_points, &n_points );

   if( !n_points )
      return true;

   rewind( in );

   TPoints points( n_points );
   read_dead_pixels( in, add_point, &points );

   unsigned W = ccd_pars.raw_width;
   unsigned H = ccd_pars.raw_height;

   for(int i_point=0; i_point<points.i_point; ++i_point)
   {
      TPoint pt = points.p[i_point];
      unsigned vals[8];
      int i_vals = 0;

      if( pt.x >= W ) continue;
      if( pt.y >= H ) continue;

      color_t* pic = &picture[pt.x + pt.y*W];

      if( pt.x >= 2 )
      {
         vals[i_vals++] = pic[-2];

         if( pt.y >= 2 )
            vals[i_vals++] = pic[-2 + (-2)*W];

         if( pt.y < H-2 )
            vals[i_vals++] = pic[-2 + (2)*W];
      }
      if( pt.x < W-2 )
      {
         vals[i_vals++] = pic[2];

         if( pt.y >= 2 )
            vals[i_vals++] = pic[2 + (-2)*W];

         if( pt.y < H-2 )
            vals[i_vals++] = pic[2 + (2)*W];
      }

      if( pt.y >= 2 )
         vals[i_vals++] = pic[(-2)*W];

      if( pt.y < H-2 )
         vals[i_vals++] = pic[(2)*W];

      // now have 3, 5, or 8 points around dead pixel
      for(int i=1; i<i_vals; ++i)
      {
         for(int j=0; j<i; ++j)
         {
            if( vals[i] < vals[j] )
            {
               unsigned temp = vals[i];
               vals[i] = vals[j];
               vals[j] = temp;
            }
         }
      }

      if( i_vals == 8 )
         pic[0] = (color_t)((vals[i_vals/2] + vals[i_vals/2-1])/2);
      else
         pic[0] = (color_t)vals[i_vals/2];
   }

   return true;
}


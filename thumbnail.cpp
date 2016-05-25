#include <stdio.h>
#include <string.h>
#pragma hdrstop

/*
   Coolpix RAW to Coolpix NEF/DNG converter
   by paul69, v0.01, 2006-01-26, <e2500@narod.ru>

   Writting thumbnail
*/

#include <math.h> // sqrt
#include "raw2nef.h"

static
unsigned cfa_pattern[8] = { 
   0x14530, 0x41350, 0x53140, 0x35410, 0x21100, 0x12010, 0x10210, 0x01120 };

bool ccd_interpolation(color_t const ccd[], color_t image[], int w, int h, 
   TCCDParam const& ccd_pars )
{
   enum { Gr, Mg, Cy, Ye };
   enum { R, G, B };

   int cfa[4]; // index of color filter

   unsigned cfa_colors = ccd_pars.cfa_colors;

   if( cfa_colors < 8 )
      cfa_colors |= cfa_pattern[ cfa_colors ];

   if( cfa_colors & TCCDParam::PrimaryColorsBit )
   {
      for(int i=0; i<4; ++i)
      {
         switch( (cfa_colors >> (16 - 4*i)) & 15 )
         {
         case 0: cfa[i] = R; break;
         case 1: cfa[i] = G; break;
         case 2: cfa[i] = B; break;
         default: return false;
         }
      }
   }
   else
   {
      for(int i=0; i<4; ++i)
      {
         switch( (cfa_colors >> (16 - 4*i)) & 15 )
         {
         case 1: cfa[i] = Gr; break;
         case 4: cfa[i] = Mg; break;
         case 3: cfa[i] = Cy; break;
         case 5: cfa[i] = Ye; break;
         default: return false;
         }
      }
   }

   int height = ccd_pars.raw_height;
   int width = ccd_pars.raw_width;

   int dx = width/w;
   int dy = height/h;

   color_t* out = image;

   for(int y=0; y<h; ++y)
   {
      int _y = y * dy;
      for(int x=0; x<w; ++x, out += 3)
      {
         int c[4];

         int _x = x * dx;
         color_t const* in = &ccd[_y*width + _x];

         // index in CFA
         int i = (_x&1) | ((_y&1)<<1);
         c[ cfa[i] ] = in[0];
         c[ cfa[i^1] ] = in[1];
         c[ cfa[i^2] ] = in[width];
         c[ cfa[i^3] ] = in[width+1];

         int RR, GG, BB;
         if( ccd_pars.cfa_colors & TCCDParam::PrimaryColorsBit )
         {
            RR = (int)c[R];
               if( RR < 0 ) RR=0; if( RR > 65535 ) RR = 65535;

            GG = (int)c[G];
               if( GG < 0 ) GG=0; if( GG > 65535 ) GG = 65535;

            BB = (int)c[B];
               if( BB < 0 ) BB=0; if( BB > 65535 ) BB = 65535;
         }
         else
         {
            float Y = (float)((c[Gr] + c[Mg] + c[Ye] + c[Cy])/4);
            float Cr = (float)((c[Mg] + c[Ye]) - (c[Gr] + c[Cy]));
            float Cb = (float)((c[Mg] + c[Cy]) - (c[Gr] + c[Ye]));

            RR = (int)(Y + 1.402f*Cr);
               if( RR < 0 ) RR=0; if( RR > 65535 ) RR = 65535;

            GG = (int)(Y - 0.34414f*Cb - 0.71414f*Cr);
               if( GG < 0 ) GG=0; if( GG > 65535 ) GG = 65535;

            BB = (int)(Y + 1.772f*Cb);
               if( BB < 0 ) BB=0; if( BB > 65535 ) BB = 65535;
         }

         out[ R ] = (color_t)RR;
         out[ G ] = (color_t)GG;
         out[ B ] = (color_t)BB;
      }
   }
   return true;
}

bool write_thumbnail(FILE* out, color_t const ccd[], int w, int h, 
   TCCDParam const& ccd_pars)
{
   int size = w*h;
   buff_t<color_t> tn(size*3);

   ccd_interpolation( ccd, tn, w, h, ccd_pars);

   int max_color[3];
   max_color[0]=max_color[1]=max_color[2]=0;
   int min_color[3];
   min_color[0]=min_color[1]=min_color[2]=65536;

   for(int i=0; i<size; ++i)
   {
      for(int j=0; j<3; ++j)
      {
         int pixel = tn[i*3+j];
         if( pixel > max_color[j] )
            max_color[j] = pixel;
         if( pixel < min_color[j] )
            min_color[j] = pixel;
      }
   }

   if( max_color[0] == 0 ) max_color[0] = 65535;
   if( max_color[1] == 0 ) max_color[1] = 65535;
   if( max_color[2] == 0 ) max_color[2] = 65535;

   if( min_color[0] == max_color[0] ) min_color[0] = 0;
   if( min_color[1] == max_color[1] ) min_color[1] = 0;
   if( min_color[2] == max_color[2] ) min_color[2] = 0;

   for(int i=0; i<size; ++i)
   {
      for(int j=0; j<3; ++j)
      {
         int pixel = tn[i*3+j];

         if( max_color[j] > 0 )
         {
            double temp = (double)(pixel - min_color[j])/(double)(max_color[j] - min_color[j]);
            temp = temp > 0 ? sqrt(temp) : 0.;
            pixel = (int)(temp*300);
         }

         if( pixel > 255 ) pixel = 255;
         fputc(pixel, out);
      }
   }
   return true;
}

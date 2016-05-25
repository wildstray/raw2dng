#include <stdio.h>
#include <string.h>
#pragma hdrstop

/*
   Coolpix RAW to Coolpix NEF/DNG converter
   by paul69, v0.14, 2005-12-20, <e2500@narod.ru>

   Writting TIFF IFD file
*/

#include "tiff.h"
#include "raw2nef.h"

bool write(IFDir& ifd, FILE* out, bool endian, unsigned tiff_start, unsigned next_ifd)
{
   unsigned size = ifd.get_size();
   char* buffer = new char[size];

   bool ret = false;

   unsigned ifd_start = ftell(out) - tiff_start;

   if( ifd.write_to_buffer( buffer, size, endian, ifd_start, next_ifd) )
   {
      ret = fwrite( buffer, size, 1, out) == 1;
   }
   delete [] buffer;
   return ret;
}

bool write_word(FILE* out, bool endian, unsigned w)
{
   if( endian )
   {
      if( fputc( (int)(w & 0xFF), out) == EOF ) return false;
      return fputc( (int)((w >> 8)&0xFF), out) != EOF;
   }

   if( fputc( (int)((w >> 8)&0xFF), out) == EOF ) return false;
   return fputc( (int)(w & 0xFF), out) != EOF;
}

bool write_dword(FILE* out, bool endian, unsigned dw)
{
   if( endian )
   {
      if( fputc( (int)((dw) & 0xFF), out) == EOF ) return false;
      if( fputc( (int)((dw >> 8) & 0xFF), out) == EOF ) return false;
      if( fputc( (int)((dw >> 16) & 0xFF), out) == EOF ) return false;
      return fputc( (int)((dw >> 24) & 0xFF), out) != EOF;
   }

   if( fputc( (int)((dw >> 24) & 0xFF), out) == EOF ) return false;
   if( fputc( (int)((dw >> 16) & 0xFF), out) == EOF ) return false;
   if( fputc( (int)((dw >> 8) & 0xFF), out) == EOF ) return false;
   return fputc( (int)((dw) & 0xFF), out) != EOF;
}


////////////////////////////////////////////////////////////////////////////////
// Writting IFD to buffer

class TOut
{
public:
   TOut( char* buf, int size, bool endian ) : buf_(buf), size_(size), off_(0), endian_(endian) {}

   char* buf_;
   int size_;
   int off_;
   bool endian_;

public:
   void put(int c)
   {
      if( off_ < size_ )
      {
         buf_[off_++] = (char)c;
      }
   }

   bool is_place() { return off_ < size_; }
   bool is_place(int n) { return off_+n <= size_; }

   bool write_byte(unsigned v);
   bool write_word(unsigned w);
   bool write_dword(unsigned dw);
};

bool TOut::write_byte(unsigned v)
{
   if( !is_place(1) ) return false;

   put((int)v);

   return  true;
}

bool TOut::write_word(unsigned w)
{
   if( !is_place(2) ) return false;

   if( endian_ )
   {
      put( (int)(w & 0xFF) );
      put( (int)((w >> 8)&0xFF) );
      return true;
   }

   put( (int)((w >> 8)&0xFF) );
   put( (int)(w & 0xFF) );
   return true;
}

bool TOut::write_dword(unsigned dw)
{
   if( !is_place(4) ) return false;

   if( endian_ )
   {
      put( (int)((dw) & 0xFF) );
      put( (int)((dw >> 8) & 0xFF) );
      put( (int)((dw >> 16) & 0xFF) );
      put( (int)((dw >> 24) & 0xFF) );
      return true;
   }

   put( (int)((dw >> 24) & 0xFF) );
   put( (int)((dw >> 16) & 0xFF) );
   put( (int)((dw >> 8) & 0xFF) );
   put( (int)((dw) & 0xFF) );
   return true;
}

bool IFDir::write_to_buffer( char* buf, int size, bool endian, unsigned ifd_start, unsigned next_ifd)
{
   TOut out( buf, size, endian );
   
   if( !out.write_word(tag_count_) )
      return false;

   int i, k;
   unsigned data_offset = ifd_start + 2 + 12*tag_count_ + 4;

   for(i=0; i<tag_count_; ++i)
   {
      Tag* t = array_[i];

      out.write_word(t->tag);
      out.write_word(t->type);
      out.write_dword(t->count);

      unsigned tag_data_size = t->get_size();
      if( tag_data_size > 4 )
      {
         out.write_dword(data_offset);
         data_offset += ((tag_data_size-1)|1)+1;
      }
      else
      {
         switch(t->type)
         {
         case TIFF::type_BYTE:
         case TIFF::type_SBYTE:
         case TIFF::type_ASCII:
         case TIFF::type_UNDEFINED:
            for(k=0; k<4; ++k)
            {
               out.write_byte(k < t->count ? ((byte*)t->value)[k] : (byte)0 );
            }
            break;

         case TIFF::type_WORD:
         case TIFF::type_SHORT:
            for(k=0; k<2; ++k)
            {
               out.write_word(k < t->count ? ((word*)t->value)[k] : (word)0 );
            }
            break;

         case TIFF::type_DWORD:
         case TIFF::type_LONG:
         case TIFF::type_FLOAT:
            out.write_dword(*(dword*)t->value);
            break;
         }
      }
   }

   out.write_dword(next_ifd);

   for(i=0; i<tag_count_; ++i)
   {
      Tag* t = array_[i];

      unsigned tag_data_size = t->get_size();
      if( tag_data_size > 4 )
      {
         switch(t->type)
         {
         case TIFF::type_BYTE:
         case TIFF::type_SBYTE:
         case TIFF::type_ASCII:
         case TIFF::type_UNDEFINED:
            for(k=0; k<t->count; ++k)
            {
               out.write_byte(((byte*)t->value)[k]);
            }
            if( t->count & 1 )
               out.write_byte(0);
            break;

         case TIFF::type_WORD:
         case TIFF::type_SHORT:
            for(k=0; k<t->count; ++k)
            {
               out.write_word(((word*)t->value)[k]);
            }
            break;

         case TIFF::type_DWORD:
         case TIFF::type_LONG:
         case TIFF::type_FLOAT:
            for(k=0; k<t->count; ++k)
            {
               out.write_dword(((dword*)t->value)[k]);
            }
            break;

         case TIFF::type_RATIONAL:
         case TIFF::type_SRATIONAL:
            for(k=0; k<t->count; ++k)
            {
               out.write_dword(((rational*)t->value)[k].n);
               out.write_dword(((rational*)t->value)[k].d);
            }
            break;
         }
      }
   }
   return true;
}


#include <stdio.h>
#include <string.h>
#pragma hdrstop
/*
   Coolpix RAW to Coolpix NEF/DNG converter
   by paul69, v0.04, 2003-01-26, <e2500@narod.ru>

   TIFF: Image File Directory management
*/

#include "tiff.h"

int TIFF::sizeof_value[n_types] = {0,1,1,2,4,8,1,1,2,4,8,4};


IFDir::~IFDir()
{
   for(int i=0; i<tag_count_; ++i)
      { delete array_[i]; }
   delete [] array_;
}

void IFDir::free()
{
   swap( IFDir() );
}

IFDir::IFDir(IFDir const& a) : tag_count_(0), array_(0), arr_size_(0),
      byte_order(a.byte_order) 
{
   arr_size_ = a.tag_count_ + 16;
   array_ = new Tag*[arr_size_];

   int i;
   for(i=0; i<a.tag_count_; ++i)
   {
      array_[i] = 0;

      Tag* tag = a.array_[i];

      if( tag )
         array_[i] = new Tag(tag->tag, tag->type, tag->count, tag->value);
   }

   tag_count_ = i;

   for(i; i<arr_size_; ++i)
      array_[i] = 0;
}

IFDir& IFDir::operator=(IFDir const& a)
{
   if( this == &a ) return *this;

   swap( IFDir(a) );

   return *this;
}

IFDir::Tag::Tag(int _tag, int _type, int _count, const void* _value)
   : tag(_tag), type(_type), count(_count), value(NULL)
{
   int size = get_size();
   value = new char[size];

   if( _value )
      memcpy( value, _value, size );
   else
      memset( value, 0, size );
}

unsigned IFDir::Tag::get_value(int i) const
{
   if( !this )
      return 0;

   if( i >= count )
      return 0;

   switch( type )
   {
   case TIFF::type_BYTE:
   case TIFF::type_ASCII:
   case TIFF::type_UNDEFINED:
      return ((unsigned char*)value)[i];

   case TIFF::type_SBYTE:
      return (int)(((signed char*)value)[i]);

   case TIFF::type_WORD:
      return ((unsigned short*)value)[i];

   case TIFF::type_SHORT:
      return (int)(((short*)value)[i]);

   case TIFF::type_DWORD:
   case TIFF::type_LONG:
      return ((unsigned long*)value)[i];
      
   default:;
   }
   return 0;
}

bool IFDir::remove(int tag)
{
   int i;
   for(i=0; i<tag_count_; ++i)
   {
      if( array_[i]->tag == tag )
      {
         for(int k=i+1; k<tag_count_; ++k)
            array_[k-1] = array_[k];
         --tag_count_;
         return true;
      }
   }
   return false;
}

IFDir::Tag* IFDir::copy_tag(IFDir::Tag* tag, bool combine)
{
   if( !tag ) return tag;

   return add_tag(tag->tag, tag->type, tag->count, tag->value, combine);
}

IFDir::Tag* IFDir::add_tag(int tag, int type, int count, const void* value, bool combine)
{
   if( type >= TIFF::n_types || count == 0 )
      return NULL;

   int i;
   for(i=0; i<tag_count_; ++i)
   {
      if( array_[i]->tag == tag )
      {
         Tag* old_tag = array_[i];
         if( combine )
         {
            if( old_tag->type == type )
            {
               array_[i] = new Tag(tag, type, old_tag->count+count, 0);
               int size = count * TIFF::sizeof_value[type];
               if( value )
                  memcpy( array_[i]->value, value, size );
               else
                  memset( array_[i]->value, 0, size );

               memcpy( array_[i]->value + size, old_tag->value, old_tag->get_size() );
               delete old_tag;
               return array_[i];
            }
         }

         array_[i] = new Tag(tag, type, count, value);
         delete old_tag;
         return array_[i];
      }
      if( array_[i]->tag > tag )
         break;
   }

   if( tag_count_ == arr_size_ )
   {
      int new_size = arr_size_ + 64;
      Tag** temp = array_;
      array_ = new Tag*[new_size];

      if( !array_ )
      {
         array_ = temp;
         return NULL;
      }

      arr_size_ = new_size;

      if( tag_count_ )
         memcpy( array_, temp, sizeof(array_[0])*tag_count_ );

      delete [] temp;
   }

   if( i < tag_count_ )
   {
      memmove( array_+i+1, array_+i, sizeof(array_[0])*(tag_count_-i) );
   }

   array_[i] = new Tag(tag, type, count, value);

   ++tag_count_;
   return array_[i];
}

unsigned IFDir::get_size()
{
   unsigned size = 2 + 12*tag_count_ + 4;
   for(int i=0; i<tag_count_; ++i)
   {
      Tag* t = array_[i];
      unsigned tag_data_size = t->get_size();
      if( tag_data_size > 4 )
         size += ((tag_data_size-1)|1)+1;
   }
   return size;
}

IFDir::Tag* IFDir::get_tag(word tag)
{
   for(int i=0; i<tag_count_; ++i)
   {
      Tag* t = array_[i];

      if( t->tag == tag )
         return t;
   }
   return NULL;
}

unsigned IFDir::get_tag_offset(word tag)
{
   unsigned offset = 2 + 12*tag_count_ + 4;
   for(int i=0; i<tag_count_; ++i)
   {
      Tag* t = array_[i];
      unsigned tag_data_size = t->get_size();

      if( t->tag == tag )
      {
         if( tag_data_size > 4 )
            return offset;

         return 2 + 12*i + 2+2+4; // tag+type+size
      }

      if( tag_data_size > 4 )
         offset += tag_data_size;
   }
   return 0;
}


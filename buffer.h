#ifndef __BUFFER_H
#define __BUFFER_H

#ifndef __SWAP_H
#include "swap.h"
#endif

template <class T>
class buff_t
{
public:
   buff_t() : buff_(0), size_(0) {}
   buff_t(unsigned size) : buff_( new T[size]), size_(size) {}
   ~buff_t() { delete [] buff_; }

   buff_t(buff_t const& a) : buff_( new T[a.size_]), size_(a.size_)
   {
      memcpy(buff_, a.buff_, sizeof(T)*size_);
   }
   buff_t& operator=(buff_t const& a)
   {
      if( &a == this ) return *this;
      alloc(a.size_);
      memcpy(buff_, a.buff_, sizeof(T)*size_);
      return *this;
   }

   void alloc(unsigned size) { delete [] buff_; buff_ = new T[size_=size]; }

   operator T* () const { return buff_; }
   unsigned size() const { return size_; }

   T* buff_;
   unsigned size_;

   void swap(buff_t& a)
   {
      ::swap(buff_, a.buff_);
      ::swap(size_, a.size_);
   }
};
#endif
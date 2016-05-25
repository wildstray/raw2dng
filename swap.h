#ifndef __SWAP_H
#define __SWAP_H

template <class T> inline void swap(T& a, T& b) { T t(a); a = b; b = t; }

#endif
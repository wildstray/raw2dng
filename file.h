#ifndef __FILE_H
#define __FILE_H

#ifndef MAX_PATH
#include <limits.h>
#define MAX_PATH PATH_MAX
#define _MAX_PATH PATH_MAX
#endif//MAX_PATH

class TFile
{
public:
   TFile(FILE* p=NULL) : fp(p) {}
   ~TFile() { if(fp) fclose(fp); }

   FILE* fp;
   operator FILE* () { return fp; }

   void operator = (FILE* p) { if(fp) fclose(fp); fp = p; }
   void close() { if(fp) fclose(fp); fp=NULL; }
};

inline int fgetw(FILE* in)
{
   int h = fgetc(in);
   return (h << 8)|fgetc(in);
}

#endif//__FILE_H

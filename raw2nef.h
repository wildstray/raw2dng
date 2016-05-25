#ifndef __RAW2NEF_H
#define __RAW2NEF_H
//
#ifndef __TIFF_H
#include "tiff.h"
#endif

#ifndef __BUFFER_H
#include "buffer.h"
#endif

#ifdef UNICODE
#define CORE_VERSION L" v0.16.6.2u"
#else
#define CORE_VERSION " v0.16.6.2"
#endif

#ifdef _MSC_VER
#include <excpt.h>
#define try __try
#define catch(...) __except(1)
#endif

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;
typedef unsigned short color_t;

struct unpack_buff { char buf[12]; };
const char* unpack( unpack_buff& buf, unsigned x );
unsigned pack( const char* s );
unsigned ParseExifCameraModel( const char* model );

#define CAMERA_ID(id) pack(#id)

struct TCCDParam
{
   unsigned raw_width;
   unsigned raw_height;
   unsigned file_size;
   unsigned cfa_colors;
   unsigned row_length;
   unsigned bits_per_sample;
   unsigned camera;
   unsigned data_offset;
   int data_shift;  
   enum
   {
      GMYC, MGCY, YCGM, CYMG,
      BGGR, GBRG, GRBG, RGGB,
      ShiftRightBit = 1,
      ShiftDownBit = 2,
      PrimaryColorsBit = 4,
   };
   enum
   {
      // external flags (can used in ini-file)
      fInterlaced       = 0x00000001,
      fARM              = 0x00000002,
      fIntelByteOrder   = 0x00000004,
      // internal flags
      f10PixPer128bits  = 0x80000000,
   };

   unsigned flags;
};

enum EReadResult
{
   e_success,
   e_unexpected_eof, // incomplete file ?
   e_too_large_file, // bad format description ?
   e_not_supported, // format not supported
   e_error_format, // unexpected format
   e_exception, // unexpected error in reading
   e_not_enough_memory,
};

int read_raw(FILE* in, color_t ccd[], TCCDParam const& ccd_pars);


class TIFF_Content
{
public:
   TIFF_Content() 
   {
      ccd_pars.camera = 0;
      ccd_pars.flags = 0;
      ccd_pars.raw_width = 0;
      ccd_pars.raw_height = 0;
      is_COOLPIX_NEF = 0;
   }
   void free();

   enum {
      const_MaxSubIFD = 4,
   };

   IFDir IFD1; // first in TIFF
   IFDir EXIF;
   IFDir MakerNote;
   IFDir SubIFD[const_MaxSubIFD];

   bool RestoreOriginalModel();
   int is_COOLPIX_NEF;

   TCCDParam ccd_pars;
   buff_t<color_t> picture;

   buff_t<word> linear_tab;

   bool Is_SONY();
   bool Is_NIKON();
   bool Is_PENTAX();
   bool Is_OLYMPUS();
   bool Is_MINOLTA();

   IFDir* Get_CFA();
   IFDir* Init_CFA();

   bool read_exif(FILE*);
   bool read_tiff(FILE*);
   int read_raw(FILE*, IFDir*);

   bool write_nef(FILE*, bool, bool=false);
   bool write_dng(FILE*, bool, bool=true, bool=false);

   bool mask_dead_pixels(FILE*);

   void Linearization(); // apply linear_tab

private:
   bool safe_read_exif(FILE*);
   bool safe_read_tiff(FILE*);
   int safe_read_raw(FILE*, IFDir*);
};

struct TRaw2Nef
{
   TRaw2Nef();
   ~TRaw2Nef();

   char* ini_fn; // ini-file name
   char* ifn; // input RAW file name
   char* ofn; // output NEF file name
   char* efn; // EXIF file name
   char* dfn; // dead pixels map file name

   char* exe_fn; // this program path and filename

   char* ofn_buf;
   char* efn_buf;
   char* ini_buf;
   char* dfn_buf;

   bool verbose;
   bool copy_exif; // copy EXIF metadata from paired JPEG to NEF
   bool endian;   // endian: true for Intel, false for Motorola
   bool compatible;
   bool quiet; // no any message box
   bool optimize; // try to minimize DNG file size (for comparing)
   bool incomplete; // allows load incomplete raw files
   bool same_number; // EXIF file has the same number as RAW file

   bool black_pixels;
   bool bright_pixels;
   bool append_table;
   int threashold;

   bool ProcessFile(unsigned camera);
   bool ParseIniFile(TCCDParam& ccd_pars, unsigned camera, unsigned file_size);
   void CreateExifFileName(const char* FilePath, int ExtOffset);

   void parse_args(int n, char* a[]);
   bool raw_to_nef(int n, char* a[]);
   bool process_data(TIFF_Content& );
};

#endif

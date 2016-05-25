#ifndef __TIFF_H
#define __TIFF_H

#ifndef __SWAP_H
#include "swap.h"
#endif

struct TIFF
{
   enum type_of_data_in_TIFF
   {
      type_BYTE = 1, type_ASCII, type_WORD, type_DWORD, type_RATIONAL,
      type_SBYTE, type_UNDEFINED, type_SHORT, type_LONG, type_SRATIONAL,
      type_FLOAT, /* type_DOUBLE, - unimplemented */
      n_types
   };

   enum used_tags
   {
      // standard TIFF tags
      NewSubfile = 0x00FE,
      ImageWidth = 0x0100,
      ImageHeight = 0x0101,
      BitsPerSample = 0x0102,
      Compression = 0x0103,
      PhotometricInterpretation = 0x0106,
      ImageDescription = 0x010E,
      Make = 0x010F,
      Model = 0x0110,
      StripOffsets = 0x0111,
      Orientation = 0x0112,
      SamplesPerPixel = 0x0115,
      RowsPerStrip = 0x0116,
      StripByteCounts = 0x0117,
      XResolution = 0x011A,
      YResolution = 0x011B,
      PlanarConfiguration = 0x011C,
      ResolutionUnit = 0x0128,
      Software = 0x0131,
      DateTime = 0x0132,

      TileWidth = 0x142,
      TileLength = 0x143,
      TileOffsets = 0x144,
      TileByteCounts = 0x145,

      SubIDFs = 0x14A,

      ReferenceBlackWhite = 0x0214,
      DateTimeOriginal = 0x9003,
      DateTimeDigitized = 0x9004,
      Copyright = 0x8298,

      CFARepeatPatternDim = 0x828D,
      CFAPattern = 0x828E,

      ExposureTime = 0x829A,
      F_Number = 0x829D,
      ExposureProgram = 0x8822,
      ExifVersion = 0x9000,
      ExposureBias = 0x9204,
      MaxApertureValue = 0x9205,
      MeteringMode = 0x9207,
      FocalLength = 0x920A,
      TiffSensingMethod = 0x9217,
      MakerNote = 0x927C,
      UserComment = 0x9286,
      SubSecTime = 0x9290,
      SubSecTimeOriginal = 0x9291,
      SubSecTimeDigitized = 0x9292,
      ExifSensingMethod = 0xA217,
      FileSource = 0xA300,
      SceneType = 0xA301,
      ColorFilterArrayPattern = 0xA302,

      DNGVersion = 0xC612,
      DNGBackwardVersion = 0xC613,
      AnalogBalance = 0xC627,
   };

   static int sizeof_value[n_types];
};

typedef unsigned long dword;
typedef unsigned short word;

struct rational { dword n, d; };
struct Rational : rational { Rational(int a, int b) {n=a; d=b;} };

class IFDir
{
public:
   IFDir() : tag_count_(0), array_(0), arr_size_(0),
      byte_order(unknown_byte_order) {}

   IFDir(IFDir const&);
   IFDir& operator=(IFDir const&);

public:
   ~IFDir();

   void free();

   void swap( IFDir a )
   {
      ::swap(tag_count_, a.tag_count_);
      ::swap(array_, a.array_);
      ::swap(arr_size_, a.arr_size_);
      ::swap(byte_order, a.byte_order);
   }

   struct Tag
   {
      int tag;
      int type;
      int count;
      char* value;

      Tag(int _tag, int _type, int _count, const void* _value);
      ~Tag() { delete [] value; }

      unsigned get_value(int i=0) const;
      int get_size() { return count * TIFF::sizeof_value[type]; }
   };

   Tag* add_tag(int tag, int type, int count, const void* value, bool combine=false);
   Tag* copy_tag(Tag* tag, bool combine=false);

   Tag* add_WORD(int tag, word value) { return add_tag(tag, TIFF::type_WORD, 1, &value); }
   Tag* add_DWORD(int tag, dword value) { return add_tag(tag, TIFF::type_DWORD, 1, &value); }
   Tag* add_RATIONAL(int tag, rational value) { return add_tag(tag, TIFF::type_RATIONAL, 1, &value); }
   Tag* add_SRATIONAL(int tag, rational value) { return add_tag(tag, TIFF::type_SRATIONAL, 1, &value); }
   Tag* add_ASCII(int tag, const char* value) { return add_tag(tag, TIFF::type_ASCII, strlen(value)+1, value); }
   Tag* combine_ASCII(int tag, const char* value) { return add_tag(tag, TIFF::type_ASCII, strlen(value)+1, value, true); }
   Tag* add_ASCII(int tag, const char* value, int len) { return add_tag(tag, TIFF::type_ASCII, len, value); }

   bool remove(int tag);

   int get_count() const { return tag_count_; }
   unsigned get_size();
   unsigned get_tag_offset(word tag);
   Tag* get_tag(word tag);

   // in write_ifd.cpp
   bool write_to_buffer( char* buf, int size, bool endian, unsigned ifd_offset, unsigned next_ifd);

public:
   int tag_count_;
   Tag** array_;
   int arr_size_;
   enum {
      unknown_byte_order,
      intel_byte_order,
      motorola_byte_order,
    };
   int byte_order; //
};

#endif//TIFF_H

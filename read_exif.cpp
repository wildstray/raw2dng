#include <stdio.h>
#include <string.h>
#pragma hdrstop

/*
   Coolpix RAW to Coolpix NEF/DNG converter
   by paul69, v0.05, 2006-01-11, <e2500@narod.ru>

   Reading EXIF JPEG/TIFF file
   Supports
      Adobe DNG
      Nikon NEF
      Olympus ORF
      Panasonic RAW
      SONY SR2
*/

#include "tiff.h"
#include "file.h"
#include "raw2nef.h"

////////////////////////////////////////////////////////////////////////////////

struct TIFF_ID
{
   unsigned short tag;
   unsigned short type;
   unsigned count;
   unsigned offset;
};

class TiffReader;

typedef unsigned char byte;
typedef bool (*TParser)(TiffReader&,TIFF_ID&,TIFF_Content&);
unsigned get_intel_dword(TiffReader& tiff);
unsigned get_motorola_dword(TiffReader& tiff);

class TiffReader
{
public:
   TiffReader(FILE* _file, unsigned offset) :
      file(_file), m_offset(offset),
      binary_format(unknown_format),
      tiff_format(undefined_tiff) {}

   FILE* file;
   unsigned m_offset;

   unsigned (*_get_word)(TiffReader&);
   unsigned (*_get_dword)(TiffReader&);
   enum { unknown_format, format_intel, format_motorola } binary_format;
   enum { undefined_tiff, standard_tiff, panasonic_raw, olympus_orf, minolta_mrw } tiff_format;

   void seek(unsigned _i) { fseek(file, _i + m_offset, SEEK_SET); }
   unsigned tell() const { return ftell(file) - m_offset; }
   void skip(unsigned _i) { fseek(file, _i, SEEK_CUR); }

   void error();
   bool tiff_init();

   void read(void* buf, int sz)
   {
      if( fread( buf, sz, 1, file) != 1 )
         error();
   }

   byte get_byte()
   {
      int c = fgetc(file);
      if( c == EOF )
         error();
      return (byte)c;
   }
   unsigned get_word() { return _get_word(*this); }
   unsigned get_dword() { return _get_dword(*this); }
   unsigned get_dword_mm() { return get_motorola_dword(*this); }
   unsigned get_dword_ii() { return get_intel_dword(*this); }

   void parse_tags(TParser, IFDir*, TIFF_Content&);
   void safe_parse_tags(TParser, IFDir*, TIFF_Content&);
   
   unsigned mrw_data_offset;
};

class TBuffer
{
public:
   TBuffer(unsigned sz) : size(sz), buff(new char[sz]) {}
   ~TBuffer() { delete [] buff; }

   operator char* () { return buff; }
   char& operator [] (int i) { return buff[i]; }

   char* buff;
   unsigned size;
};


unsigned get_intel_dword(TiffReader& tiff)
{
   unsigned dw = tiff.get_byte();
   dw |= tiff.get_byte() << 8;
   dw |= tiff.get_byte() << 16;
   dw |= tiff.get_byte() << 24;
   return dw;
}
unsigned get_intel_word(TiffReader& tiff)
{
   unsigned dw = tiff.get_byte();
   dw |= tiff.get_byte() << 8;
   return dw;
}

unsigned get_motorola_dword(TiffReader& tiff)
{
   unsigned dw = tiff.get_byte()<<24;
   dw |= tiff.get_byte() << 16;
   dw |= tiff.get_byte() << 8;
   dw |= tiff.get_byte();
   return dw;
}

unsigned get_motorola_word(TiffReader& tiff)
{
   unsigned dw = tiff.get_byte() << 8;
   dw |= tiff.get_byte();
   return dw;
}

////////////////////////////////////////////////////////////////////////////////

bool TiffReader::tiff_init()
{
   byte tiff_header[4];
   read( tiff_header, 4 );

   if( memcmp( tiff_header, "II*\0", 4) == 0 )    // TIFF, NEF, DNG
   {
      binary_format = format_intel;
      tiff_format = standard_tiff;
      _get_dword = get_intel_dword;
      _get_word = get_intel_word;
      return true;
   }

   if( memcmp( tiff_header, "MM\0*", 4) == 0 )
   {
      binary_format = format_motorola;
      tiff_format = standard_tiff;
      _get_dword = get_motorola_dword;
      _get_word = get_motorola_word;
      return true;
   }

   if( memcmp( tiff_header, "IIU\0", 4) == 0 )  // Panasonic RAW
   {
      binary_format = format_intel;
      tiff_format = panasonic_raw;
      _get_dword = get_intel_dword;
      _get_word = get_intel_word;
      return true;
   }

   if( memcmp( tiff_header, "IIRS", 4) == 0 )  // OLYMPUS Camedia ORF
   {
      binary_format = format_intel;
      tiff_format = olympus_orf;
      _get_dword = get_intel_dword;
      _get_word = get_intel_word;
      return true;
   }

   if( memcmp( tiff_header, "IIRO", 4) == 0 )  // OLYMPUS E-1 ORF
   {
      binary_format = format_intel;
      tiff_format = olympus_orf;
      _get_dword = get_intel_dword;
      _get_word = get_intel_word;
      return true;
   }

   if( memcmp( tiff_header, "MMOR", 4) == 0 )  // OLYMPUS E-20 ORF
   {
      tiff_format = olympus_orf;
      binary_format = format_motorola;
      _get_dword = get_motorola_dword;
      _get_word = get_motorola_word;
      return true;
   }

   if( memcmp( tiff_header, "\0MRM", 4) == 0 )    // MRW
   {
      mrw_data_offset = get_dword_mm() + 8;
      for(;;)
      {
         read( tiff_header, 4 );
         unsigned size = get_dword_mm();

         if( memcmp( tiff_header, "\0TTW", 4) == 0 )
            break;

         skip( size );
      }

      bool res = tiff_init();
      tiff_format = minolta_mrw;
      m_offset = ftell(file)-4;
      return res;
   }

   return binary_format != unknown_format;
}

void TiffReader::error()
{
   throw "Unexpected EOF";
}

////////////////////////////////////////////////////////////////////////////////
void add_tag(IFDir* IFD, TiffReader& tiff, TIFF_ID const& tid)
{
   unsigned k;
   IFDir::Tag* tag = IFD->add_tag( tid.tag, tid.type, tid.count, NULL );

   if( !tag ) return;

   switch( tid.type )
   {
   case TIFF::type_UNDEFINED:
   case TIFF::type_BYTE:
   case TIFF::type_SBYTE:
   case TIFF::type_ASCII:

      for(k=0; k<tid.count; ++k) {
         ((byte*)tag->value)[k] = tiff.get_byte();
      } break;

   case TIFF::type_WORD:
   case TIFF::type_SHORT:

      for(k=0; k<tid.count; ++k) {
         ((word*)tag->value)[k] = (word)tiff.get_word();
      } break;

   case TIFF::type_DWORD:
   case TIFF::type_LONG:

      for(k=0; k<tid.count; ++k) {
         ((dword*)tag->value)[k] = tiff.get_dword();
      } break;

   case TIFF::type_RATIONAL:
   case TIFF::type_SRATIONAL:

      for(k=0; k<2*tid.count; ++k) {
         ((dword*)tag->value)[k] = tiff.get_dword();
      } break;

   default:
      break;
   }
}

////////////////////////////////////////////////////////////////////////////////

bool ParseNikonMakerTag(TiffReader& tiff, TIFF_ID& tid, TIFF_Content& NEF);

bool ParseCFATag( TiffReader&, TIFF_ID& tid, TIFF_Content& )
{
   switch( tid.tag )
   {
   case TIFF::NewSubfile:
   case TIFF::ImageWidth:
   case TIFF::ImageHeight:
   case TIFF::BitsPerSample:
   case TIFF::Compression:
   case TIFF::PhotometricInterpretation:
   case TIFF::StripOffsets:
   case TIFF::SamplesPerPixel:
   case TIFF::RowsPerStrip:
   case TIFF::StripByteCounts:
   case TIFF::PlanarConfiguration:
   case TIFF::TileWidth:
   case TIFF::TileLength:
   case TIFF::TileOffsets:
   case TIFF::TileByteCounts:
   case TIFF::CFARepeatPatternDim:
   case TIFF::CFAPattern:
   case 0xC617: // CFALayout
   case 0xC618: // LinearizationTable (compressed NEF, for instance)
      return true;
   default: break;
   }

   return false;
}



bool ParseTag( TiffReader& tiff, TIFF_ID& tid, TIFF_Content& NEF )
{
   switch( tid.tag )
   {
   case TIFF::ImageWidth:
   case TIFF::ImageHeight:
   case TIFF::BitsPerSample:
   case TIFF::Compression:
   case TIFF::StripOffsets:
   case TIFF::SamplesPerPixel:
   case TIFF::RowsPerStrip:
   case TIFF::StripByteCounts:
      return true;

   case 0x10E: return true; // ImageDescription
   case 0x10F: return true; // Maker
   case 0x110: return true; // Model
   case 0x112: return true; // Orientation
   case 0x11A: return true; // XResolution
   case 0x11B: return true; // YResolution
   case 0x128: return true; // ResolutionUnit

   case 0x131: return true; // Software
   case 0x132: return true; // DateTime
   case 0x13E: return true; // WhitePoint

   case 0x214: return true; // ReferenceBlackWhite

   case 0x927C:
   {
      byte buf[6];
      unsigned temp = tiff.tell();
      tiff.read(buf, 6);

      if( memcmp(buf, "Nikon\0", 6) == 0 )
      {
         int sub_type = tiff.get_dword_ii();
         if( sub_type == 2 || sub_type == 0x1002 )
         {
            // Nikon TIFF IFD
            TiffReader tiff2( tiff );

            tiff2.m_offset += tiff.tell();

            if( tiff2.tiff_init() )
            {
               // IFD1
               unsigned ifd = tiff2.get_dword();
               if( ifd )
               {
                  tiff2.seek(ifd);
                  tiff2.parse_tags(ParseNikonMakerTag, &NEF.MakerNote, NEF);
               }
               tiff.seek( temp );
               return false;
            }
         }
      }
      else
      {
         if( NEF.Is_NIKON() )
         {
            // Nikon IFD
            tiff.seek( tid.offset );
            tiff.parse_tags(ParseNikonMakerTag, &NEF.MakerNote, NEF);
            tiff.seek( temp );
            return false;
         }
      }

      return false;
   }

   case 0x8769: return false; // Exif IFD
   case 0x8825: return false; // GPS IFD
   case 0xA005: return false; // Interoperability IFD

   case 0x828D: return false; // CFARepeatPatternDim
   case 0x828E: return false; // CFAPattern

   case 0x8298: return true; // Copyright
   case 0x829A: return true; // ExposureTime
   case 0x829D: return true; // F-Number

   case 0x8773: return true; // ICC profile

   case 0x8822: return true; // Exposure Program
   case 0x8824: return true; // SpectralSensitivity
   case 0x8827: return true; // ISO
   case 0x8828: return true; // OptElectricConvFactor

   case 0x9000: return false; // ExifVersion
   case 0x9003: return true; // DateTimeOriginal
   case 0x9004: return true; // DateTimeDigitized

   case 0x9101: return false; // ComponentsConfiguration
   case 0x9102: return false; // CompressedBitsPerPixel

   case 0x9201: return true; // ShutterSpeedValue
   case 0x9202: return true; // Aperture Value
   case 0x9203: return true; // BrightnessValue
   case 0x9204: return true; // Exposure Bias
   case 0x9205: return true; // MaxApertureValue
   case 0x9206: return true; // SubjectDistance
   case 0x9207: return true; // Metering Mode
   case 0x9208: return true; // Light Source
   case 0x9209: return true; // Flash
   case 0x920A: return true; // FocalLength
   case 0x9214: return true; // SubjectArea

   case 0x9286: return true; // UserComment
   case 0x9290: return true; // SubSecTime
   case 0x9291: return true; // SubSecTimeOriginal
   case 0x9292: return true; // SubSecTimeDigitized

   case 0xA000: return false; // FlashPixVersion
   case 0xA001: return true; // ColorSpace
   case 0xA002: return false; // PixelXDimension for compressed only
   case 0xA003: return false; // PixelYDimension for compressed only
   case 0xA004: return true; // RelatedSoundFile

   case 0xA20B: return true; // FlashEnergy
   case 0xA20C: return true; // SpatialFrequencyResponse
   case 0xA20E: return true; // FocalPlaneXResolution
   case 0xA20F: return true; // FocalPlaneYResolution
   case 0xA210: return true; // FocalPlaneResolutionUnit
   case 0xA214: return true; // SubjectLocation
   case 0xA215: return true; // ExposureIndex
   case 0xA217: return true; // SensingMethod
   case 0xA300: return true; // FileSource
   case 0xA301: return true; // SceneType
   case 0xA302: return true; // ColorFilterArrayPattern

   case 0xA401: return true; // CustomRendered
   case 0xA402: return true; // ExposureMode
   case 0xA403: return true; // WhiteBalance
   case 0xA404: return true; // DigitalZoomRatio
   case 0xA405: return true; // FocalLengthIn35mmFilm
   case 0xA406: return true; // SceneCaptureType
   case 0xA407: return true; // GainControl
   case 0xA408: return true; // Contrast
   case 0xA409: return true; // Saturation
   case 0xA40A: return true; // Sharpness
   case 0xA40B: return true; // DeviceSettingDescription
   case 0xA40C: return true; // SubjectDistanceRange

   case TIFF::DNGVersion: return true;
   default: break;
   }

   return false;
}

bool ParsePanasonicTag( TiffReader& tiff, TIFF_ID& tid, TIFF_Content& NEF )
{
   switch( tid.tag )
   {
   case 0x017: return true;
   }
   return ParseTag( tiff, tid, NEF );
}

bool ParseOlympusTag( TiffReader& tiff, TIFF_ID& tid, TIFF_Content& NEF )
{
   switch( tid.tag )
   {
   case TIFF::ImageWidth:
   case TIFF::ImageHeight:
   case TIFF::BitsPerSample:
   case TIFF::Compression:
   case TIFF::StripOffsets:
   case TIFF::SamplesPerPixel:
   case TIFF::RowsPerStrip:
   case TIFF::StripByteCounts:
      return true;
   default:;
   }
   return ParseTag( tiff, tid, NEF );
}

// ExifReader 2.80 (http://www.takenet.or.jp/~ryuuji/minisoft/exifread/english/)
bool ParseNikonMakerTag(TiffReader& tiff, TIFF_ID& tid, TIFF_Content& NEF )
{
   switch( tid.tag )
   {
   case 0x001: return false; // Version
   case 0x002: return true; // ISO Setting
   case 0x003: return true; // ColorMode
   case 0x004: return false; // Quality always RAW
   case 0x005: return true; // WhiteBalance
   case 0x006: return true; // Sharpening
   case 0x007: return true; // AutoFocus Mode
   case 0x008: return true; // Flash Sync Mode
   case 0x00A: return true; // ImageDiagonal
   case 0x00F: return true; // ISO Selection
   case 0x010: return false; // Dump
   case 0x011: return false; // thumbnail
   case 0x080: return true; // ImageAdjustment
   case 0x081: return true; // Image Adjustment // ExifReader 2.80
   case 0x082: return true; // AuxiliaryLens
   case 0x084: return true; // Lens
   case 0x085: return true; // Manual Focus
   case 0x086: return true; // Digital Zoom
   case 0x088: return true; // AF Position
   case 0x089: return true; // ShotMode // ExifReader 2.80
   case 0x08D: return true; // ColorSpace // ExifReader 2.80
   case 0x08F: return true; // Scene
   case 0x092: return true; // Hue Adjustment // ExifReader 2.80
   case 0x094: return true; // Saturation // ExifReader 2.80
   case 0x095: return true; // Noise Reduction
   case 0x096: return true; 
   case 0x100: return false;
   case 0xE00: return false; // PrintIM

   default: return ParseTag( tiff, tid, NEF );
   }
}

bool TIFF_Content::Is_MINOLTA()
{
   IFDir::Tag* Maker = IFD1.get_tag(TIFF::Make);
   if( !Maker ) return false;
   if( Maker->count >= 14 && 0 == memcmp( (const char*)Maker->value, "KONICA MINOLTA", 14 ) ) return true;
   if( Maker->count >= 14 && 0 == memcmp( (const char*)Maker->value, "Konica Minolta", 14 ) ) return true;
   if( Maker->count >= 7 && 0 == memcmp( (const char*)Maker->value, "Minolta", 7 ) ) return true;
   return false;
}

bool TIFF_Content::Is_OLYMPUS()
{
   IFDir::Tag* Maker = IFD1.get_tag(TIFF::Make);
   if( !Maker ) return false;
   if( Maker->count < 7 ) return false;
   return 0 == memcmp( (const char*)Maker->value, "OLYMPUS", 7 );
}

bool TIFF_Content::Is_PENTAX()
{
   IFDir::Tag* Maker = IFD1.get_tag(TIFF::Make);
   if( !Maker ) return false;
   if( Maker->count < 6 ) return false;
   return 0 == memcmp( (const char*)Maker->value, "PENTAX", 6 );
}

bool TIFF_Content::Is_SONY()
{
   IFDir::Tag* Maker = IFD1.get_tag(TIFF::Make);
   if( !Maker ) return false;
   if( Maker->count < 4 ) return false;
   return 0 == memcmp( (const char*)Maker->value, "SONY", 4 );
}

bool TIFF_Content::Is_NIKON()
{
   IFDir::Tag* Maker = IFD1.get_tag(TIFF::Make);
   if( !Maker ) return false;
   if( Maker->count < 5 ) return false;
   return 0 == memcmp( (const char*)Maker->value, "NIKON", 5 );
}

////////////////////////////////////////////////////////////////////////////////
void TiffReader::safe_parse_tags(TParser parser, IFDir* IFD, TIFF_Content& NEF)
{
   try
   {
      if( binary_format == format_motorola )
         IFD->byte_order = IFD->motorola_byte_order;

      if( binary_format == format_intel )
         IFD->byte_order = IFD->intel_byte_order;

      unsigned number_of_fields = get_word();

      unsigned SubIFD[NEF.const_MaxSubIFD] = {0};
      unsigned Exif_IFD = 0;
      unsigned temp;

      TIFF_ID tid;
      unsigned i;
      for(i=0; i < number_of_fields; ++i, seek( temp ) )
      {
         tid.tag = (unsigned short)get_word();
         tid.type = (unsigned short)get_word();
         tid.count = get_dword();

         unsigned data_pos = tell();
         tid.offset = get_dword();

         temp = tell();

         if( tid.type >= TIFF::n_types ) continue;

         unsigned size = TIFF::sizeof_value[tid.type]*tid.count;

         // if size of data less 4 bytes, then offset hold data
         if( tid.type >= TIFF::n_types || size > 4 )
         {
            data_pos = tid.offset;
         }

         seek( data_pos );

         if( tid.tag == 0x8769 ) {
            Exif_IFD = tid.offset;
            continue; }

         if( tid.tag == 0x14A ) {
            if( tid.type == TIFF::type_DWORD
             || tid.type == TIFF::type_LONG )
            {
               for(int i=0; i<(int)tid.count && i<NEF.const_MaxSubIFD; ++i)
                  SubIFD[i] = get_dword();
            }
            continue; }

         if( parser( *this, tid, NEF ) )
            add_tag(IFD, *this, tid);
      }

      if( Exif_IFD )
      {
         seek( Exif_IFD );
         parse_tags(ParseTag, &NEF.EXIF, NEF);
      }
      for(int i=0; i<NEF.const_MaxSubIFD; ++i)
      {
         if( SubIFD[i] == 0 ) break;

         seek( SubIFD[i] );

         parse_tags(ParseCFATag, &NEF.SubIFD[i], NEF);
      }
   }
   catch(const char* x)
   {
      printf("error: %s\n", x);
   }
   catch(...)
   {
      printf("error: exception\n");
   }
}

void TiffReader::parse_tags(TParser parser, IFDir* IFD, TIFF_Content& NEF)
{
   try
   {
      safe_parse_tags(parser, IFD, NEF);
   }
   catch(...)
   {
      printf("error: except\n");
   }
}
////////////////////////////////////////////////////////////////////////////////

IFDir* TIFF_Content::Init_CFA()
{
   IFDir::Tag* BitsPerSample = IFD1.get_tag(TIFF::BitsPerSample);
   if( !BitsPerSample ) return 0;
   if( BitsPerSample->count != 1 ) return 0;
   if( BitsPerSample->get_value() != 12
    && BitsPerSample->get_value() != 16 ) return 0;

   IFDir::Tag* CFAP = EXIF.get_tag( TIFF::ColorFilterArrayPattern );
   if( !CFAP ) return 0;
   // make valid for read_cfa() CFA IFD

   IFDir* CFA = &SubIFD[0];

   CFA->byte_order = IFD1.byte_order;

   CFA->copy_tag( IFD1.get_tag( TIFF::ImageWidth ) );
   // it is not valid Height, will corect it late
   dword H = IFD1.get_tag( TIFF::ImageHeight )->get_value();
   CFA->add_DWORD( TIFF::ImageHeight, H );
   CFA->copy_tag( IFD1.get_tag( TIFF::BitsPerSample ) );
   CFA->copy_tag( IFD1.get_tag( TIFF::Compression ) );
   CFA->add_WORD( TIFF::PhotometricInterpretation, 0x8023 ); // CFA
   CFA->copy_tag( IFD1.get_tag( TIFF::SamplesPerPixel ) );
   CFA->add_DWORD( TIFF::RowsPerStrip, H );

   IFDir::Tag* StripOffsets = IFD1.get_tag( TIFF::StripOffsets );
   // only first offset has sense
   CFA->add_DWORD( TIFF::StripOffsets, StripOffsets->get_value() );
   IFDir::Tag* StripByteCounts = IFD1.get_tag( TIFF::StripByteCounts );
   dword approx_size = StripOffsets->get_value( StripOffsets->count-1 )
       + StripByteCounts->get_value( StripByteCounts->count-1 );
   CFA->add_DWORD( TIFF::StripByteCounts, approx_size );

   word cfa_size[2] = {2,2};
   byte cfa_patt[4] = {2,1,1,0};
   
   if( CFAP )
   {
      bool II = EXIF.byte_order == EXIF.intel_byte_order;

      cfa_size[0] = (word)(II ? CFAP->get_value(0) : CFAP->get_value(1));
      cfa_size[1] = (word)(II ? CFAP->get_value(2) : CFAP->get_value(3));

      cfa_patt[0] = (byte)CFAP->get_value(4);
      cfa_patt[1] = (byte)CFAP->get_value(5);
      cfa_patt[2] = (byte)CFAP->get_value(6);
      cfa_patt[3] = (byte)CFAP->get_value(7);
   }
   CFA->add_tag( TIFF::CFARepeatPatternDim, TIFF::type_WORD, 2, cfa_size );
   CFA->add_tag( TIFF::CFAPattern, TIFF::type_BYTE, 4, cfa_patt );
   return CFA;
}

bool Is_CFA( IFDir* IFD )
{
   IFDir::Tag* PhotometricInterpretation = IFD->get_tag(TIFF::PhotometricInterpretation);
   if( !PhotometricInterpretation ) return false;
   if( PhotometricInterpretation->get_value() == 0x8023 ) return true;
   return false;
}

IFDir* TIFF_Content::Get_CFA()
{
   IFDir* CFA = 0;
   for(int i=0; i<const_MaxSubIFD; ++i)
   {
      if( !SubIFD[i].get_count() )
         break;

      if( ::Is_CFA( &SubIFD[i] ) )
      {
         CFA = &SubIFD[i];
         // pass thumbnail CFA image
      }
   }
   if( !CFA )
   {
      if( Is_OLYMPUS() )
      {
         return Init_CFA();
      }
      if( Is_PENTAX() )
      {
         return Init_CFA();
      }
      if( Is_MINOLTA() )
      {
         return Init_CFA();
      }
   }
   return CFA;
}

bool TIFF_Content::read_tiff( FILE* in )
{
   try
   {
      return safe_read_tiff( in );
   }
   catch(...)
   {
      printf("error: except\n");
   }
   return false;
}

bool TIFF_Content::RestoreOriginalModel()
{
   IFDir::Tag* Make = IFD1.get_tag(TIFF::Make);
   if( !Make ) return false;

   if( Make->count >= 5
    && 0 == memcmp( Make->value, "NIKON", 5 ) )
   {
      IFDir::Tag* Model = IFD1.get_tag(TIFF::Model);
      if( !Model ) return false;

      if( Model->count >= 1 && Model->value[0] == 'E' )
         is_COOLPIX_NEF = true;

      if( Make->count >= 7 && Make->value[5] == 0 && strlen(Make->value) == 5
       && Model->count >= 7 && Model->value[5] == 0 && strlen(Model->value) == 5 )
      {
         IFD1.add_ASCII(TIFF::Make, Make->value + 6, Make->count-6 );
         IFD1.add_ASCII(TIFF::Model, Model->value + 6, Model->count-6 );
      }
   }
   return true;
}

bool TIFF_Content::safe_read_tiff( FILE* in )
{
   try
   {
      TiffReader tiff( in, ftell(in) );

      if( !tiff.tiff_init() )
         { return false; }

      // IFD1
      unsigned temp = tiff.get_dword();
      if( !temp )
         return false;

      tiff.seek( temp );

      if( tiff.tiff_format == tiff.olympus_orf )
      {
         tiff.parse_tags( ParseOlympusTag, &IFD1, *this );
      }
      else
      if( tiff.tiff_format == tiff.panasonic_raw )
      {
         tiff.parse_tags( ParsePanasonicTag, &IFD1, *this );

         IFDir::Tag* ISOSpeedRating = IFD1.get_tag( 0x017 );
         if( ISOSpeedRating )
         {
            if( !EXIF.get_tag( 0x8827 ) )
               EXIF.add_WORD( 0x8827, (word)ISOSpeedRating->get_value() );

            IFD1.remove( 0x017 );
         }
      }
      else
      if( tiff.tiff_format == tiff.minolta_mrw )
      {
         tiff.parse_tags( ParseTag, &IFD1, *this );

         fseek( in, 0, SEEK_END );
         unsigned file_size = ftell( in );
         rewind( in );
         
         IFD1.add_WORD( TIFF::BitsPerSample, (word)12 );
         IFD1.add_DWORD( TIFF::StripOffsets, (dword)tiff.mrw_data_offset );         
         IFD1.add_DWORD( TIFF::StripByteCounts, (dword)(file_size-tiff.mrw_data_offset) );
         IFD1.add_DWORD( TIFF::ImageWidth, IFD1.get_tag(TIFF::ImageWidth)->get_value()+8 );
         IFD1.add_DWORD( TIFF::ImageHeight, IFD1.get_tag(TIFF::ImageHeight)->get_value()+4 );

         byte cfa[8] = {0,0, 0,0,  0,1,1,2};
         if( EXIF.byte_order == EXIF.intel_byte_order )
            cfa[0] = cfa[2] = 2;
         else
            cfa[1] = cfa[3] = 2;

         EXIF.add_tag( TIFF::ColorFilterArrayPattern, TIFF::type_UNDEFINED, 8, cfa );
      }
      else
         tiff.parse_tags( ParseTag, &IFD1, *this );

      // Restore original vendor and model from NEF created by raw2nef
      RestoreOriginalModel();
      return true;
   }
   catch(const char* x)
   {
      printf("error: %s\n", x);
   }
   catch(...)
   {
      printf("error: exception\n");
   }

   return false;
}

bool TIFF_Content::read_exif( FILE* in )
{
   try
   {
      return safe_read_exif( in );
   }
   catch(...)
   {
      printf("error: except\n");
   }
   return false;
}

bool TIFF_Content::safe_read_exif( FILE* in )
{
   try
   {
      int marker;
      int marker_count = 0;
      unsigned segment_pos, segment_size;

      for(;; fseek( in, segment_pos + segment_size, SEEK_SET ) )
      {
         // wait for marker
         do
         {
            marker = fgetc( in );
            if (marker == EOF)
               { return false; }
         }
         while (marker == 0xFF);

         // store start of marker segment
         segment_pos = ftell( in );

         if( marker_count == 0 && marker != 0xD8 ) // SOI
            { return false; }

         ++marker_count;

         if( marker == 0xD8 ) // SOI, no size
         {
            segment_size = 0;
            continue;
         }

         if( marker == 0xD9 ) // EOI
            break;

         if( marker == 0xDA ) // SOS
            break;

         segment_size = fgetw( in );

         if( feof( in ) ) { break; }

         if( marker == 0xE1 ) // APP1
         {
            byte buff[6];
            fread( buff, 6, 1, in );

            if( memcmp( buff, "Exif\0", 5) != 0 ) {
               continue; }

            TiffReader tiff( in, ftell(in) );

            if( !tiff.tiff_init() )
               { continue; }

            // IFD1
            unsigned temp = tiff.get_dword();
            if( !temp ) break;

            tiff.seek( temp );
            tiff.parse_tags( ParseTag, &IFD1, *this );
            return true;
         }
      }
   }
   catch(const char* x)
   {
      printf("error: %s\n", x);
   }
   catch(...)
   {
      printf("error: exception\n");
   }
   return false;
}

void TIFF_Content::free()
{
   is_COOLPIX_NEF = 0;

   IFD1.free();
   EXIF.free();
   MakerNote.free();

   for(int i=0; i<const_MaxSubIFD; ++i)
      SubIFD[i].free();

   memset( &ccd_pars, 0, sizeof(ccd_pars));

   picture.swap( buff_t<color_t>() );
   linear_tab.swap( buff_t<word>() );
}
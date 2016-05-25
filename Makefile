CC=g++
CFLAGS=-c -g3 -Wno-write-strings -Wno-narrowing -fpermissive
LDFLAGS=
SOURCES=tiff_ifd.cpp raw2nef.cpp read_exif.cpp camera_id.cpp mask_dead.cpp read_cfa.cpp read_nikon.cpp read_dng.cpp jpeg.cpp read_raw2.cpp packtilev.cpp get_len.cpp thumbnail.cpp write_ifd.cpp write_dng.cpp
OBJECTS=$(SOURCES:.cpp=.o)

all: $(SOURCES) raw2dng raw2nef deadpix

raw2dng: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) write_dng2.cpp -o $@

raw2nef: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) write_nef.cpp write_nef2.cpp -o $@

deadpix: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) dead_pix.cpp dead_pix2.cpp -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f *.o raw2dng raw2nef deadpix

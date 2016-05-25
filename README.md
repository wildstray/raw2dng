# raw2dng
Fork of **raw2dng** (http://e2500.narod.ru/raw2dng_e.htm). Portable version with (in the making) support for 
**Xiaomi Yi** and other Sony Exmor based cameras.

Example of raw2nef.ini:
```
; file_size, data_offset, raw_width, raw_height, cfa_colors, line_length, bits, flags, camera ID1, camera ID2 etc 
; file_size is used for autodetect camera type if paired jpeg file is not available
; data_offset - offset to begin of raw data (16-bits only)
; raw_width - width of output raw picture. Without rubbish pixels in the end of line.
; raw_height - height of output raw picture. Without rubbish pixels below the last line.
; cfa_colors - one of 0 - GMYC, 1 - MGCY, 2 - YCGM, 3 - CYMG, 4 - BGGR, 5 - GBRG, 6 - GRBG, 7 - RGGB;
; line_length - Exactly input line length in bytes. 
; bits - supports 10, 12, 16 bits per pixel. Output data has 12 bits.
; flags is OR of : 1 - interlaced, 2 - ARM CPU (mixed data bits), 4 - intel byte order in 16-bits data
; Camera IDs is the list of cameras with same parameters. 
; This ID have to be matched to the first camera name in EXIF Model field.
; For example, if EXIF camera model is "C40Z,D40Z" - ID is C40Z.
1581060, 0, 1288,  964, 3, 1632, 10, 1, E900 ; Thanks to Sakura Shan
2465792, 0, 1616, 1204, 2, 2048, 10, 1, E950, E775, C2020Z
2940928, 0, 1616, 1204, 2, 2424, 12, 1, E2500
2940928, 0, 1616, 1206, 7, 2424, 12, 3, E2100 ;*
7438336, 0, 2576, 1924, 0, 3864, 12, 1, E5000, E5700
4771840, 0, 2064, 1540, 1, 3096, 12, 1, E995
4771840, 0, 2064, 1540, 0, 3096, 12, 1, E990, E885, C3030Z ;*
4775936, 0, 2064, 1542, 5, 3096, 12, 3, E3100
4775936, 0, 2064, 1542, 7, 3096, 12, 3, E3700 ;*
5869568, 0, 2288, 1710, 4, 3432, 12, 1, E4300
5869568, 0, 2288, 1710, 4, 3432, 12, 3, Z2 ;*
5865472, 0, 2288, 1708, 0, 3432, 12, 1, E4500, C4040Z
5935104, 0, 2304, 1716, 0, 3456, 12, 1, C40Z
5939200, 0, 2304, 1718, 4, 3456, 12, 1, C4100Z
5939200, 0, 2304, 1718, 4, 3456, 12, 3, C765UZ, C770UZ
6054400, 0, 2346, 1720, 7, 3520, 12, 0, QV-R41 
7630848, 0, 2608, 1948, 6, 3912, 12, 3, E5900
7630848, 0, 2608, 1948, 7, 3912, 12, 3, C55Z ;*
7684000, 0, 2260, 1700, 7, 4520, 16, 0, QV4000 ; Casio QV-4000
7753344, 0, 2602, 1986, 7, 3904, 12, 0, EX-Z55 ; Casio EX-Z55 ?
9313536, 0, 2844, 2142, 7, 4288, 12, 0, EX-P600 ; Casio EX-P600
10979200,0, 3114, 2350, 7, 4672, 12, 0, EX-P700 ; Casio EX-P700 ?
16244224, 1536, 3288, 2458, 7, 6608, 16, 4, FZ30
16841216, 1548, 3858, 2154, 7, 7760, 16, 4, LX1
31850496, 0, 4608, 3456, 7, 9216, 16, 4, XIAOMI-YI ; Xiaomi Yi
```

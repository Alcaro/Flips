#include "global.h"
#include "array.h"

//uses the standard 0xEDB88320 generator polynomial
uint32_t crc32_update(arrayview<uint8_t> data, uint32_t crc);
static inline uint32_t crc32(arrayview<uint8_t> data) { return crc32_update(data, 0); }

//calculates the 'inverse' crc32 of a buffer
//given
// a = crc32(buf1)
// b = crc32_update(buf2, a) ( == crc32(buf1+buf2) )
//then
// crc32_cancel(buf2, b) == a
uint32_t crc32_cancel(arrayview<uint8_t> data, uint32_t crc);

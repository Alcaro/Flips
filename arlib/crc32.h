#include "global.h"
#include "array.h"

//uses 0xEDB88320 as generator polynomial
uint32_t crc32(arrayview<uint8_t> data);
uint32_t crc32_update(arrayview<uint8_t> data, uint32_t crc);

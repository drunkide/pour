#ifndef COMMON_BYTESWAP_H
#define COMMON_BYTESWAP_H

static uint16_t MSB16(uint16_t value)
{
    return (((value     ) & 0xFF) << 8)
         | (((value >> 8) & 0xFF)      );
}

static uint32_t MSB32(uint32_t value)
{
    return (((value      ) & 0xFF) << 24)
         | (((value >>  8) & 0xFF) << 16)
         | (((value >> 16) & 0xFF) <<  8)
         | (((value >> 24) & 0xFF)      );
}

static uint64_t MSB64(uint64_t value)
{
    return (((value      ) & 0xFF) << 56)
         | (((value >>  8) & 0xFF) << 48)
         | (((value >> 16) & 0xFF) << 40)
         | (((value >> 24) & 0xFF) << 32)
         | (((value >> 32) & 0xFF) << 24)
         | (((value >> 40) & 0xFF) << 16)
         | (((value >> 48) & 0xFF) <<  8)
         | (((value >> 56) & 0xFF)      );
}

#endif

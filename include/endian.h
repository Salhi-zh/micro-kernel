#ifndef ENDIAN_H
#define ENDIAN_H

#include <kernel.h>

/* Assume little-endian (x86) */
static inline uint16_t htobe16(uint16_t x)
{
    return (x >> 8) | (x << 8);
}

static inline uint16_t be16toh(uint16_t x)
{
    return (x >> 8) | (x << 8);
}

static inline uint32_t htobe32(uint32_t x)
{
    return ((x >> 24) & 0xFF) |
           ((x >> 8)  & 0xFF00) |
           ((x << 8)  & 0xFF0000) |
           ((x << 24) & 0xFF000000);
}

static inline uint32_t be32toh(uint32_t x)
{
    return htobe32(x);  /* symmetric on little-endian */
}

#endif

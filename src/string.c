#include <string.h>

void* memset(void* dest, int c, size_t count)
{
    uint8_t* d = (uint8_t*)dest;
    while (count--) {
        *d++ = (uint8_t)c;
    }
    return dest;
}

void* memcpy(void* dest, const void* src, size_t count)
{
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (count--) {
        *d++ = *s++;
    }
    return dest;
}

size_t strlen(const char* str)
{
    size_t len = 0;
    while (*str++) len++;
    return len;
}

int strcmp(const char* s1, const char* s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n)
{
    while (n-- && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    if (n == (size_t)-1) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

char* strcpy(char* dest, const char* src)
{
    char* d = dest;
    while ((*d++ = *src++)) ;
    return dest;
}
char *strchr(const char *s, int c) {
    while (1) {
        if ((unsigned char)*s == (unsigned char)c)
            return (char *)s;
        if (*s == '\0')
            return NULL;
        s++;
    }
}

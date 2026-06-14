#include "string.h"
#include <stdint.h>

void* memset(void* s, int c, size_t n) {
    uint64_t val = (unsigned char)c;
    val |= val << 8;
    val |= val << 16;
    val |= val << 32;

    uint64_t* dest = (uint64_t*)s;
    size_t qwords = n / 8;
    size_t bytes = n % 8;

    __asm__ volatile (
        "rep stosq"
        : "+D"(dest), "+c"(qwords)
        : "a"(val)
        : "memory"
    );

    uint8_t* dest_b = (uint8_t*)dest;
    __asm__ volatile (
        "rep stosb"
        : "+D"(dest_b), "+c"(bytes)
        : "a"(val)
        : "memory"
    );

    return s;
}

void* memcpy(void* dest, const void* src, size_t n) {
    uint64_t* d = (uint64_t*)dest;
    const uint64_t* s = (const uint64_t*)src;
    size_t qwords = n / 8;
    size_t bytes = n % 8;

    __asm__ volatile (
        "rep movsq"
        : "+D"(d), "+S"(s), "+c"(qwords)
        :
        : "memory"
    );

    uint8_t* d_b = (uint8_t*)d;
    const uint8_t* s_b = (const uint8_t*)s;
    __asm__ volatile (
        "rep movsb"
        : "+D"(d_b), "+S"(s_b), "+c"(bytes)
        :
        : "memory"
    );

    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    if (d == s || n == 0) return dest;

    if (d < s || d >= s + n) {
        return memcpy(dest, src, n);
    }

    for (size_t i = n; i != 0; i--) {
        d[i - 1] = s[i - 1];
    }

    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    while(n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

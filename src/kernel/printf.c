#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "string.h"

static void reverse(char* s) {
    int i, j;
    char c;
    for (i = 0, j = (int)strlen(s) - 1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

static void itoa(uint64_t n, char* s, int base) {
    int i = 0;
    if (n == 0) {
        s[i++] = '0';
    } else {
        while (n > 0) {
            int rem = (int)(n % base);
            s[i++] = (char)((rem > 9) ? (rem - 10) + 'a' : rem + '0');
            n /= base;
        }
    }
    s[i] = '\0';
    reverse(s);
}

int vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    size_t i = 0;
    const char* p;
    char buf[65];

    for (p = format; *p != '\0' && i < size - 1; p++) {
        if (*p != '%') {
            str[i++] = *p;
            continue;
        }

        p++;
        switch (*p) {
            case 'c':
                str[i++] = (char)va_arg(ap, int);
                break;
            case 's': {
                char* s = va_arg(ap, char*);
                while (*s != '\0' && i < size - 1) {
                    str[i++] = *s++;
                }
                break;
            }
            case 'd': {
                int64_t n = va_arg(ap, int64_t);
                if (n < 0) {
                    str[i++] = '-';
                    n = -n;
                }
                itoa((uint64_t)n, buf, 10);
                char* s = buf;
                while (*s != '\0' && i < size - 1) {
                    str[i++] = *s++;
                }
                break;
            }
            case 'u': {
                uint64_t n = va_arg(ap, uint64_t);
                itoa(n, buf, 10);
                char* s = buf;
                while (*s != '\0' && i < size - 1) {
                    str[i++] = *s++;
                }
                break;
            }
            case 'x': {
                uint64_t n = va_arg(ap, uint64_t);
                itoa(n, buf, 16);
                char* s = buf;
                while (*s != '\0' && i < size - 1) {
                    str[i++] = *s++;
                }
                break;
            }
            case 'p': {
                uint64_t n = (uint64_t)va_arg(ap, void*);
                itoa(n, buf, 16);
                char* s = buf;
                while (*s != '\0' && i < size - 1) {
                    str[i++] = *s++;
                }
                break;
            }
            case '%':
                str[i++] = '%';
                break;
            default:
                str[i++] = *p;
                break;
        }
    }

    str[i] = '\0';
    return (int)i;
}

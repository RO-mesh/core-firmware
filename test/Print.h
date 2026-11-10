#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

class Print {
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t *buffer, size_t size) {
        size_t n = 0;
        while (size--) {
            if (write(*buffer++)) n++;
            else break;
        }
        return n;
    }
    size_t print(const char *s) {
        if (!s) return 0;
        return write((const uint8_t*)s, strlen(s));
    }
    size_t println(const char *s = "") {
        size_t n = print(s);
        n += print("\r\n");
        return n;
    }
};


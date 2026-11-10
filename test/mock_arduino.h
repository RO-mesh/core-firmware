#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string>

// Minimal Arduino types & constants for native desktop compilation
typedef uint8_t byte;

#ifndef LOW
  #define LOW 0x0
#endif
#ifndef HIGH
  #define HIGH 0x1
#endif
#ifndef INPUT
  #define INPUT 0x0
#endif
#ifndef OUTPUT
  #define OUTPUT 0x1
#endif
#ifndef INPUT_PULLUP
  #define INPUT_PULLUP 0x2
#endif

#ifndef PUB_KEY_SIZE
  #define PUB_KEY_SIZE 32
#endif

// Mock FileSystem
class FILESYSTEM {
public:
  void mkdir(const char*) {}
};

// Mock String
class String : public std::string {
public:
  String() : std::string() {}
  String(const char* s) : std::string(s ? s : "") {}
  String(const std::string& s) : std::string(s) {}

  void trim() {
    while (!empty() && (back() == ' ' || back() == '\r' || back() == '\n' || back() == '\t')) pop_back();
    while (!empty() && (front() == ' ' || front() == '\r' || front() == '\n' || front() == '\t')) erase(begin());
  }

  bool equalsIgnoreCase(const char* other) const {
    if (!other) return false;
    return strcasecmp(c_str(), other) == 0;
  }
};

// Mock Serial
class MockSerial {
public:
  void begin(unsigned long) {}
  void print(const char* s) { printf("%s", s); }
  void println(const char* s = "") { printf("%s\n", s); }
  void printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
  }
  int available() { return 0; }
  String readStringUntil(char) { return String(""); }
};

extern MockSerial Serial;

inline uint32_t millis() { return 0; }
inline void delay(uint32_t) {}
inline void pinMode(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t) { return HIGH; }
inline void digitalWrite(uint8_t, uint8_t) {}
inline void tone(uint8_t, uint32_t, uint32_t) {}

#endif // MOCK_ARDUINO_H

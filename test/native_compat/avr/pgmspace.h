/**
 * @file avr/pgmspace.h (native test shim)
 * @brief Host-only stand-in for AVR's <avr/pgmspace.h>.
 *
 * ONLY on the include path for the PlatformIO `native` test environment (via
 * `-I test/native_compat`). On real AVR the genuine header is used.
 *
 * On the host there is no separate program-memory address space, so PROGMEM is
 * a no-op and the pgm_read_* accessors are plain dereferences. Everything is
 * `#ifndef`-guarded so this yields gracefully if ArduinoFake (or another shim)
 * already provides these symbols.
 */
#ifndef NATIVE_COMPAT_AVR_PGMSPACE_H
#define NATIVE_COMPAT_AVR_PGMSPACE_H

#include <stdint.h>

#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef PGM_P
#define PGM_P const char *
#endif
#ifndef PSTR
#define PSTR(s) (s)
#endif

#ifndef pgm_read_byte_near
#define pgm_read_byte_near(addr) (*(const uint8_t *)(addr))
#endif
#ifndef pgm_read_word_near
#define pgm_read_word_near(addr) (*(const uint16_t *)(addr))
#endif
#ifndef pgm_read_dword_near
#define pgm_read_dword_near(addr) (*(const uint32_t *)(addr))
#endif

#ifndef pgm_read_byte
#define pgm_read_byte(addr) pgm_read_byte_near(addr)
#endif
#ifndef pgm_read_word
#define pgm_read_word(addr) pgm_read_word_near(addr)
#endif
#ifndef pgm_read_dword
#define pgm_read_dword(addr) pgm_read_dword_near(addr)
#endif

#endif // NATIVE_COMPAT_AVR_PGMSPACE_H

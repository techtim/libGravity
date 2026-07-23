/**
 * @file arduino_compat.h (native test shim)
 * @brief Arduino core macros that ArduinoFake intentionally does not provide.
 *
 * ArduinoFake omits the Arduino min()/max() macros because they clash with
 * std::min/std::max in its own (and FakeIt's) sources. The firmware headers use
 * the Arduino forms, so define them here for the host build.
 *
 * Include this AFTER <ArduinoFake.h> (so the mock's own std::min/std::max are
 * already compiled) and BEFORE any firmware header that uses min()/max().
 */
#ifndef NATIVE_COMPAT_ARDUINO_COMPAT_H
#define NATIVE_COMPAT_ARDUINO_COMPAT_H

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#endif // NATIVE_COMPAT_ARDUINO_COMPAT_H

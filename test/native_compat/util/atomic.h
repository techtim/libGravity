/**
 * @file util/atomic.h (native test shim)
 * @brief Host-only stand-in for AVR's <util/atomic.h>.
 *
 * This file is ONLY on the include path for the PlatformIO `native` test
 * environment (via `-I test/native_compat`). On the real ATmega328 target the
 * genuine toolchain header is used instead.
 *
 * Unit tests run single-threaded with no ISR preemption, so ATOMIC_BLOCK just
 * needs to execute its trailing `{ ... }` block once. Expanding the macro to
 * nothing turns `ATOMIC_BLOCK(x) { ... }` into a plain scoped block.
 */
#ifndef NATIVE_COMPAT_UTIL_ATOMIC_H
#define NATIVE_COMPAT_UTIL_ATOMIC_H

#ifndef ATOMIC_RESTORESTATE
#define ATOMIC_RESTORESTATE 0
#endif
#ifndef ATOMIC_FORCEON
#define ATOMIC_FORCEON 0
#endif

// Discard the argument; the following `{ ... }` remains as an ordinary scope.
#ifndef ATOMIC_BLOCK
#define ATOMIC_BLOCK(type)
#endif

#ifndef NONATOMIC_BLOCK
#define NONATOMIC_BLOCK(type)
#endif
#ifndef NONATOMIC_RESTORESTATE
#define NONATOMIC_RESTORESTATE 0
#endif
#ifndef NONATOMIC_FORCEOFF
#define NONATOMIC_FORCEOFF 0
#endif

#endif // NATIVE_COMPAT_UTIL_ATOMIC_H

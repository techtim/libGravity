/**
 * @file debug_ram.h
 * @brief Optional free-stack probe, compiled in only with -DDEBUG_FREE_RAM.
 *
 * The stack grows down from RAMEND toward .bss; app is the last object there,
 * so a stack overflow reaches its trailing members (cv_cal, selected_save_slot)
 * before anything else. Painting the gap at boot and counting what is still
 * untouched gives the all-time low-water mark.
 *
 * Everything - the probe, the readout and their call sites - lives here so the
 * feature can be removed from the build with one flag. Both entry points are
 * macros so the disabled build carries no code and no declarations.
 */
#ifndef GRAVITYPLUS_DEBUG_RAM_H
#define GRAVITYPLUS_DEBUG_RAM_H

#ifdef DEBUG_FREE_RAM

static const uint8_t STACK_PAINT = 0xC5;

// Lowest address the stack may reach: the top of the heap if anything has
// allocated, otherwise the end of .bss. Scanning from .bss unconditionally
// would hit live heap bytes and report 0 free however much stack is left.
inline uint8_t *StackFloor() {
  extern int __heap_start;
  extern char *__brkval; // avr-libc declares it char*; 0 until the first malloc
  return (uint8_t *)(__brkval ? (void *)__brkval : (void *)&__heap_start);
}

// Call once, as early as possible: anything already on the stack is missed.
inline void PaintStack() {
  uint8_t here; // address of a local == roughly the current stack pointer
  for (uint8_t *p = StackFloor(); p < &here; ++p) {
    *p = STACK_PAINT;
  }
}

// Bytes of stack never touched since boot. 0 means the stack has reached app.
inline uint16_t StackHeadroom() {
  uint8_t *floor = StackFloor();
  uint8_t top; // ~current SP: everything at or above it is a live frame
  uint8_t *p = floor;
  // Bounded deliberately. A bare "while (*p == STACK_PAINT)" only ever stops
  // because live stack data happens to differ from the paint byte - with the
  // gap fully intact it would scan on past the stack entirely.
  while (p < &top && *p == STACK_PAINT) {
    ++p;
  }
  return (uint16_t)(p - floor);
}

// Readout, top right: the one strip nothing else uses - above the menu box
// (y >= 6) and right of the widest pattern strip.
inline void DrawFreeRam() {
  char buf[6];
  itoa(StackHeadroom(), buf, 10);
  gravity.display.setFont(TEXT_FONT);
  gravity.display.setDrawColor(1);
  gravity.display.drawStr(SCREEN_WIDTH - gravity.display.getStrWidth(buf) - 1,
                          5, buf);
  gravity.display.setDrawColor(2);
}

#define DEBUG_RAM_PAINT() PaintStack()
#define DEBUG_RAM_DRAW() DrawFreeRam()

#else // !DEBUG_FREE_RAM

#define DEBUG_RAM_PAINT() ((void)0)
#define DEBUG_RAM_DRAW() ((void)0)

#endif // DEBUG_FREE_RAM
#endif // GRAVITYPLUS_DEBUG_RAM_H

// ATTINY: 1634
#include "cfg-emisar-d4sv2-tintramp.h"

//-- willjow ------------------------------------------------------------------
#include "cfg-willjow-common.h"

#ifdef DEFAULT_TINT_STYLE
#undef DEFAULT_TINT_STYLE
#endif
#define DEFAULT_TINT_STYLE 1

#ifdef DEFAULT_TINT_STEPS
#undef DEFAULT_TINT_STEPS
#endif
#define DEFAULT_TINT_STEPS 7

#ifdef RGB_LED_OFF_DEFAULT
#undef RGB_LED_OFF_DEFAULT
#endif
#define RGB_LED_OFF_DEFAULT 0x19  // low, voltage

#ifndef USE_MOMENTARY_LOCKOUT_RGB_LED
#define USE_MOMENTARY_LOCKOUT_RGB_LED
#endif

#ifdef RGB_LED_LOCKOUT_DEFAULT
#undef RGB_LED_LOCKOUT_DEFAULT
#endif
#define RGB_LED_LOCKOUT_DEFAULT 0x50  // momentary high, red

#ifndef USE_LOWPASS_WHILE_ASLEEP
#define USE_LOWPASS_WHILE_ASLEEP
#endif

#ifdef DEFAULT_JUMP_START_LEVEL
#undef DEFAULT_JUMP_START_LEVEL
#endif
#define DEFAULT_JUMP_START_LEVEL 1

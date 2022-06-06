// ATTINY: 85
#include "cfg-sofirn-sp36.h"

//-- willjow ------------------------------------------------------------------
#include "cfg-willjow-common.h"

// the high button LED mode on this light uses too much power
// lockout: off (0)
// off mode: low (1)
#ifdef INDICATOR_LED_DEFAULT_MODE
#undef INDICATOR_LED_DEFAULT_MODE
#define INDICATOR_LED_DEFAULT_MODE 1
#endif

#ifdef USE_BEACON_MODE
#undef USE_BEACON_MODE
#endif

#ifndef USE_SOS_MODE
#define USE_SOS_MODE
#endif

#ifndef USE_SOS_MODE_IN_BLINKY_GROUP
#define USE_SOS_MODE_IN_BLINKY_GROUP  // put SOS in the blinkies mode group
#endif

#ifdef DEFAULT_MANUAL_MEMORY
#undef DEFAULT_MANUAL_MEMORY
#endif
#define DEFAULT_MANUAL_MEMORY DEFAULT_LEVEL

/*
 * misc.c: Misc extra functions for Anduril.
 *
 * Copyright (C) 2017 Selene Scriven
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MISC_C
#define MISC_C

#include "misc.h"

/* no longer used
void blink_confirm(uint8_t num) {
    uint8_t brightness = actual_level;
    uint8_t bump = actual_level + BLINK_BRIGHTNESS;
    if (bump > MAX_LEVEL) bump = 0;
    for (; num>0; num--) {
        set_level(bump);
        delay_4ms(10/4);
        set_level(brightness);
        if (num > 1) { delay_4ms(100/4); }
    }
}
*/

// make a short, visible pulse
// (either brighter or darker, depending on current brightness)
#ifndef BLINK_ONCE_TIME
#define BLINK_ONCE_TIME 10
#endif
#ifndef BLINK_BRIGHTNESS
#define BLINK_BRIGHTNESS (MAX_LEVEL/6)
#endif
void blink_once() {
    uint8_t brightness = actual_level;
    uint8_t bump = brightness + BLINK_BRIGHTNESS;
    if (bump > MAX_LEVEL) bump = 0;

    set_level(bump);
    delay_4ms(BLINK_ONCE_TIME/4);
    set_level(brightness);
}

// Just go dark for a moment to indicate to user that something happened
void blip() {
    uint8_t temp = actual_level;
    set_level(0);
    delay_4ms(3);
    set_level(temp);
}

#ifdef USE_CLOSED_FORM_NEAREST_LEVEL
float fmod(float a, float b) {
    return a - (uint8_t)(a / b) * b;
}
#endif

// find the ramp level closest to the target,
// using only the allowed levels (steps between floor and ceil inclusive)
// steps = 0 corresponds to smooth ramping
uint8_t nearest_level(int16_t target, uint8_t floor, uint8_t ceil, uint8_t steps) {
    // using int16_t here saves us a bunch of logic elsewhere,
    // by allowing us to correct for numbers < 0 or > 255 in one central place

    // special case for 1-step ramp... use halfway point between floor and ceiling
    if (1 == steps)
        return (ceil + floor) >> 1;

    if (target >= ceil) return ceil;
    if (target <= floor) return floor;
    if (!steps) return target;

    #if defined(USE_CLOSED_FORM_NEAREST_LEVEL)
    // ATtiny has no fpu, so we should avoid using float
    // (as nice as it would be to have a closed-form solution...)
    // this should also be the most accurate solution because we aren't
    // truncating
    float step_size = (float)(ceil - floor) / (steps - 1);
    float mod = fmod(target - floor, step_size);
    if ((2 * mod) > step_size)
        return target + (step_size - mod);
    return target - mod;

    #elif defined(USE_BINARY_SEARCH_NEAREST_LEVEL)
    // use binary search if we have space because the runtime is
    // theoretically better
    uint8_t ramp_range = ceil - floor;
    uint8_t step_radius = (ramp_range / (steps - 1)) >> 1;
    uint8_t guess = floor;
    uint8_t l = 0;
    uint8_t r = steps - 1;
    while (l <= r) {
        uint8_t i = (l + r) >> 1;
        guess = floor + (i * (uint16_t)ramp_range / (steps - 1));
        int16_t diff = target - guess;
        if (diff > step_radius)
            l = i + 1;
        else if (diff < -step_radius)
            r = i - 1;
        else
            return guess;
    }
    // we can end up here because guess and step_radius are truncated
    return guess;

    #else
    // linear search implementation uses ~40B less program space than binary
    // search
    uint8_t ramp_range = ceil - floor;
    uint8_t step_radius = (ramp_range / (steps - 1)) >> 1;
    uint8_t guess = floor;

    for(uint8_t i=0; i<steps; i++) {
        guess = floor + (i * (uint16_t)ramp_range / (steps - 1));
        int16_t diff = target - guess;
        if (diff < 0) diff = -diff;
        if (diff <= step_radius)
            return guess;
    }
    return guess;
    #endif
}


#endif


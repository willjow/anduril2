/*
 * tint-ramping.c: Tint ramping functions for Anduril.
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

#ifndef TINT_RAMPING_C
#define TINT_RAMPING_C

#include "tint-ramping.h"

uint8_t tint_ramping_state(Event event, uint16_t arg) {
    static int8_t tint_ramp_direction = TINT_3H_DIRECTION;
    static uint8_t prev_tint = 0;
    // don't activate auto-tint modes unless the user hits the edge
    // and keeps pressing for a while
    static uint8_t past_edge_counter = 0;
    // bugfix: click-click-hold from off to strobes would invoke tint ramping
    // in addition to changing state...  so ignore any tint-ramp events which
    // don't look like they were meant to be here
    static uint8_t active = 0;

    uint8_t style_tint = tint_style;
    #ifdef USE_1H_STYLE_CONFIG
    uint8_t style_1h = ramp_1h_style;
    #endif

    uint8_t step_size = 1;
    if (style_tint
        && tint_steps > 1
        && (0 < prev_tint && prev_tint < 255))  // allow step from auto to edge
        step_size = 253 / (tint_steps - 1);

    // click, click, hold: change the tint
    // click, click, click, hold: change the tint downward
    if ((event == EV_click3_hold) || (event == EV_click4_hold)) {
        // reset at beginning of movement
        if (! arg) {
            active = 1;  // first frame means this is for us
            past_edge_counter = 0;  // doesn't start until user hits the edge
            // fix ramp direction on first frame if necessary
            if (event == EV_click4_hold) {
                tint_ramp_direction = TINT_4H_DIRECTION;
            }
            else if (tint >= 254) {
                tint_ramp_direction = TINT_3H_DIRECTION;
            }
            else if (tint <= 1) {
                #ifdef USE_1H_STYLE_CONFIG
                if (!style_1h)
                #endif
                tint_ramp_direction = TINT_4H_DIRECTION;
            }
        }
        // ignore event if we weren't the ones who handled the first frame
        if (! active) return EVENT_HANDLED;

        // ramp slower in discrete mode
        if (style_tint && (arg % HOLD_TIMEOUT != 0)) {
            return MISCHIEF_MANAGED;
        }

        // change normal tints
        tint = nearest_tint_level(tint + (int16_t)tint_ramp_direction * step_size);

        // if the user kept pressing long enough, go the final step
        if ((tint == 1 || tint == 254)
            && ((!style_tint && past_edge_counter == past_edge_timeout)
                || (style_tint && past_edge_counter == PAST_EDGE_TIMEOUT_FACTOR))) {
            past_edge_counter ++;
            tint ^= 1;  // 1 -> 0, 254 -> 255
            blip();
        }
        // if tint change stalled, let user know we hit the edge
        else if (prev_tint == tint) {
            if (past_edge_counter == 0) blip();
            // count up but don't wrap back to zero
            if (past_edge_counter < 255) past_edge_counter ++;
        }
        prev_tint = tint;
        set_level(actual_level);
        return EVENT_HANDLED;
    }

    // click, click, hold, release: reverse direction for next ramp
    else if ((event == EV_click3_hold_release)
             || (event == EV_click4_hold_release)) {
        active = 0;  // ignore next hold if it wasn't meant for us

        // reverse
        #ifdef USE_1H_STYLE_CONFIG
        if ((event == EV_click4_hold_release) || !style_1h)
        #endif
        tint_ramp_direction = -tint_ramp_direction;

        #ifdef START_AT_MEMORIZED_TINT
        // remember tint after battery change
        save_config_wl();
        #endif
        // bug?: for some reason, brightness can seemingly change
        // from 1/150 to 2/150 without this next line... not sure why
        set_level(actual_level);
        return EVENT_HANDLED;
    }

    // 5 clicks: toggle between smooth/stepped tint ramping
    else if (event == EV_5clicks) {
        tint_style = !style_tint;
        #ifdef MEMORIZE_TINT_STYLE
        save_config();
        #endif
        #ifdef START_AT_MEMORIZED_TINT
        // remember tint after battery change
        save_config_wl();
        #endif
        blip();
        tint = nearest_tint_level(tint);
        set_level(actual_level);
        return EVENT_HANDLED;
    }

    // we can't use AUTO_REVERSE_TIME for tint ramping in the same manner as in
    // ramp-mode because EV_tick events will be consumed there

    return EVENT_NOT_HANDLED;
}

uint8_t nearest_tint_level(int16_t target) {
    if (tint == 0 && target < 0) return 0;
    if (tint == 255 && target > 255) return 255;
    return nearest_level(target, 1, 254, tint_steps * tint_style);
}

#endif


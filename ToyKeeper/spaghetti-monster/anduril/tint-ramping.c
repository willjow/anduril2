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
    static int8_t tint_ramp_direction = -1;
    static uint8_t prev_tint = 0;
    // don't activate auto-tint modes unless the user hits the edge
    // and keeps pressing for a while
    static uint8_t past_edge_counter = 0;
    // bugfix: click-click-hold from off to strobes would invoke tint ramping
    // in addition to changing state...  so ignore any tint-ramp events which
    // don't look like they were meant to be here
    static uint8_t active = 0;

    #ifdef USE_1H_STYLE_CONFIG
    uint8_t style_1h = ramp_1h_style;
    #endif

    // click, click, hold: change the tint
    // click, click, click, hold: change the tint downward
    if ((event == EV_click3_hold) || (event == EV_click4_hold)) {
        ///// tint-toggle mode
        // toggle once on first frame; ignore other frames
        if (tint_style) {
            // only respond on first frame
            if (arg) return EVENT_NOT_HANDLED;

            // force tint to be 1 or 254
            if ((event == EV_click4_hold) || (tint != 1)) { tint = 254;}

            if (event == EV_click3_hold) {
                #ifdef USE_1H_STYLE_CONFIG
                if (style_1h) { tint = 1; }
                else { tint ^= 0xFF; }  // invert between 1 and 254
                #else
                tint ^= 0xFF;  // invert between 1 and 254
                #endif
            }
            set_level(actual_level);
            return EVENT_HANDLED;
        }

        ///// smooth tint-ramp mode
        // reset at beginning of movement
        if (! arg) {
            active = 1;  // first frame means this is for us
            past_edge_counter = 0;  // doesn't start until user hits the edge
            // fix ramp direction on first frame if necessary
            if (event == EV_click4_hold) {
                tint_ramp_direction = 1;
            }
            else if (tint >= 254) {
                tint_ramp_direction = -1;
            }
            else if (tint <= 1) {
                #ifdef USE_1H_STYLE_CONFIG
                if (!style_1h) {
                    tint_ramp_direction = 1;
                }
                #else
                tint_ramp_direction = 1;
                #endif
            }
        }
        // ignore event if we weren't the ones who handled the first frame
        if (! active) return EVENT_HANDLED;

        // change normal tints
        if ((tint_ramp_direction > 0) && (tint < 254)) {
            tint += 1;
        }
        else if ((tint_ramp_direction < 0) && (tint > 1)) {
            tint -= 1;
        }
        // if the user kept pressing long enough, go the final step
        if (past_edge_counter == 64) {
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
        if ((event == EV_click4_hold_release) || !style_1h) {
            tint_ramp_direction = -tint_ramp_direction;
        }
        #else
        tint_ramp_direction = -tint_ramp_direction;
        #endif
        // remember tint after battery change
        save_config_wl();
        return EVENT_HANDLED;
    }

    // 5 clicks: go to/from the middle tint
    else if (event == EV_5clicks) {
        if (tint != 127) {
            tint = 127;
        }
        else {
            tint = prev_tint;
        }
        set_level(actual_level);
        // remember tint after battery change
        save_config_wl();
        return EVENT_HANDLED;
    }

    // we can't use AUTO_REVERSE_TIME for tint ramping the same manner as in
    // ramp-mode because EV_tick events will be consumed there

    return EVENT_NOT_HANDLED;
}


#endif


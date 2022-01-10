/*
 * lockout-mode.c: Lockout mode for Anduril.
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

#ifndef LOCKOUT_MODE_C
#define LOCKOUT_MODE_C

#include "lockout-mode.h"

uint8_t lockout_state(Event event, uint16_t arg) {
    #ifdef USE_MOON_DURING_LOCKOUT_MODE
    // momentary(ish) moon mode during lockout
    // button is being held

    #ifdef USE_AUX_RGB_LEDS
    uint8_t aux_mode = rgb_led_lockout_mode;
    uint8_t aux_pattern = aux_mode >> 4;
    uint8_t aux_color = aux_mode & 0x0f;
    #endif

    // 4 clicks, but hold last: exit and start at floor
    // handle this up here so that we don't strobe between nearest_level(1) and
    // the lowest floor while holding 4H
    if (event == EV_click4_hold) {
        if (arg == 0) blip();
        set_level(nearest_level(1));
        #ifdef USE_RAMP_AFTER_MOON_CONFIG
        if (dont_ramp_after_moon) {
            current_event = EV_none;
            set_state(steady_state, 1);
        }
        // don't start ramping immediately;
        // give the user time to release at moon level
        else if (arg >= HOLD_TIMEOUT) {
            current_event = EV_click1_hold;
            set_state(steady_state, 1);
        }
        #else
        // reset button sequence to avoid activating anything in ramp mode
        current_event = EV_none;
        // ... and back to ramp mode
        set_state(steady_state, 1);
        #endif
        return MISCHIEF_MANAGED;
    }
    #ifdef USE_RAMP_AFTER_MOON_CONFIG
    else if (event == EV_click4_hold_release) {
        // in the case that we release before the ramp timeout, change state
        // without ramping
        current_event = EV_none;
        set_state(steady_state, 1);
        return MISCHIEF_MANAGED;
    }
    #endif
    #ifdef USE_AUX_RGB_LEDS
    // don't turn on during RGB aux LED configuration
    else if (event == EV_click7_hold) { set_level(0); }
    #endif
    else if ((event & (B_CLICK | B_PRESS)) == (B_CLICK | B_PRESS)) {
        // hold: lowest floor
        // click, hold: highest floor (or manual mem level)
        uint8_t min_floor = ramp_floors[0];
        uint8_t max_floor = ramp_floors[1];
        if (min_floor > max_floor) {
            uint8_t temp = min_floor;
            min_floor = max_floor;
            max_floor = temp;
        }

        if ((event & 0x0f) == 2) {  // second click
            #ifdef USE_MANUAL_MEMORY
            if (manual_memory) {
                set_level(manual_memory);
            } else
            #endif
            {
                #ifdef USE_MOMENTARY_LOCKOUT_RGB_LED
                if (aux_pattern == 4 || aux_pattern == 5) {
                    set_level(min_floor);
                } else
                #endif
                { set_level(max_floor); }
            }
        } else {  // anything except second click
            #ifdef USE_MOMENTARY_LOCKOUT_RGB_LED
            if (aux_pattern == 4 || aux_pattern == 5) {  // momentary low/high aux
                uint8_t actual_color = pgm_read_byte(rgb_led_colors + aux_color);
                actual_color = actual_color << (aux_pattern - 4);  // set level
                rgb_led_set(actual_color);
            } else
            #endif
            { set_level(min_floor); }
        }
    }
    // button was released
    else if ((event & (B_CLICK | B_PRESS)) == (B_CLICK)) {
        set_level(0);  // should also turn off aux leds
    }
    #endif  // ifdef USE_MOON_DURING_LOCKOUT_MODE

    // regular event handling
    // conserve power while locked out
    // (allow staying awake long enough to exit, but otherwise
    //  be persistent about going back to sleep every few seconds
    //  even if the user keeps pressing the button)
    #ifdef USE_INDICATOR_LED
    if (event == EV_enter_state) {
        indicator_led(indicator_led_mode >> 2);
    } else
    #elif defined(USE_AUX_RGB_LEDS)
    if (event == EV_enter_state) {
        rgb_led_update(aux_mode, 0);
    } else
    #endif
    if (event == EV_tick) {
        if (arg > HOLD_TIMEOUT) {
            go_to_standby = 1;
            #ifdef USE_INDICATOR_LED
            indicator_led(indicator_led_mode >> 2);
            #elif defined(USE_AUX_RGB_LEDS)
            rgb_led_update(aux_mode, arg);
            #endif
        }
        return MISCHIEF_MANAGED;
    }
    #if defined(TICK_DURING_STANDBY) && (defined(USE_INDICATOR_LED) || defined(USE_AUX_RGB_LEDS))
    else if (event == EV_sleep_tick) {
        #if defined(USE_INDICATOR_LED)
        if ((indicator_led_mode & 0b00001100) == 0b00001100) {
            indicator_blink(arg);
        }
        #elif defined(USE_AUX_RGB_LEDS)
        rgb_led_update(aux_mode, arg);
        #endif
        return MISCHIEF_MANAGED;
    }
    #endif
    #ifdef USE_TINT_RAMPING
    // disable tint ramping during lockout
    else if (event == EV_click3_hold) {
        return MISCHIEF_MANAGED;
    }
    #endif
    // 4 clicks: exit and turn on
    else if (event == EV_4clicks) {
        #ifdef USE_MANUAL_MEMORY
        // FIXME: memory timer is totally ignored
        if (manual_memory)
            set_state(steady_state, manual_memory);
        else
        #endif
        set_state(steady_state, memorized_level);
        return MISCHIEF_MANAGED;
    }

    // 5 clicks: exit and turn on at ceiling level
    else if (event == EV_5clicks) {
        set_state(steady_state, MAX_LEVEL);
        return MISCHIEF_MANAGED;
    }

    ////////// Every action below here is blocked in the simple UI //////////
    #ifdef USE_SIMPLE_UI
    if (simple_ui_active) {
        return EVENT_NOT_HANDLED;
    }
    #endif

    #ifdef USE_AUTOLOCK
    // 10H: configure the autolock option
    else if (event == EV_click10_hold) {
        push_state(lockout_config_state, 0);
        return MISCHIEF_MANAGED;
    }
    #endif

    #if defined(USE_INDICATOR_LED)
    // 7 clicks: rotate through indicator LED modes (lockout mode)
    else if (event == EV_7clicks) {
        #if defined(USE_INDICATOR_LED)
            uint8_t mode = indicator_led_mode >> 2;
            #ifdef TICK_DURING_STANDBY
            mode = (mode + 1) & 3;
            #else
            mode = (mode + 1) % 3;
            #endif
            #ifdef INDICATOR_LED_SKIP_LOW
            if (mode == 1) { mode ++; }
            #endif
            indicator_led_mode = (mode << 2) + (indicator_led_mode & 0x03);
            indicator_led(mode);
        #elif defined(USE_AUX_RGB_LEDS)
        #endif
        save_config();
        return MISCHIEF_MANAGED;
    }
    #elif defined(USE_AUX_RGB_LEDS)
    // 7 clicks: change RGB aux LED pattern
    else if (event == EV_7clicks) {
        uint8_t pattern = (aux_pattern + 1) % RGB_LED_LOCKOUT_NUM_PATTERNS;
        rgb_led_lockout_mode = (pattern << 4) | aux_color;
        rgb_led_update(rgb_led_lockout_mode, 0);
        save_config();
        blink_once();
        return MISCHIEF_MANAGED;
    }
    // 7H: change RGB aux LED color
    else if (event == EV_click7_hold) {
        setting_rgb_mode_now = 1;
        if (0 == (arg & 0x3f)) {
            uint8_t color = (aux_color + 1) % RGB_LED_NUM_COLORS;
            rgb_led_lockout_mode = (aux_mode & 0xf0) | color;
            //save_config();
        }
        rgb_led_update(rgb_led_lockout_mode, arg);
        return MISCHIEF_MANAGED;
    }
    // 7H, release: save new color
    else if (event == EV_click7_hold_release) {
        setting_rgb_mode_now = 0;
        save_config();
        return MISCHIEF_MANAGED;
    }
    #endif

    return EVENT_NOT_HANDLED;
}

#ifdef USE_AUTOLOCK
void lockout_config_save(uint8_t step, uint8_t value) {
    if (1 == step) {
        // set the auto-lock timer to N minutes, where N is the number of clicks
        autolock_time = value;
    }
}

uint8_t lockout_config_state(Event event, uint16_t arg) {
    return config_state_base(event, arg, 1, lockout_config_save);
}
#endif


#endif


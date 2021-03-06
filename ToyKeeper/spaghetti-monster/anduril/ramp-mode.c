/*
 * ramp-mode.c: Ramping functions for Anduril.
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

#ifndef RAMP_MODE_C
#define RAMP_MODE_C

#include "ramp-mode.h"

#ifdef USE_SUNSET_TIMER
#include "sunset-timer.h"
#endif

uint8_t steady_state(Event event, uint16_t arg) {
    static int8_t ramp_direction = 1;
    #if (B_TIMING_OFF == B_RELEASE_T)
    // if the user double clicks, we need to abort turning off,
    // and this stores the level to return to
    static uint8_t level_before_off = 0;
    #endif

    uint8_t style_ramp = ramp_style;
    uint8_t mode_min = ramp_floors[style_ramp];
    uint8_t mode_max = ramp_ceils[style_ramp];
    uint8_t step_size = 1;
    if (style_ramp) {
        #ifdef USE_SIMPLE_UI
        uint8_t steps = ramp_stepss[1 + simple_ui_active];
        #else
        uint8_t steps = ramp_stepss[1];
        #endif
        if (steps > 1)
            step_size = (mode_max - mode_min) / (steps - 1);
    }

    // how bright is "turbo"?
    uint8_t turbo_level;
    #if defined(USE_2C_STYLE_CONFIG)  // user can choose 2C behavior
        uint8_t style_2c = ramp_2c_style;
        #ifdef USE_SIMPLE_UI
        // simple UI has its own turbo config
        if (simple_ui_active) style_2c = ramp_2c_style_simple;
        #endif
        // 0 = no turbo
        // 1 = Anduril 1 direct to turbo
        // 2 = Anduril 2 direct to ceiling, or turbo if already at ceiling
        if (0 == style_2c) turbo_level = mode_max;
        else if (1 == style_2c) turbo_level = MAX_LEVEL;
        else {
            if (memorized_level < mode_max) { turbo_level = mode_max; }
            else { turbo_level = MAX_LEVEL; }
        }
    #elif defined(USE_2C_MAX_TURBO)  // Anduril 1 style always
        // simple UI: to/from ceiling
        // full UI: to/from turbo (Anduril1 behavior)
        #ifdef USE_SIMPLE_UI
        if (simple_ui_active) turbo_level = mode_max;
        else
        #endif
        turbo_level = MAX_LEVEL;
    #else  // Anduril 2 style always
        // simple UI: to/from ceiling
        // full UI: to/from ceiling if mem < ceiling,
        //          or to/from turbo if mem >= ceiling
        if ((memorized_level < mode_max)
            #ifdef USE_SIMPLE_UI
            || simple_ui_active
            #endif
           ) { turbo_level = mode_max; }
        else { turbo_level = MAX_LEVEL; }
    #endif

    #ifdef USE_1H_STYLE_CONFIG
    uint8_t style_1h = ramp_1h_style;
    #endif

    #ifdef USE_SUNSET_TIMER
    // handle the shutoff timer first
    static uint8_t timer_orig_level = 0;
    uint8_t sunset_active = sunset_timer;  // save for comparison
    // clock tick
    sunset_timer_state(event, arg);
    // if the timer was just turned on
    if (sunset_timer  &&  (! sunset_active)) {
        timer_orig_level = actual_level;
    }
    // if the timer just expired, shut off
    else if (sunset_active  &&  (! sunset_timer)) {
        set_state(off_state, 0);
        return MISCHIEF_MANAGED;
    }
    #endif  // ifdef USE_SUNSET_TIMER

    // turn LED on when we first enter the mode
    if ((event == EV_enter_state) || (event == EV_reenter_state)) {
        #if defined(USE_MOMENTARY_MODE) && defined(USE_STROBE_STATE)
        momentary_mode = 0;  // 0 = ramping, 1 = strobes
        #endif
        // if we just got back from config mode, go back to memorized level
        if (event == EV_reenter_state) {
            arg = memorized_level;
        }
        // remember this level, unless it's moon or turbo
        if ((arg > mode_min) && (arg < mode_max))
            memorized_level = arg;
        // use the requested level even if not memorized
        arg = nearest_ramp_level(arg);
        set_level_and_therm_target(arg);
        ramp_direction = 1;
        return MISCHIEF_MANAGED;
    }
    #if (B_TIMING_OFF == B_RELEASE_T)
    // 1 click (early): off, if configured for early response
    else if (event == EV_click1_release) {
        level_before_off = actual_level;
        set_level_and_therm_target(0);
        return MISCHIEF_MANAGED;
    }
    // 2 clicks (early): abort turning off, if configured for early response
    else if (event == EV_click2_press) {
        set_level_and_therm_target(level_before_off);
        return MISCHIEF_MANAGED;
    }
    #endif  // if (B_TIMING_OFF == B_RELEASE_T)
    // 1 click: off
    else if (event == EV_1click) {
        set_state(off_state, 0);
        return MISCHIEF_MANAGED;
    }
    // 2 clicks: go to/from highest level
    else if (event == EV_2clicks) {
        if (actual_level < turbo_level) {
            set_level_and_therm_target(turbo_level);
        }
        else {
            set_level_and_therm_target(memorized_level);
        }
        #ifdef USE_SUNSET_TIMER
        timer_orig_level = actual_level;
        #endif
        return MISCHIEF_MANAGED;
    }

    #ifdef USE_LOCKOUT_MODE
    // 4 clicks: shortcut to lockout mode
    else if (event == EV_4clicks) {
        set_level(0);
        set_state(lockout_state, 0);
        return MISCHIEF_MANAGED;
    }
    #endif

    // hold: change brightness (brighter, dimmer)
    // click, hold: change brightness (dimmer)
    else if ((event == EV_click1_hold) || (event == EV_click2_hold)) {
        // ramp slower in discrete mode
        if (style_ramp  &&  (arg % HOLD_TIMEOUT != 0)) {
            return MISCHIEF_MANAGED;
        }
        #ifdef USE_RAMP_SPEED_CONFIG
        // ramp slower if user configured things that way
        if ((! style_ramp) && (arg % ramp_speed)) {
            return MISCHIEF_MANAGED;
        }
        #endif
        // fix ramp direction on first frame if necessary
        if (!arg) {
            // click, hold should always go down if possible
            if (event == EV_click2_hold) {
                ramp_direction = -1;
            }
            // make it ramp down instead, if already at max
            else if (actual_level >= mode_max) {
                #ifdef USE_1H_STYLE_CONFIG
                if (!style_1h) {
                    ramp_direction = -1;
                }
                #ifdef BLINK_AT_RAMP_CEIL
                else {
                    blip();
                }
                #endif
                #else
                ramp_direction = -1;
                #endif
            }
            // make it ramp up if already at min
            // (off->hold->stepped_min->release causes this state)
            else if (actual_level <= mode_min) {
                ramp_direction = 1;
            }
        }
        // if the button is stuck, err on the side of safety and ramp down
        else if ((arg > TICKS_PER_SECOND * 5
                    #ifdef USE_RAMP_SPEED_CONFIG
                    // FIXME: count from time actual_level hits mode_max,
                    //   not from beginning of button hold
                    * ramp_speed
                    #endif
                    ) && (actual_level >= mode_max)) {
            ramp_direction = -1;
        }
        #ifdef USE_LOCKOUT_MODE
        // if the button is still stuck, lock the light
        else if ((arg > TICKS_PER_SECOND * 10
                    #ifdef USE_RAMP_SPEED_CONFIG
                    // FIXME: count from time actual_level hits mode_min,
                    //   not from beginning of button hold
                    * ramp_speed
                    #endif
                    ) && (actual_level <= mode_min)) {
            blink_once();
            set_state(lockout_state, 0);
        }
        #endif
        memorized_level = nearest_ramp_level(actual_level \
                          + (int16_t)ramp_direction * step_size);
        #if defined(BLINK_AT_RAMP_MIDDLE) \
            || defined(BLINK_AT_RAMP_CEIL) \
            || defined(BLINK_AT_RAMP_FLOOR)
        // only blink once for each threshold
        if ((memorized_level != actual_level) && (
                0  // for easier syntax below
                #ifdef BLINK_AT_RAMP_MIDDLE_1
                || (memorized_level == BLINK_AT_RAMP_MIDDLE_1)
                #endif
                #ifdef BLINK_AT_RAMP_MIDDLE_2
                || (memorized_level == BLINK_AT_RAMP_MIDDLE_2)
                #endif
                #ifdef BLINK_AT_RAMP_CEIL
                || (memorized_level == mode_max)
                #endif
                #ifdef BLINK_AT_RAMP_FLOOR
                || (memorized_level == mode_min)
                #endif
                )) {
            blip();
        }
        #endif
        #if defined(BLINK_AT_STEPS)
        ramp_style = 1;
        uint8_t nearest = nearest_ramp_level(actual_level);
        ramp_style = style_ramp;
        // only blink once for each threshold
        if ((memorized_level != actual_level) &&
                    (style_ramp == 0) &&
                    (memorized_level == nearest)
                    )
        {
            blip();
        }
        #endif
        set_level_and_therm_target(memorized_level);
        #ifdef USE_SUNSET_TIMER
        timer_orig_level = actual_level;
        #endif
        return MISCHIEF_MANAGED;
    }
    // reverse ramp direction on hold release
    else if ((event == EV_click1_hold_release)
             || (event == EV_click2_hold_release)) {
        #ifdef USE_1H_STYLE_CONFIG
        if ((event == EV_click2_hold_release) || !style_1h)
        #endif
        ramp_direction = -ramp_direction;

        #ifdef START_AT_MEMORIZED_LEVEL
        save_config_wl();
        #endif
        return MISCHIEF_MANAGED;
    }

    else if (event == EV_tick) {
        // un-reverse after 1 second
        if (arg == AUTO_REVERSE_TIME) ramp_direction = 1;

        #ifdef USE_SUNSET_TIMER
        // reduce output if shutoff timer is active
        if (sunset_timer) {
            uint8_t dimmed_level = timer_orig_level * (sunset_timer-1) / sunset_timer_peak;
            if (dimmed_level < 1) dimmed_level = 1;
            #ifdef USE_SET_LEVEL_GRADUALLY
            set_level_gradually(dimmed_level);
            target_level = dimmed_level;
            #else
            set_level_and_therm_target(dimmed_level);
            #endif
        }
        #endif  // ifdef USE_SUNSET_TIMER

        #ifdef USE_SET_LEVEL_GRADUALLY
        int16_t diff = gradual_target - actual_level;
        static uint16_t ticks_since_adjust = 0;
        ticks_since_adjust++;
        if (diff) {
            uint16_t ticks_per_adjust = 256;
            if (diff < 0) {
                //diff = -diff;
                if (actual_level > THERM_FASTER_LEVEL) {
                    #ifdef THERM_HARD_TURBO_DROP
                    ticks_per_adjust >>= 2;
                    #endif
                    ticks_per_adjust >>= 2;
                }
            } else {
                // rise at half speed
                ticks_per_adjust <<= 1;
            }
            while (diff) {
                ticks_per_adjust >>= 1;
                //diff >>= 1;
                diff /= 2;  // because shifting produces weird behavior
            }
            if (ticks_since_adjust > ticks_per_adjust)
            {
                gradual_tick();
                ticks_since_adjust = 0;
            }
        }
        #endif  // ifdef USE_SET_LEVEL_GRADUALLY
        return MISCHIEF_MANAGED;
    }

    #ifdef USE_THERMAL_REGULATION
    // overheating: drop by an amount proportional to how far we are above the ceiling
    else if (event == EV_temperature_high) {
        #if 0
        blip();
        #endif
        #ifdef THERM_HARD_TURBO_DROP
        //if (actual_level > THERM_FASTER_LEVEL) {
        if (actual_level == MAX_LEVEL) {
            #ifdef USE_SET_LEVEL_GRADUALLY
            set_level_gradually(THERM_FASTER_LEVEL);
            target_level = THERM_FASTER_LEVEL;
            #else
            set_level_and_therm_target(THERM_FASTER_LEVEL);
            #endif
        } else
        #endif
        if (actual_level > MIN_THERM_STEPDOWN) {
            int16_t stepdown = actual_level - arg;
            if (stepdown < MIN_THERM_STEPDOWN) stepdown = MIN_THERM_STEPDOWN;
            else if (stepdown > MAX_LEVEL) stepdown = MAX_LEVEL;
            #ifdef USE_SET_LEVEL_GRADUALLY
            set_level_gradually(stepdown);
            #else
            set_level(stepdown);
            #endif
        }
        return MISCHIEF_MANAGED;
    }
    // underheating: increase slowly if we're lower than the target
    //               (proportional to how low we are)
    else if (event == EV_temperature_low) {
        #if 0
        blip();
        #endif
        if (actual_level < target_level) {
            //int16_t stepup = actual_level + (arg>>1);
            int16_t stepup = actual_level + arg;
            if (stepup > target_level) stepup = target_level;
            else if (stepup < MIN_THERM_STEPDOWN) stepup = MIN_THERM_STEPDOWN;
            #ifdef USE_SET_LEVEL_GRADUALLY
            set_level_gradually(stepup);
            #else
            set_level(stepup);
            #endif
        }
        return MISCHIEF_MANAGED;
    }
    #ifdef USE_SET_LEVEL_GRADUALLY
    // temperature is within target window
    // (so stop trying to adjust output)
    else if (event == EV_temperature_okay) {
        // if we're still adjusting output...  stop after the current step
        if (gradual_target > actual_level)
            gradual_target = actual_level + 1;
        else if (gradual_target < actual_level)
            gradual_target = actual_level - 1;
        return MISCHIEF_MANAGED;
    }
    #endif  // ifdef USE_SET_LEVEL_GRADUALLY
    #endif  // ifdef USE_THERMAL_REGULATION

    ////////// Every action below here is blocked in the simple UI //////////
    // That is, unless we specifically want to enable 3C for smooth/stepped selection in Simple UI
    #if defined(USE_SIMPLE_UI) && !defined(USE_SIMPLE_UI_RAMPING_TOGGLE)
    if (simple_ui_active) {
        return EVENT_NOT_HANDLED;
    }
    #endif

    // 3 clicks: toggle smooth vs discrete ramping
    else if (event == EV_3clicks) {
        ramp_style = !ramp_style;
        #ifdef MEMORIZE_RAMP_STYLE
        save_config();
        #endif
        #ifdef START_AT_MEMORIZED_LEVEL
        save_config_wl();
        #endif
        blip();
        memorized_level = nearest_ramp_level(actual_level);
        set_level_and_therm_target(memorized_level);
        #ifdef USE_SUNSET_TIMER
        timer_orig_level = actual_level;
        #endif
        return MISCHIEF_MANAGED;
    }

    // If we allowed 3C in Simple UI, now block further actions
    #if defined(USE_SIMPLE_UI) && defined(USE_SIMPLE_UI_RAMPING_TOGGLE)
    if (simple_ui_active) {
        return EVENT_NOT_HANDLED;
    }
    #endif

    #ifndef USE_TINT_RAMPING
    // 3H: momentary turbo (on lights with no tint ramping)
    else if (event == EV_click3_hold) {
        if (! arg) {  // first frame only, to allow thermal regulation to work
            #ifdef USE_2C_STYLE_CONFIG
            uint8_t tl = style_2c ? MAX_LEVEL : turbo_level;
            set_level_and_therm_target(tl);
            #else
            set_level_and_therm_target(turbo_level);
            #endif
        }
        return MISCHIEF_MANAGED;
    }
    else if (event == EV_click3_hold_release) {
        set_level_and_therm_target(memorized_level);
        return MISCHIEF_MANAGED;
    }
    #endif  // ifndef USE_TINT_RAMPING

    #if !defined(USE_TINT_RAMPING) && defined(USE_MOMENTARY_MODE)
    // 5 clicks: shortcut to momentary mode (on lights with no tint ramping)
    else if (event == EV_5clicks) {
        set_level(0);
        set_state(momentary_state, 0);
        return MISCHIEF_MANAGED;
    }
    #endif

    #ifdef USE_RAMP_CONFIG
    // 7H: configure this ramp mode
    else if (event == EV_click7_hold) {
        push_state(ramp_config_state, 0);
        return MISCHIEF_MANAGED;
    }
    #endif

    #ifdef USE_MANUAL_MEMORY
    else if (event == EV_10clicks) {
        // turn on manual memory and save current brightness
        manual_memory = actual_level;
        #ifdef USE_TINT_RAMPING
        manual_memory_tint = tint;  // remember tint too
        #endif
        save_config();
        blink_once();
        return MISCHIEF_MANAGED;
    }
    else if (event == EV_click10_hold) {
        #ifdef USE_RAMP_EXTRAS_CONFIG
        // let user configure a bunch of extra ramp options
        push_state(ramp_extras_config_state, 0);
        #else  // manual mem, but no timer
        // turn off manual memory; go back to automatic
        if (0 == arg) {
            manual_memory = 0;
            save_config();
            blink_once();
        }
        #endif
        return MISCHIEF_MANAGED;
    }
    #endif  // ifdef USE_MANUAL_MEMORY

    return EVENT_NOT_HANDLED;
}


#ifdef USE_RAMP_CONFIG
void ramp_config_save(uint8_t step, uint8_t value) {

    // 0 = smooth ramp, 1 = stepped ramp, 2 = simple UI's ramp
    uint8_t ramp_num = ramp_style;
    #ifdef USE_SIMPLE_UI
    if (current_state == simple_ui_config_state) ramp_num = 2;
    #endif

    #if defined(USE_SIMPLE_UI) && defined(USE_2C_STYLE_CONFIG)
    // simple UI config is weird...
    // has some ramp extras after floor/ceil/steps
    if (4 == step) {
        ramp_2c_style_simple = value;
    }
    else
    #endif

    // save adjusted value to the correct slot
    if (value) {
        if (1 == step) {
            if (value <= ramp_ceils[ramp_num])
                ramp_floors[ramp_num] = value;
        }
        else if (2 == step) {
            // subtract from MAX_LEVEL for ceiling
            value = MAX_LEVEL + 1 - value;
            if (value <= MAX_LEVEL && value >= ramp_floors[ramp_num])
                ramp_ceils[ramp_num] = value;
        }
        else if (3 == step) {
            ramp_stepss[ramp_num] = value;
        }
    }
}

uint8_t ramp_config_state(Event event, uint16_t arg) {
    #ifdef USE_RAMP_SPEED_CONFIG
    const uint8_t num_config_steps = 3;
    #else
    uint8_t num_config_steps = ramp_style + 2;
    #endif
    return config_state_base(event, arg,
                             num_config_steps, ramp_config_save);
}

#ifdef USE_SIMPLE_UI
uint8_t simple_ui_config_state(Event event, uint16_t arg) {
    #if defined(USE_2C_STYLE_CONFIG)
    #define SIMPLE_UI_NUM_MENU_ITEMS 4
    #else
    #define SIMPLE_UI_NUM_MENU_ITEMS 3
    #endif
    return config_state_base(event, arg,
                             SIMPLE_UI_NUM_MENU_ITEMS,
                             ramp_config_save);
}
#endif
#endif  // #ifdef USE_RAMP_CONFIG

#ifdef USE_RAMP_EXTRAS_CONFIG
void ramp_extras_config_save(uint8_t step, uint8_t value) {
    // item 1: disable manual memory, go back to automatic
    if (1 == step) { manual_memory = 0; }

    #ifdef USE_MANUAL_MEMORY_TIMER
    // item 2: set manual memory timer duration
    // FIXME: should be limited to (65535 / SLEEP_TICKS_PER_MINUTE)
    //   to avoid overflows or impossibly long timeouts
    //   (by default, the effective limit is 145, but it allows up to 255)
    else if (2 == step) { manual_memory_timer = value; }
    #endif

    #ifdef USE_RAMP_AFTER_MOON_CONFIG
    // item 3: ramp up after hold-from-off for moon?
    // 0 = yes, ramp after moon
    // 1+ = no, stay at moon
    else if (3 == step) {
        dont_ramp_after_moon = value;
    }
    #endif

    #ifdef USE_2C_STYLE_CONFIG
    // item 4: Anduril 1 2C turbo, or Anduril 2 2C ceiling?
    // 0 = no turbo, only ceiling
    // 1 = Anduril 1, 2C turbo
    // 2+ = Anduril 2, 2C ceiling
    else if (4 == step) {
        ramp_2c_style = value;
    }
    #endif

    #ifdef USE_1H_STYLE_CONFIG
    // item 5: disable reversing with 1H?
    // 0 = enable reversing with 1H
    // 1+ = disable reversing with 1H
    else if (5 == step) {
        ramp_1h_style = value;
    }
    #endif
}

uint8_t ramp_extras_config_state(Event event, uint16_t arg) {
    return config_state_base(event, arg, 5, ramp_extras_config_save);
}
#endif

#ifdef USE_GLOBALS_CONFIG
void globals_config_save(uint8_t step, uint8_t value) {
    if (0) {}
    #ifdef USE_TINT_RAMPING
    else if (step == 1+tint_steps_config_step) {
        if (value && value < 255) {
            // otherwise we could have step size 0
            tint_steps = value;
            tint = nearest_tint_level(tint);
        }
    }
    #endif
    #ifdef USE_JUMP_START
    else if (step == 1+jump_start_config_step) { jump_start_level = value; }
    #endif
}

uint8_t globals_config_state(Event event, uint16_t arg) {
    return config_state_base(event, arg, globals_config_num_steps, globals_config_save);
}
#endif

uint8_t nearest_ramp_level(int16_t target) {
    uint8_t style_ramp = ramp_style;
    uint8_t floor;
    uint8_t ceil;
    uint8_t steps;
    #ifdef USE_SIMPLE_UI
    if (simple_ui_active) {
        floor = ramp_floors[2];
        ceil = ramp_ceils[2];
        steps = ramp_stepss[2];
    }
    else {
    #endif
        floor = ramp_floors[style_ramp];
        ceil = ramp_ceils[style_ramp];
        steps = ramp_stepss[1] * style_ramp;  // 0 for smooth ramping
    #ifdef USE_SIMPLE_UI
    }
    #endif
    return nearest_level(target, floor, ceil, steps);
}

#ifdef USE_THERMAL_REGULATION
void set_level_and_therm_target(uint8_t level) {
    target_level = level;
    set_level(level);
}
#else
#define set_level_and_therm_target(level) set_level(level)
#endif


#endif


/*
 * tint-ramping.h: Tint ramping functions for Anduril.
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

#ifndef TINT_RAMPING_H
#define TINT_RAMPING_H

#ifndef TINT_3H_DIRECTION
#define TINT_3H_DIRECTION -1
#endif
#ifndef TINT_4H_DIRECTION
#define TINT_4H_DIRECTION 1
#endif

// 0: smooth tint ramp
// 1: stepped tint ramp
#ifndef DEFAULT_TINT_STYLE
#define DEFAULT_TINT_STYLE 0
#endif
uint8_t tint_style = DEFAULT_TINT_STYLE;

// number of steps for stepped tint ramping
// (stepped ramping and two tint steps reduces to tint toggling)
#ifndef DEFAULT_TINT_STEPS
#define DEFAULT_TINT_STEPS 2
#endif
uint8_t tint_steps = DEFAULT_TINT_STEPS;

#define PAST_EDGE_TIMEOUT_FACTOR 3
const uint8_t past_edge_timeout = PAST_EDGE_TIMEOUT_FACTOR * HOLD_TIMEOUT;

#ifdef USE_MANUAL_MEMORY
uint8_t manual_memory_tint;
#endif

// not actually a mode, more of a fallback under other modes
uint8_t tint_ramping_state(Event event, uint16_t arg);

// find nearest level for stepped tint ramping
uint8_t nearest_tint_level(int16_t target);

#endif

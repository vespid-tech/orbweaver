/*
 * Orbweaver - leds.h
 * Copyright (C) 2026 Vespid
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef LEDS_H
#define LEDS_H


#define LED_GREEN 0
#define LED_AMBER 1
#define LED_RED   2

void leds_init(void);
void leds_service(void);
void led_set_green(bool value);
void led_set_amber(bool value);
void led_set_red(bool value);
void led_toggle_green(void);
void led_toggle_amber(void);
void led_toggle_red(void);
void led_display_num(uint8_t display_num);

#endif

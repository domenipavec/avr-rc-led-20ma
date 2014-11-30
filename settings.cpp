/* File: settings.cpp
 * Settings.
 */
/* Copyright (c) 2012-2013 Domen Ipavec (domen.ipavec@z-v.si)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <util/delay.h>

#include <avr/io.h>
//#include <avr/pgmspace.h>
#include <avr/eeprom.h> 

#include <stdint.h>

#include "bitop.h"

#include "main.h"
#include "settings.h"

uint8_t EEMEM leds_brightness_mode_ee[7];
uint8_t EEMEM leds_brightness_ee[7];
uint8_t EEMEM leds_mode_ee[7];
uint16_t EEMEM leds_time_ee[7];

uint8_t leds_brightness_mode[7];
uint8_t leds_brightness[7];
uint8_t leds_mode[7];
uint16_t leds_time[7];

uint8_t menu_led() {
    uint8_t led = 0;
    for (;;) {
        // reset time
        timer[0] = 0;
        
        // led on for short time
        set_brightness(led, 0xff);
        while (timer[0] < 16);
        set_brightness(led, 0);
        
        
        while (timer[0] < 160) {
            if (BITCLEAR(PINA, PA7)) {
                while(BITCLEAR(PINA, PA7));
                _delay_ms(10);
                return led;
            }
        }
        
        led++;
        if (led == 7) {
            led = 0;
        }
    }
}

uint8_t menu_N(uint8_t led, uint8_t max) {
    uint8_t i = 0;
    for (;;) {
        for (uint8_t j = 0; j <= i; j++) {
            timer[0] = 0;
            while (timer[0] < 16);
            set_brightness(led, 0xff);
            while (timer[0] < 32);
            set_brightness(led, 0);
        }
        
        while (timer[0] < 180) {
            if (BITCLEAR(PINA, PA7)) {
                while(BITCLEAR(PINA, PA7));
                _delay_ms(10);
                return i;
            }
        }
        
        i++;
        if (i == max) {
            i = 0;
        }
    }
}

uint8_t menu_select_brightness(uint8_t led) {
    uint8_t brightness = 0;
    for (;;) {
        set_brightness(led, brightness);
        timer[0] = 0;
        while (timer[0] < 2) {
            if (BITCLEAR(PINA, PA7)) {
                while(BITCLEAR(PINA, PA7));
                _delay_ms(10);
                return brightness;
            }
        }
        brightness++;
    }
}

void menu_brightness(uint8_t led) {
    uint8_t mode = menu_N(led, 2);
    if (mode == 0) {
        uint8_t brightness = menu_select_brightness(led);

        leds_brightness[led] = brightness;
        leds_brightness_mode[led] = CONSTANT;
        eeprom_write_byte(&leds_brightness_ee[led], brightness);
        eeprom_write_byte(&leds_brightness_mode_ee[led], CONSTANT);
    } else {
        uint8_t channel = menu_N(led, 3);
        
        leds_brightness_mode[led] = channel;
        eeprom_write_byte(&leds_brightness_mode_ee[led], channel);
    }
}

void menu_mode(uint8_t led) {
    uint8_t mode = menu_N(led, 4);
    leds_mode[led] = mode;
    eeprom_write_byte(&leds_mode_ee[led], mode);
}

uint16_t menu_select_time(uint8_t led) {
    uint16_t time;
    while (BITSET(PINA, PA7));
    
    timer[0] = 0;
    set_brightness(led, 0xff);
    while(BITCLEAR(PINA, PA7));
    set_brightness(led, 0);
    time = timer[0];
    
    _delay_ms(10);
    
    return time;
}

void menu_time(uint8_t led) {
    uint16_t time;
    uint8_t mode = menu_N(led, 2);
    if (mode == 0) {
        time = menu_select_time(led);
    } else if (mode == 1) {
        time = leds_time[menu_led()];
    }
    
    leds_time[led] = time;
    eeprom_write_word(&leds_time_ee[led], time);
}

void menu_root() {
    for (;;) {
        uint8_t led = menu_led();
        
        uint8_t root_menu = 0;
        const uint8_t ROOT_MENU_N = 3;
        while (root_menu != ROOT_MENU_N) {
            root_menu = menu_N(led, ROOT_MENU_N + 1);
            switch(root_menu) {
                case 0:
                    menu_brightness(led);
                    break;
                case 1:
                    menu_mode(led);
                    break;
                case 2:
                    menu_time(led);
                    break;
            }
        }
    }
}
/* File: main.cpp
 * Contains base main function and usually all the other stuff that avr does...
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
#include <avr/interrupt.h>
//#include <avr/pgmspace.h>
#include <avr/eeprom.h> 

#include <stdint.h>

#include "bitop.h"
#include "exponential.h"

using namespace avr_cpp_lib;

volatile uint8_t leds[8];

uint8_t EEMEM leds_brightness_mode_ee[7];
static const uint8_t CONSTANT = 10;
uint8_t EEMEM leds_brightness_ee[7];
uint8_t EEMEM leds_mode_ee[7];
uint16_t EEMEM leds_time_ee[7];

uint8_t leds_brightness_mode[7];
uint8_t leds_brightness[7];
uint8_t leds_mode[7];
uint16_t leds_time[7];

uint16_t EEMEM signal_min_ee[3];
uint16_t EEMEM signal_max_ee[3];

volatile uint16_t signal_min[3];
volatile uint16_t signal_max[3];
volatile uint8_t signal_divider[3];

volatile uint16_t signal_start[3];
volatile uint8_t signal_value[3];

volatile uint16_t timer[7];

void set_brightness(uint8_t led, uint8_t b) {
    b = exponential(b);
    for (uint8_t i = 0; i < 8; i++) {
        if (BITSET(b, i)) {
            SETBIT(leds[i], led);
        } else {
            CLEARBIT(leds[i], led);
        }
    }
}

void update_divider(uint8_t i) {
    signal_divider[i] = (signal_max[i] - signal_min[i])>>8;
    if (signal_divider[i] < 1) {
        signal_divider[i] = 1;
    }
}

ISR(TIM0_COMPA_vect) {
    static uint8_t mode = 0;
    switch (mode) {
        case 0:
            PORTA = leds[1];
            OCR0A = 3;
            mode++;
            break;
        case 1:
            PORTA = leds[2];
            OCR0A = 7;
            mode++;
            break;
        case 2:
            PORTA = leds[3];
            OCR0A = 15;
            mode++;
            break;
        case 3:
            PORTA = leds[4];
            OCR0A = 31;
            mode++;
            break;
        case 4:
            PORTA = leds[5];
            OCR0A = 63;
            mode++;
            break;
        case 5:
            PORTA = leds[6];
            OCR0A = 127;
            mode++;
            break;
        case 6:
            PORTA = leds[7];
            OCR0A = 1;
            mode = 0;
            break;
    }
}

ISR(TIM0_OVF_vect) {
    PORTA = leds[0];
}

ISR(TIM1_COMPA_vect) {
    for (uint8_t i = 0; i < 7; i++) {
        timer[i]++;
    }
}

ISR(PCINT1_vect) {
    static uint8_t last = 0;
    uint8_t curr = PINB & 0b0111;
    uint8_t diff = last ^ (curr);
    
    for (uint8_t i = 0; i < 3; i++) {
        if (BITSET(diff, i)) {
            if (BITSET(curr, i)) {
                signal_start[i] = TCNT1;
            } else {
                uint16_t signal = TCNT1 - signal_start[i];
                if (signal < 100) {
                    continue;
                }
                if (signal < signal_min[i]) {
                    signal_min[i] = signal;
                    eeprom_write_word(&signal_min_ee[i], signal);
                    update_divider(i);
                } else if (signal > signal_max[i]) {
                    signal_max[i] = signal;
                    eeprom_write_word(&signal_max_ee[i], signal);
                    update_divider(i);
                }
                signal_value[i] = (signal - signal_min[i])/signal_divider[i];
            }
        }
    }
    
    last = curr;
}

uint8_t menu_led() {
    uint8_t led = 0;
    for (;;) {
        // reset time
        timer[0] = 0;
        
        // led on for short time
        set_brightness(led, 0xff);
        while (timer[0] < 16);
        set_brightness(led, 0);
        
        
        while (timer[0] < 230) {
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
        
        while (timer[0] < 250) {
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

        eeprom_write_byte(&leds_brightness_ee[led], brightness);
        eeprom_write_byte(&leds_brightness_mode_ee[led], CONSTANT);
    } else {
        uint8_t channel = menu_N(led, 3);
        
        eeprom_write_byte(&leds_brightness_mode_ee[led], channel);
    }
}

void menu_mode(uint8_t led) {
    uint8_t mode = menu_N(led, 3);
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
    
    eeprom_write_word(&leds_time_ee[led], time);
}

void update_brightness(uint8_t led) {
    if (leds_brightness_mode[led] == CONSTANT) {
        set_brightness(led, leds_brightness[led]);
    } else {
        set_brightness(led, signal_value[leds_brightness_mode[led]]);
    }
}

void update_led(uint8_t led) {
    switch (leds_mode[led]) {
        case 0:
            update_brightness(led);
            break;
        case 1:
            if (timer[led] < leds_time[led]) {
                set_brightness(led, 0);
            } else if (timer[led] < 2*leds_time[led]) {
                update_brightness(led);
            } else {
                timer[led] = 0;
            }
            break;
        case 2:
            if (timer[led] < leds_time[led]) {
                update_brightness(led);
            } else if (timer[led] < 2*leds_time[led]) {
                set_brightness(led, 0);
            } else {
                timer[led] = 0;
            }
            break;
    }
}

int main() {
    // init button
    SETBIT(PORTA, PA7);
    
    // read eeprom
    eeprom_read_block((void*)&leds_brightness_mode, (const void *)&leds_brightness_mode_ee, 7);
    eeprom_read_block((void*)&leds_brightness, (const void *)&leds_brightness_ee, 7);
    eeprom_read_block((void*)&leds_mode, (const void *)&leds_mode_ee, 7);
    eeprom_read_block((void*)&leds_time, (const void *)&leds_time_ee, 14);
    
    eeprom_read_block((void*)&signal_min, (const void *)&signal_min_ee, 6);
    eeprom_read_block((void*)&signal_max, (const void *)&signal_max_ee, 6);
    for (uint8_t i = 0; i < 3; i++) {
        update_divider(i);
    }
    
    // init leds
    DDRA = 0b01111111;
    
    // init led pwm
    TIMSK0 = 0b00000011;
    OCR0A = 1;
    TCCR0B = 0b00000100;
    for (uint8_t i = 0; i < 8; i++) {
        leds[i] = 0b10000000;
    }
    
    // timer clear on compare, 100ms/16
    TIMSK1 = 0b00000010;
    OCR1A = 49999;
    TCCR1B = 0b00001001;
    
    // enable interrupts
    sei();
    
    // enter settings ?
    if (BITCLEAR(PINA, PA7)) {
        while(BITCLEAR(PINA, PA7));
        
        for (uint8_t i = 0; i < 3; i++) {
            signal_min[i] = 2000;
            signal_max[i] = 2256;
            signal_divider[i] = 1;
        }
        eeprom_write_block((void*)signal_min_ee, (void*)signal_min, 6);
        eeprom_write_block((void*)signal_max_ee, (void*)signal_max, 6);
        
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
    
    // init pc interrupts 8,9,10
    GIMSK = 0b00100000;
    PCMSK1 = 0b00000111;

    // reset all timers
    for (uint8_t i = 0; i < 7; i++) {
        timer[i] = 0;
    }
    
	for (;;) {
        for (uint8_t i = 0; i < 7; i++) {
            update_led(i);
        }
    }
}

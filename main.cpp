/* File: main.cpp
 * Contains base main function and usually all the other stuff that avr does...
 */
/* Copyright (c) 2012-2014 Domen Ipavec (domen.ipavec@z-v.si)
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

//#include <util/delay.h>

#include <avr/io.h>
#include <avr/interrupt.h>
//#include <avr/pgmspace.h>
#include <avr/eeprom.h> 

#include <stdint.h>

#include "bitop.h"
#include "exponential.h"

#include "main.h"
#include "settings.h"

using namespace avr_cpp_lib;

volatile uint8_t leds[8];

uint16_t EEMEM signal_min_ee[3];
uint16_t EEMEM signal_max_ee[3];

volatile uint16_t signal_min[3];
volatile uint16_t signal_max[3];
volatile uint16_t signal[3];
volatile bool signal_update[3] = {false, false, false};
volatile uint8_t signal_divider[3];

volatile uint16_t signal_start[3];
volatile uint8_t signal_value[3];

bool signal_calibrated[3] = {false,false,false};

volatile uint16_t timer[7];

static const uint8_t SIGNAL_STEP = 16;

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
    signal_divider[i] = (signal_max[i] - signal_min[i] - 2*SIGNAL_STEP)>>8;
    if (signal_divider[i] < 1) {
        signal_divider[i] = 1;
    }
}

ISR(TIM0_COMPA_vect) {
    static uint8_t mode = 1;
    PORTA = leds[mode];
    mode++;
    switch (mode-2) {
        case 0:
            OCR0A = 3;
            break;
        case 1:
            OCR0A = 7;
            break;
        case 2:
            OCR0A = 15;
            break;
        case 3:
            OCR0A = 31;
            break;
        case 4:
            OCR0A = 63;
            break;
        case 5:
            OCR0A = 127;
            break;
        case 6:
            OCR0A = 1;
            mode = 1;
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
    uint16_t t = TCNT1;
    
    for (uint8_t i = 0; i < 3; i++) {
        if (BITSET(diff, i)) {
            if (BITSET(curr, i)) {
                signal_start[i] = t;
            } else {
                signal[i] = t - signal_start[i];
                if (t < signal_start[i]) {
                    signal[i] -= 15536;
                }
                signal_update[i] = true;
            }
        }
    }

    last = curr;
}

void update_brightness(uint8_t led) {
    if (leds_brightness_mode[led] == CONSTANT) {
        set_brightness(led, leds_brightness[led]);
    } else {
        set_brightness(led, signal_value[leds_brightness_mode[led]]);
    }
}

uint16_t get_time(uint8_t led) {
    if (leds_time_mode[led] == CONSTANT) {
        return leds_time[led];
    } else {
        return 16+signal_value[leds_time_mode[led]]*4;
    }
}

void update_led(uint8_t led) {
    uint16_t lt = get_time(led);
    switch (leds_mode[led]) {
        case 0:
            update_brightness(led);
            break;
        case 1:
            if (timer[led] < lt) {
                set_brightness(led, 0);
            } else if (timer[led] < 2*lt) {
                update_brightness(led);
            } else {
                timer[led] -= 2*lt;
            }
            break;
        case 2:
            if (timer[led] < lt) {
                update_brightness(led);
            } else if (timer[led] < 2*lt) {
                set_brightness(led, 0);
            } else {
                timer[led] -= 2*lt;
            }
            break;
        case 3:
            if (timer[led] < 16) {
                update_brightness(led);
            } else if (timer[led] < lt) {
                set_brightness(led, 0);
            } else {
                timer[led] -= lt;
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
    eeprom_read_block((void*)&leds_time_mode, (const void *)&leds_time_mode_ee, 7);
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
        
        menu_root();
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
        for (uint8_t i = 0; i < 3; i++) {
            if (signal_update[i]) {
//                 if (signal[i] < 1000) {
//                     continue;
//                 }
                if (BITCLEAR(PINA, PA7)) {
                    if (!signal_calibrated[i]) {
                        signal_min[i] = signal[i];
                        signal_max[i] = signal[i];
                        signal_divider[i] = 1;
                        eeprom_write_block((void*)signal_min, (void*)signal_min_ee, 6);
                        eeprom_write_block((void*)signal_max, (void*)signal_max_ee, 6);
                        signal_calibrated[i] = true;
                    }
                    if (signal[i] < signal_min[i]) {
                        while (signal[i] < signal_min[i]) {
                            signal_min[i] -= SIGNAL_STEP;
                        }
                        eeprom_write_word(&signal_min_ee[i], signal_min[i]);
                        update_divider(i);
                    }
                    if (signal[i] > signal_max[i]) {
                        while (signal[i] > signal_max[i]) {
                            signal_max[i] += SIGNAL_STEP;
                        }
                        eeprom_write_word(&signal_max_ee[i], signal_max[i]);
                        update_divider(i);
                    }
                }
                uint16_t value = (signal[i] - signal_min[i] + SIGNAL_STEP)/signal_divider[i];
                if (signal[i] < signal_min[i]) {
                    value = 0;
                }
                if (value > 255) {
                    value = 255;
                }
                signal_value[i] = value;
                signal_update[i] = false;
            }
        }
    }
}

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>

#define F_CPU 1000000UL
#include <util/delay.h>

#define BIT(pos) (1 << (pos))
#define GET_BIT(x, pos) ((x >> pos) & 1)
#define SET_BIT(x, pos) ((x) |= (BIT(pos)))
#define CLEAR_BIT(x, pos) ((x) &= ~(BIT(pos)))

const uint8_t cplex_pins[16][2] = {
    // Group 0
    {PB1, PB2},
    {PB1, PB3},
    {PB1, PB4},
    {PB1, PB5},

    // Group 1
    {PB2, PB1},
    {PB2, PB3},
    {PB2, PB4},
    {PB2, PB5},

    // Group 2
    {PB3, PB1},
    {PB3, PB2},
    {PB3, PB4},
    {PB3, PB5},

    // Group 3
    {PB4, PB1},
    {PB4, PB2},
    {PB4, PB3},
    {PB4, PB5},
};

int intr_count = 0;
int active_led = 0;
int speed = 1;
int sec = 0;

void sleep() {
    GIMSK |= _BV(PCIE);                   // Enable Pin Change Interrupts
    PCMSK |= _BV(PCINT3);                 // Use PB3 as interrupt pin
    ADCSRA &= ~_BV(ADEN);                 // ADC off
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);  // replaces above statement

    sleep_enable();  // Sets the Sleep Enable bit in the MCUCR Register (SE BIT)
    sei();           // Enable interrupts
    sleep_cpu();     // sleep

    cli();                  // Disable interrupts
    PCMSK &= ~_BV(PCINT3);  // Turn off PB3 as interrupt pin
    sleep_disable();        // Clear SE bit
    ADCSRA |= _BV(ADEN);    // ADC on

    sei();  // Enable interrupts
}

void set_led_index(int i) {
    uint8_t val = 0;
    uint8_t dir = 0;

    SET_BIT(val, cplex_pins[i][0]);
    SET_BIT(dir, cplex_pins[i][0]);
    SET_BIT(dir, cplex_pins[i][1]);

    PORTB = val;
    DDRB = dir;
}

ISR(PCINT0_vect) {
    // This is called when the interrupt occurs, but I don't need to do anything in it
}

// Interrupt vector for Timer0
ISR(TIMER0_OVF_vect) {
    if (intr_count == speed)  // waiting for 63 because to get 1 sec delay
    {
        set_led_index(active_led);
        active_led++;
        if (active_led == 16) active_led = 0;

        intr_count = 0;  // making intr_count=0 to repeat the count
        ++sec;
    } else {
        intr_count++;  // incrementing c upto 63
    }
}

void timer_setup() {
    TCCR0A = 0x00;  // Normal mode
    TCCR0B = 0x00;
    TCCR0B |= (1 << CS00) | (1 << CS02);  // prescaling with 1024
    sei();                                // enabling global interrupt
    TCNT0 = 0;
    TIMSK |= (1 << TOIE0);  // enabling timer0 interrupt
}

int main() {
    timer_setup();

    while (1);
}

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <stdlib.h>

#define BIT(pos) (1 << (pos))
#define GET_BIT(x, pos) ((x >> pos) & 1)
#define SET_BIT(x, pos) ((x) |= (BIT(pos)))
#define CLEAR_BIT(x, pos) ((x) &= ~(BIT(pos)))

#define PIN_BTN PB0
#define LED_CNT 16

#define F_CLK 1000000UL
#define TMR_OVF_CNT 256UL
#define MS_TO_OVF(x) ((x * F_CLK) / (TMR_OVF_CNT * 1000UL))

#define CURSOR_TIME_BASE 2000
#define CURSOR_TIME_STAGE_DELTA 16
#define SPIN_TIME 100
#define HIT_TIME_BASE 8000
#define HIT_TIME_STAGE_DELTA 64
#define GAME_TIMEOUT 40000

const uint8_t cplex_pins[LED_CNT][2] = {
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

typedef enum {
    GAME_INIT,
    GAME_ACTIVE,
    GAME_HIT,
} game_state_t;

game_state_t game_state;  // Current game state
int game_stage;           // Current stage
int game_target;          // Current target
int game_cursor;          // Current position

int idle_time = 0;
int timer_ovf_cnt = 0;

#define BTN_DEBOUNCE_NUM 16
int btn_pressed_cnt;

void game_target_rand() {
    game_target = rand() % LED_CNT;  // Pick a new target
}

void game_stage_next() {
    game_state = GAME_ACTIVE;
    game_stage++;        // Increase the stage
    game_target_rand();  // Pick a random target
}

void game_init() {
    // Configure timer
    TCCR0A = 0x00;  // Normal mode
    TCCR0B = 0x00;
    TCCR0B |= (1 << CS00);  // No prescaling
    TCNT0 = 0;

    // Configure button pin as input with pull-up resistor
    DDRB &= ~(1 << PIN_BTN);
    SET_BIT(PORTB, PIN_BTN);

    // Setup game
    game_state = GAME_INIT;
    game_cursor = 15;
    idle_time = 0;
    timer_ovf_cnt = 0;
}

void game_start() {
    game_state = GAME_ACTIVE;
    game_stage = 0;      // Start from stage 0
    game_target_rand();  // Pick a random target
}

void led_off() {
    PORTB &= (1 << PIN_BTN);
    DDRB &= (1 << PIN_BTN);
}

void led_set_index(int i) {
    // FIXME:
    if (i % 4 == 3) return;

    uint8_t val = PORTB & (1 << PIN_BTN);
    uint8_t dir = DDRB & (1 << PIN_BTN);

    SET_BIT(val, cplex_pins[i][1]);
    SET_BIT(dir, cplex_pins[i][1]);
    SET_BIT(dir, cplex_pins[i][0]);

    PORTB = val;
    DDRB = dir;
}

void sleep() {
    GIMSK |= _BV(PCIE);    // Enable Pin Change Interrupts
    PCMSK |= _BV(PCINT0);  // Use PB0 as interrupt pin

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);  // replaces above statement

    // Set button
    DDRB = 0;  // All inputs
    PORTB = (1 << PIN_BTN);

    sleep_enable();  // Sets the Sleep Enable bit in the MCUCR Register (SE BIT)
    sei();           // Enable interrupts
    sleep_cpu();     // sleep

    cli();            // Disable interrupts
    sleep_disable();  // Clear SE bit
}

void game_do_init() {
    if (timer_ovf_cnt > SPIN_TIME) {
        // Move lights counter-clockwise
        game_cursor--;
        if (game_cursor < 0) {
            game_start();
        }

        // Reset IRQ counter
        timer_ovf_cnt = 0;
    }

    // Program the LED
    led_set_index(game_cursor);
}

void game_do_active() {
    idle_time++;

    if (timer_ovf_cnt > (CURSOR_TIME_BASE - game_stage * CURSOR_TIME_STAGE_DELTA)) {
        // Move lights clockwise
        game_cursor++;
        if (game_cursor >= LED_CNT) {
            game_cursor = 0;
        }

        // Reset IRQ counter
        timer_ovf_cnt = 0;
    }

    // Read button
    if (!GET_BIT(PINB, PIN_BTN)) {
        if (btn_pressed_cnt > BTN_DEBOUNCE_NUM) {
            if (game_cursor == game_target) {
                game_state = GAME_HIT;
            } else {
                game_init();
            }
            idle_time = 0;
            btn_pressed_cnt = 0;
        } else {
            btn_pressed_cnt++;
        }
    } else {
        btn_pressed_cnt = 0;
    }

    // Tooggle between target and cursor
    if (timer_ovf_cnt & 1) {
        led_set_index(game_target);
    } else {
        led_set_index(game_cursor);
    }

    // Check for idle time and put device to sleep if necessary
    if (idle_time > GAME_TIMEOUT) {
        sleep();
        game_init();
    }
}

void game_do_hit() {
    if (timer_ovf_cnt > (HIT_TIME_BASE - game_stage * HIT_TIME_STAGE_DELTA)) {
        game_stage_next();

        // Reset IRQ counter
        timer_ovf_cnt = 0;
    }

    // Program the LED
    if (GET_BIT(timer_ovf_cnt, 7)) {
        led_set_index(game_target);
    } else {
        led_off();
    }
}

int main() {
    game_init();

    while (1) {
        // Time game execution with timer overflow
        if (GET_BIT(TIFR, TOV0)) {
            SET_BIT(TIFR, TOV0);  // Clear timer overflow (W1TC)

            // Game logic
            switch (game_state) {
                case GAME_INIT:
                    game_do_init();
                    break;

                case GAME_ACTIVE:
                    game_do_active();
                    break;

                case GAME_HIT:
                    game_do_hit();
                    break;
            }

            timer_ovf_cnt++;
        }
    }
}

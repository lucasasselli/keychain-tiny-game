#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <stdlib.h>

#define BIT(pos) (1 << (pos))
#define GET_BIT(x, pos) ((x >> pos) & 1)
#define SET_BIT(x, pos) ((x) |= (BIT(pos)))
#define CLEAR_BIT(x, pos) ((x) &= ~(BIT(pos)))

#define LED_CNT 16

#define BTN_DEBOUNCE_NUM 10
#define BTN_PIN PB0

#define CURSOR_TIME_BASE 800
#define CURSOR_TIME_STAGE_DELTA 16
#define INIT_SPIN_TIME 100
#define INIT_SPIN_NUM 3
#define FAIL_TIME 8000
#define GAME_TIMEOUT 40000
#define GAME_MAX_STAGES 45
#define WIN_TIME 8000

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
    GAME_FAIL,
    GAME_WIN
} game_state_t;

game_state_t game_state;  // Current game state
int game_init_cnt;        // Number of loops furing init animation
int game_stage;           // Stage
int game_target;          // Target light position
int game_cursor;          // Cursor light position
int game_dir;             // Cursor direction

int idle_time = 0;
int timer_ovf_cnt = 0;

int btn_pressed_cnt;
int btn_state;
int btn_old_state;

void game_target_rand() {
    game_target = rand() % LED_CNT;  // Pick a new target
}

void game_init() {
    // Configure timer
    TCCR0A = 0x00;  // Normal mode
    TCCR0B = 0x00;
    TCCR0B |= (1 << CS00);  // No prescaling
    TCNT0 = 0;

    // Configure button pin as input with pull-up resistor
    DDRB &= ~(1 << BTN_PIN);
    SET_BIT(PORTB, BTN_PIN);

    // Setup game
    game_state = GAME_INIT;
    game_init_cnt = 0;
    game_cursor = 0;
    game_dir = 0;
    idle_time = 0;
    timer_ovf_cnt = 0;

    game_target_rand();  // Pick a random target
}

void game_start() {
    game_state = GAME_ACTIVE;
    game_stage = 0;  // Start from stage 0
}

void game_stage_next() {
    game_dir ^= 1;       // Change direction
    game_target_rand();  // Pick a random target

    game_stage++;  // Increase the stage
    if (game_stage > GAME_MAX_STAGES) {
        game_state = GAME_WIN;
    } else {
        game_state = GAME_ACTIVE;
    }
}

void led_off() {
    PORTB &= (1 << BTN_PIN);
    DDRB &= (1 << BTN_PIN);
}

void led_set_index(int i) {
    uint8_t val = PORTB & (1 << BTN_PIN);
    uint8_t dir = DDRB & (1 << BTN_PIN);

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
    PORTB = (1 << BTN_PIN);

    sleep_enable();  // Sets the Sleep Enable bit in the MCUCR Register (SE BIT)
    sei();           // Enable interrupts
    sleep_cpu();     // sleep

    cli();            // Disable interrupts
    sleep_disable();  // Clear SE bit
}

void cursor_next() {
    game_cursor++;
    if (game_cursor >= LED_CNT) {
        game_cursor = 0;
    }
}

void cursor_prev() {
    game_cursor--;
    if (game_cursor < 0) {
        game_cursor = LED_CNT - 1;
    }
}

void game_do_init() {
    if (timer_ovf_cnt > INIT_SPIN_TIME) {
        cursor_next();

        if (game_cursor == game_target) {
            game_init_cnt++;
            if (game_init_cnt > INIT_SPIN_NUM) {
                game_start();
            }
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
        if (game_dir) {
            cursor_next();
        } else {
            cursor_prev();
        }

        // Reset IRQ counter
        timer_ovf_cnt = 0;
    }

    // De-bounce button
    if (!GET_BIT(PINB, BTN_PIN)) {
        if (btn_pressed_cnt < BTN_DEBOUNCE_NUM) {
            btn_pressed_cnt++;
        } else {
            btn_state = 1;
        }
    } else {
        if (btn_pressed_cnt > 0) {
            btn_pressed_cnt--;
        } else {
            btn_state = 0;
        }
    }

    if (btn_state == 1 && btn_old_state == 0) {
        // Positive edge
        if (game_cursor == game_target) {
            game_stage_next();
        } else {
            game_state = GAME_FAIL;
        }
        idle_time = 0;
    }

    btn_old_state = btn_state;

    // Tooggle between target and cursor
    if ((timer_ovf_cnt & 15) == 1) {
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

void game_do_fail() {
    if (timer_ovf_cnt > FAIL_TIME) {
        game_init();

        // Reset IRQ counter
        timer_ovf_cnt = 0;
    }

    // Program the LED
    if (GET_BIT(timer_ovf_cnt, 7)) {
        led_set_index(game_cursor);
    } else {
        led_off();
    }
}

void game_do_win() {
    if (timer_ovf_cnt > WIN_TIME) {
        game_init();

        // Reset IRQ counter
        timer_ovf_cnt = 0;
    }

    // Program the LED
    if (GET_BIT(timer_ovf_cnt, 7)) {
        cursor_next();
        led_set_index(game_cursor);
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

                case GAME_FAIL:
                    game_do_fail();
                    break;

                case GAME_WIN:
                    game_do_win();
                    break;
            }

            timer_ovf_cnt++;
        }
    }
}

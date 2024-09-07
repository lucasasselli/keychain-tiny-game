#include <assert.h>
#include <curses.h>
#include <fmt/format.h>
#include <inttypes.h>
#include <locale.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <boost/program_options.hpp>
#include <cwchar>
#include <iostream>
#include <string>
#include <vector>

#include "avr_ioport.h"
#ifdef __cplusplus
extern "C" {
#include "button.h"
}
#endif

#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_gdb.h"
#include "sim_io.h"
#include "sim_vcd_file.h"

using namespace boost;
using namespace std;

#define GET_BIT(x, pos) ((x >> (pos)) & 1)

button_t button;
int do_button_press = 0;
avr_t *avr = NULL;
avr_vcd_t vcd_file;

uint8_t portb_state = 0;
uint8_t ddrb_state = 0;
bool led_state[16];
bool tui;

// GUI
const int WIN_OFFSET = 2;
const int WIN_BOARD_Y = 15;
const int WIN_LOG_X = 80;
const int WIN_LOG_Y = 30;
const int LOGS_LINES_MAX = WIN_LOG_Y - 2;

WINDOW *board_win;
WINDOW *logs_win;

vector<string> logs;

bool do_screen_update = false;

void ncurses_init() {
    initscr();
    nodelay(stdscr, true);
    noecho();
    curs_set(0);
    clear();
}

void ncurses_stop() {
    endwin();
}

void win_draw_leds(WINDOW *win) {
    const int X_OFF = 15;
    mvwprintw(win, 0, 1, "Board");  // Title

    auto x = [](int val) { return led_state[val] ? 'o' : '.'; };

    int line = 4;
    mvwprintw(win, line++, X_OFF, "       %c %c %c       ", x(15), x(0), x(1));
    mvwprintw(win, line++, X_OFF, "     %c       %c     ", x(14), x(2));
    mvwprintw(win, line++, X_OFF, "   %c           %c   ", x(13), x(3));
    mvwprintw(win, line++, X_OFF, "   %c           %c   ", x(12), x(4));
    mvwprintw(win, line++, X_OFF, "   %c           %c   ", x(11), x(5));
    mvwprintw(win, line++, X_OFF, "     %c       %c     ", x(10), x(6));
    mvwprintw(win, line++, X_OFF, "       %c %c %c       ", x(9), x(8), x(7));
}

void win_draw_logs(WINDOW *win) {
    mvwprintw(win, 0, 1, "SimAVR");  // Title

    int log_start = 0;
    if (logs.size() > LOGS_LINES_MAX) {
        log_start = logs.size() - LOGS_LINES_MAX;
    }

    int line = 1;
    for (int i = log_start; i < logs.size(); i++) {
        mvwprintw(win, line++, 1, "%s", logs[i].c_str());
    }
}

void log(const char *format, ...) {
    va_list args;
    va_start(args, format);

    // Calculate the size of the formatted string
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);

    // Create a string with the required size
    std::vector<char> buffer(size + 1);
    vsnprintf(buffer.data(), buffer.size(), format, args);

    va_end(args);

    if (tui) {
        logs.push_back(std::string(buffer.data(), size));
        do_screen_update = true;
    } else {
        cout << buffer.data() << std::endl;
    }
}

void ncurses_update() {
    // Board window
    box(board_win, 0, 0);

    // move and print in window
    win_draw_leds(board_win);

    // refreshing the window
    wrefresh(board_win);

    // Logs window
    box(logs_win, 0, 0);

    // move and print in window
    mvwprintw(logs_win, 0, 1, "SimAVR");
    win_draw_logs(logs_win);

    // refreshing the window
    wrefresh(logs_win);
}

bool main_loop() {
    avr_run(avr);

    uint32_t key = getch();
    switch ((char)key) {
        case 'q':
        case 0x1f:  // escape
            return false;
        case ' ':
            do_button_press++;  // pass the message to the AVR thread
            break;
        case 'r':
            log("Starting VCD trace\n");
            avr_vcd_start(&vcd_file);
            break;
        case 's':
            log("Stopping VCD trace\n");
            avr_vcd_stop(&vcd_file);
            break;
    }

    if (do_screen_update && tui) {
        ncurses_update();
        do_screen_update = false;
    }

    return true;
}

void update_led_state(void) {
    int a = 0;
    for (int i = 0; i < 16; i++) {
        int c = (i / 4);
        int x = (i % 4);

        if (x == 0) {
            a = 0;
        }

        if (a == c) a++;

        led_state[i] = GET_BIT(portb_state, c + 1) && !GET_BIT(portb_state, a + 1) && GET_BIT(ddrb_state, c + 1) && GET_BIT(ddrb_state, a + 1);

        if (led_state[i]) {
            log("LED %d on!", i);
        }

        a++;
    }

    do_screen_update = true;
}

void portb_callback(struct avr_irq_t *irq, uint32_t value, void *param) {
    portb_state = (portb_state & ~(1 << irq->irq)) | (value << irq->irq);
    update_led_state();
}

void ddrb_callback(struct avr_irq_t *irq, uint32_t value, void *param) {
    portb_state = (ddrb_state & ~(1 << irq->irq)) | (value << irq->irq);
    update_led_state();
}

static void *avr_run_thread(void *param) {
    int b_press = do_button_press;
    while (1) {
        avr_run(avr);

        // Handle button
        if (do_button_press != b_press) {
            b_press = do_button_press;
            log("Button pressed");
            button_press(&button, 1000000);
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    // Command line
    string path;

    program_options::options_description desc("Allowed options");
    /* clang-format off */
    desc.add_options()("help,h", "produce help message")
        ("path", program_options::value<std::string>(&path)->required(), "path to the file")
        ("tui", program_options::bool_switch(&tui), "enable TUI mode");
    /* clang-format on */

    program_options::positional_options_description p;
    p.add("path", 1);

    program_options::variables_map vm;
    try {
        program_options::store(program_options::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 0;
        }

        program_options::notify(vm);
    } catch (const program_options::error &e) {
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << desc << "\n";
        return 1;
    }

    // Setup SimAVR
    elf_firmware_t f;
    elf_read_firmware(path.c_str(), &f);
    f.frequency = 1000000;

    avr = avr_make_mcu_by_name("attiny85");
    if (!avr) {
        fprintf(stderr, "%s: AVR '%s' not known\n", argv[0], f.mmcu);
        exit(1);
    }
    avr_init(avr);
    avr_load_firmware(avr, &f);

    // initialize our 'peripheral'
    button_init(avr, &button, "button");

    // "connect" the output irq of the button to the port pin of the AVR
    avr_connect_irq(button.irq + IRQ_BUTTON_OUT, avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 0));

    for (int i = 0; i < 8; i++) {
        avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), i), portb_callback, NULL);
    }
    // avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), IOPORT_IRQ_DIRECTION_ALL), ddrb_callback, NULL);

    // even if not setup at startup, activate gdb if crashing
    avr->gdb_port = 1234;
    if (0) {
        // avr->state = cpu_Stopped;
        avr_gdb_init(avr);
    }

    /*
     *	VCD file initialization
     *
     *	This will allow you to create a "wave" file and display it in gtkwave
     *	Pressing "r" and "s" during the demo will start and stop recording
     *	the pin changes
     */
    avr_vcd_init(avr, "gtkwave_output.vcd", &vcd_file, 100000 /* usec */);
    avr_vcd_add_signal(&vcd_file, avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), IOPORT_IRQ_PIN_ALL), 8 /* bits */, "portb");
    avr_vcd_add_signal(&vcd_file, button.irq + IRQ_BUTTON_OUT, 1 /* bits */, "button");

    // 'raise' it, it's a "pullup"
    avr_raise_irq(button.irq + IRQ_BUTTON_OUT, 1);

    // the AVR run on it's own thread. it even allows for debugging!
    // pthread_t run;
    // pthread_create(&run, NULL, avr_run_thread, NULL);

    // GUI
    if (tui) {
        ncurses_init();
        board_win = newwin(WIN_BOARD_Y, 50, WIN_OFFSET, 10);
        logs_win = newwin(WIN_LOG_Y, WIN_LOG_X, WIN_BOARD_Y + WIN_OFFSET, 10);
    }

    while (main_loop());

    if (tui) {
        ncurses_stop();
    }

    return EXIT_SUCCESS;
}

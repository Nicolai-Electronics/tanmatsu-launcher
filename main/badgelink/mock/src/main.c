
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#include <fcntl.h>
#include <inttypes.h>
#include <sched.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include "badgelink.h"

static void help(char const* name) {
    printf("Usage: %s <port> OR %s <input> <output>\n", name, name);
    printf("Badgelink test setup\n");
}

void (*mock__thread_func)(void*);
FILE* mock__infd;
FILE* mock__outfd;

void usb_send_data(uint8_t const* data, size_t len) {
    fwrite(data, 1, len, mock__outfd);
}

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) {
        help(*argv);
    }

    if (argc == 2) {
        mock__infd  = fopen(argv[1], "r+b");
        mock__outfd = mock__infd;
        if (!mock__infd) {
            perror("Cannot open serial port");
            return 1;
        }
    } else {
        mock__infd = fopen(argv[1], "rb");
        if (!mock__infd) {
            perror("Cannot open input file");
            return 1;
        }
        mock__outfd = fopen(argv[2], "wb");
        if (!mock__outfd) {
            perror("Cannot open output file");
            return 1;
        }
    }

    struct termios attr;
    tcgetattr(mock__infd->_fileno, &attr);
    cfmakeraw(&attr);
    tcsetattr(mock__infd->_fileno, TCSANOW, &attr);
    fcntl(mock__infd->_fileno, F_SETFL, O_NONBLOCK);

    badgelink_init();
    badgelink_start();

    mock__thread_func(NULL);

    return 0;
}

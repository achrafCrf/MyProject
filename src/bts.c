
// src/bts.c
#include "bts.h"
#include <stdio.h>

int bts_init(struct gpiod_chip *chip, bts_t *bts) {
    if (!chip || !bts) return -1;

    bts->led_red = gpiod_chip_get_line(chip, LED_RED_LINE);
    if (!bts->led_red) { perror("LED red"); return -1; }
    if (gpiod_line_request_output(bts->led_red, "led_red", 0) < 0) {
        perror("request_output red"); return -1;
    }

    bts->led_green = gpiod_chip_get_line(chip, LED_GREEN_LINE);
    if (!bts->led_green) { perror("LED green"); return -1; }
    if (gpiod_line_request_output(bts->led_green, "led_green", 0) < 0) {
        perror("request_output green"); return -1;
    }

    return 0;
}

void bts_set_state(bts_t *bts, int state) {
    if (!bts || !bts->led_red || !bts->led_green) return;
    if (state == 1) {
        gpiod_line_set_value(bts->led_red, 1);
        gpiod_line_set_value(bts->led_green, 0);
    } else {
        gpiod_line_set_value(bts->led_red, 0);
        gpiod_line_set_value(bts->led_green, 1);
    }
}

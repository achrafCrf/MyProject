
// src/bel.c
#include "bel.h"
#include <unistd.h>
#include <stdio.h>

int bel_init(struct gpiod_chip *chip, bel_t *bel, int line, int active_high) {
    if (!chip || !bel) return -1;
    bel->in = gpiod_chip_get_line(chip, line);
    if (!bel->in) { perror("BEL get_line"); return -1; }
    if (gpiod_line_request_input(bel->in, "bel_in") < 0) {
        perror("BEL request_input"); return -1;
    }
    bel->active_high = active_high ? 1 : 0;
    return 0;
}

int bel_read(bel_t *bel) {
    if (!bel || !bel->in) return -1;
    int v = gpiod_line_get_value(bel->in);
    if (v < 0) return -1;
    return bel->active_high ? v : !v;
}

int bel_read_debounced(bel_t *bel, int samples, int us_between_samples) {
    if (!bel || samples <= 0) return -1;
    int count1 = 0, count0 = 0;
    for (int i = 0; i < samples; ++i) {
        int v = bel_read(bel);
        if (v < 0) return -1;
        if (v) count1++; else count0++;
        if (us_between_samples > 0) usleep(us_between_samples);
    }
    return (count1 >= count0) ? 1 : 0;
}


// src/bea.c
#include "bea.h"
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <math.h>

static inline void ts_now(struct timespec *ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

int bea_init(struct gpiod_chip *chip, bea_t *bea) {
    if (!chip || !bea) return -1;

    bea->trig = gpiod_chip_get_line(chip, BEA_TRIG_LINE);
    if (!bea->trig) { perror("BEA get TRIG"); return -1; }
    if (gpiod_line_request_output(bea->trig, "bea_trig", 0) < 0) {
        perror("BEA request_output TRIG"); return -1;
    }

    bea->echo = gpiod_chip_get_line(chip, BEA_ECHO_LINE);
    if (!bea->echo) { perror("BEA get ECHO"); return -1; }
    if (gpiod_line_request_input(bea->echo, "bea_echo") < 0) {
        perror("BEA request_input ECHO"); return -1;
    }

    // Calibration par défaut: "pulse_us" → valeurs fictives (à ajuster)
    // Idée: 100 µs ≈ 10 A ; 100 µs ≈ 230 V (exemple, à affiner en essais)
    bea->scale_current  = 0.10;  // 0.10 A par µs (donc 100 µs = 10 A)
    bea->offset_current = 0.0;
    bea->scale_voltage  = 2.30;  // 2.30 V par µs (donc 100 µs = 230 V)
    bea->offset_voltage = 0.0;

    return 0;
}

void bea_set_current_calib(bea_t *bea, double scale_A_per_us, double offset_A) {
    bea->scale_current  = scale_A_per_us;
    bea->offset_current = offset_A;
}

void bea_set_voltage_calib(bea_t *bea, double scale_V_per_us, double offset_V) {
    bea->scale_voltage  = scale_V_per_us;
    bea->offset_voltage = offset_V;
}

double bea_measure_pulse_us(bea_t *bea) {
    if (!bea || !bea->trig || !bea->echo) return -1.0;

    // Génère l’impulsion TRIG ~10µs
    gpiod_line_set_value(bea->trig, 0);
    usleep(2);
    gpiod_line_set_value(bea->trig, 1);
    usleep(10);
    gpiod_line_set_value(bea->trig, 0);

    // Attente du front montant ECHO (dans la limite d’un timeout)
    struct timespec start, end;
    int timeout = 0;
    while (gpiod_line_get_value(bea->echo) == 0) {
        ts_now(&start);
        // petit sleep pour ne pas saturer CPU, mais garder réactivité
        usleep(5);
        if (++timeout > 60000) { // ~300 ms
            return -2.0; // timeout avant front montant
        }
    }

    // Mesure jusqu’au front descendant ECHO
    timeout = 0;
    while (gpiod_line_get_value(bea->echo) == 1) {
        ts_now(&end);
        usleep(5);
        if (++timeout > 60000) { // ~300 ms
            return -3.0; // timeout avant front descendant
        }
    }

    double pulse_us = (end.tv_sec - start.tv_sec) * 1e6
                    + (end.tv_nsec - start.tv_nsec) / 1e3;
    if (pulse_us < 0) return -4.0;
    return pulse_us;
}

double bea_sample_current_A(bea_t *bea) {
    double p = bea_measure_pulse_us(bea);
    if (p < 0) return p; // code d’erreur négatif
    return bea->scale_current * p + bea->offset_current;
}

double bea_sample_voltage_V(bea_t *bea) {
    double p = bea_measure_pulse_us(bea);
    if (p < 0) return p; // code d’erreur négatif
    return bea->scale_voltage * p + bea->offset_voltage;
}

static double compute_rms(const double *buf, int n) {
    if (n <= 0) return -1.0;
    double sumsq = 0.0;
    for (int i = 0; i < n; ++i) sumsq += buf[i] * buf[i];
    return sqrt(sumsq / n);
}

double bea_rms_current_A(bea_t *bea, int samples, int sleep_between_samples_us) {
    if (samples <= 0) return -1.0;
    double acc[128];
    if (samples > (int)(sizeof(acc)/sizeof(acc[0]))) return -2.0;

    for (int i = 0; i < samples; ++i) {
        double v = bea_sample_current_A(bea);
        if (v < -1.0) return v; // relaie l’erreur
        acc[i] = v;
        if (sleep_between_samples_us > 0) usleep(sleep_between_samples_us);
    }
    return compute_rms(acc, samples);
}

double bea_rms_voltage_V(bea_t *bea, int samples, int sleep_between_samples_us) {
    if (samples <= 0) return -1.0;
    double acc[128];
    if (samples > (int)(sizeof(acc)/sizeof(acc[0]))) return -2.0;

    for (int i = 0; i < samples; ++i) {
        double v = bea_sample_voltage_V(bea);
        if (v < -1.0) return v; // relaie l’erreur
        acc[i] = v;
        if (sleep_between_samples_us > 0) usleep(sleep_between_samples_us);
    }
    return compute_rms(acc, samples);
}

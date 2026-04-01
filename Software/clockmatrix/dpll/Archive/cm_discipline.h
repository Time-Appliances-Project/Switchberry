// cm_discipline.h
//
// Standalone disciplining loop utility for Renesas/IDT ClockMatrix 8A3400x.
//
// Measurement:
//   Status.DPLL{meas_dpll}_PHASE_STATUS (signed 36-bit in ITDC_UI units).
//   For the ClockMatrix default input TDC frequency of 625 MHz:
//     ITDC_UI = 1 / (32 * 625e6) = 50 ps.
//
// Actuation:
//   DPLL_Freq_Write[{target_dpll}].DPLL_WR_FREQ_* (signed 42-bit FFO units 2^-53).
//   The target DPLL must be configured in "write frequency mode".
//
// Control objective:
//   Phase lock to GPS (phase error -> 0) with *continuous* frequency steering
//   only (no phase steps/jumps).

#pragma once

#include <stdint.h>
#include "renesas_cm8a34001_tables.h"  // cm_bus_t + cm_string_* helpers

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned meas_dpll;       // e.g. 5
    unsigned target_dpll;     // e.g. 2 (must be in write-frequency mode)

    double interval_sec;      // update period, e.g. 1.0
    double tau_sec;           // optional LPF on phase measurement (<=0 disables)

    // PI gains on phase error (seconds)
    // cmd_frac = Kp*phase + Ki*integral(phase)
    // where cmd_frac is fractional frequency offset.
    double kp;                // [1/s]
    double ki;                // [1/s^2]

    double max_abs_ppb;       // clamp output command (<=0 disables)
    double max_abs_phase_sec; // ignore samples with |phase| above this (<=0 disables)

    int dry_run;
    int print_each;
} cm_discipline_cfg_t;

int cm_discipline_run(const cm_bus_t *bus,
                      const cm_discipline_cfg_t *cfg,
                      uint64_t iterations);

// Convert signed 36-bit PHASE_STATUS (ITDC_UI units) into seconds.
// Assumes default ITDC frequency of 625 MHz -> 50 ps per ITDC_UI.
double cm_phase_status_to_seconds(int64_t phase_s36);

#ifdef __cplusplus
}
#endif

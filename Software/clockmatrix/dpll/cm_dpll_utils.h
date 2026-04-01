// cm_dpll_utils.h
//
// Shared ClockMatrix 8A3400x helpers used by both:
//   - cm_discipline.c (disciplining servo)
//   - dpll_utility.c  (standalone register / one-shot operations)
//
// Keeps all direct register packing/unpacking and SPI->cm_bus glue in one place.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "renesas_cm8a34001_tables.h"  // cm_bus_t + cm_string_* helpers

#ifdef __cplusplus
extern "C" {
#endif

// Default Input TDC clock used by the ClockMatrix devices.
#define CM_ITDC_HZ           625000000.0
#define CM_ITDC_UI_SEC       (1.0 / (32.0 * CM_ITDC_HZ))  // 50 ps

// DPLL_WR_FREQ is an FFO in units 2^-53 (fractional frequency).
#define CM_WR_FREQ_FRAC_BITS 53

// ---------------------------------------------------------------------------
// SPI -> cm_bus glue
// ---------------------------------------------------------------------------

// Initialize a cm_bus_t that talks to a ClockMatrix over an already-open Linux
// spidev file descriptor.
//
// The cm_bus_t stores a `user` pointer; this helper expects you to pass in a
// stable int storage (spi_fd_user_storage) which it will set to spi_fd and use
// as the user pointer. This makes it easy to keep the bus as a stack object.
void cm_bus_init_spi(cm_bus_t *bus, int *spi_fd_user_storage, int spi_fd);

// ---------------------------------------------------------------------------
// Generic parsing helpers
// ---------------------------------------------------------------------------

// Parse comma-separated list like "9,10,11" into out[]. Returns 0 on success.
int cm_parse_u32_list(const char *s, unsigned *out, size_t out_cap, size_t *out_n);

// ---------------------------------------------------------------------------
// Register helpers
// ---------------------------------------------------------------------------

// Read signed 36-bit Status.DPLL{meas_dpll}_PHASE_STATUS.
int cm_read_phase_status_s36(const cm_bus_t *bus, unsigned meas_dpll, int64_t *out_s36);

// Convert signed 36-bit phase status (ITDC_UI units) into seconds.
double cm_phase_s36_to_seconds(int64_t phase_s36);

// DPLL_Ctrl.DPLL_FOD_FREQ is M/N (Hz). N==0 encodes 1.
int cm_read_dpll_fod_freq_hz(const cm_bus_t *bus, unsigned dpll_idx,
                             double *out_hz, uint64_t *M_out, uint16_t *N_out);

int cm_read_output_div_u32(const cm_bus_t *bus, unsigned out_idx, uint32_t *out_div);

int cm_read_output_phase_adj_s32(const cm_bus_t *bus, unsigned out_idx, int32_t *out_adj);

// Write Output[out_idx].OUT_PHASE_ADJ. If trace!=0 prints write + readback.
// If dry_run!=0 it only prints (if trace) and returns success.
int cm_write_output_phase_adj_s32(const cm_bus_t *bus, unsigned out_idx, int32_t adj,
                                  int trace, int dry_run);

// Read DPLL_Freq_Write[dpll_idx].DPLL_WR_FREQ (signed 42-bit word, units 2^-53).
// If trace!=0 prints the read value and decoded ppb.
int cm_read_dpll_wr_freq_s42(const cm_bus_t *bus, unsigned dpll_idx, int64_t *out_word_s42,
                              int trace);

// Write DPLL_Freq_Write[dpll_idx].DPLL_WR_FREQ (signed 42-bit word, units 2^-53).
// If trace!=0 prints write + readback.
// If dry_run!=0 it only prints (if trace) and returns success.
int cm_write_dpll_wr_freq_s42(const cm_bus_t *bus, unsigned dpll_idx, int64_t word_s42,
                              int trace, int dry_run);

#ifdef __cplusplus
}
#endif

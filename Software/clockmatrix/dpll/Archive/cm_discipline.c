// cm_discipline.c
//
// Standalone disciplining utility for Renesas/IDT ClockMatrix 8A3400x.
//
// State machine (inspired by ptp4l/ts2phc):
//   S0 (measure): average phase for a short window.
//   S1 (step):    coarse output phase correction via OUTPUT_x.OUT_PHASE_ADJ (FOD cycles).
//   S2 (slew):    continuous frequency steering via DPLL_WR_FREQ (write-frequency mode).
//
// Measurement source:
//   STATUS.DPLL{meas}_PHASE_STATUS: signed 36-bit phase offset in ITDC_UI.
//   Assuming default ITDC clock 625 MHz => ITDC_UI = 1/(32*625e6) = 50 ps.
//
// Coarse phase step:
//   OUTPUT_x.OUT_PHASE_ADJ: signed 32-bit value in *FOD cycles*.
//   FOD frequency is reported by DPLL_Ctrl.DPLL_FOD_FREQ_M/N (Hz) as M/N.
//   Output frequency is FOD_Hz / OUT_DIV.
//
// Frequency slew:
//   DPLL_Freq_Write[n].DPLL_WR_FREQ: signed 42-bit fractional frequency offset in units 2^-53.
//   Target DPLL(s) must be configured in write-frequency mode.

#define _GNU_SOURCE

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <linux/spi/spidev.h>

#include "linux_dpll.h"
#include "tcs_dpll.h"
#include "renesas_cm8a34001_tables.h"

// ---------------------------- Constants ------------------------------------

// Default Input TDC clock used by the ClockMatrix devices.
#define CM_ITDC_HZ               625000000.0
#define CM_ITDC_UI_SEC           (1.0 / (32.0 * CM_ITDC_HZ))  // 50 ps

// DPLL_WR_FREQ is an FFO in units 2^-53 (fractional frequency).
#define CM_WR_FREQ_FRAC_BITS     53

// Sign conventions are messy across configs; keep a single global polarity knob.
// If you find the loop runs away, flip --invert.
#define CM_DEFAULT_POLARITY      (+1.0)

// ---------------------------- SPI -> cm_bus --------------------------------

static int g_spi_fd = -1;
static cm_bus_t g_cm_bus;

static int cm_spi_read8(void *user, uint16_t addr, uint8_t *val)
{
    int spi_fd = *(int *)user;
    uint8_t tmp = 0;
    dpll_result_t r = dpll_read8(spi_fd, addr, &tmp);
    if (r != DPLL_OK) return -1;
    *val = tmp;
    return 0;
}

static int cm_spi_write8(void *user, uint16_t addr, uint8_t val)
{
    int spi_fd = *(int *)user;
    dpll_result_t r = dpll_write8(spi_fd, addr, val);
    return (r == DPLL_OK) ? 0 : -1;
}

static int cm_spi_read(void *user, uint16_t addr, uint8_t *buf, size_t len)
{
    int spi_fd = *(int *)user;
    dpll_result_t r = dpll_read_seq(spi_fd, addr, buf, len);
    return (r == DPLL_OK) ? 0 : -1;
}

static int cm_spi_write(void *user, uint16_t addr, const uint8_t *buf, size_t len)
{
    int spi_fd = *(int *)user;
    dpll_result_t r = dpll_write_seq(spi_fd, addr, buf, len);
    return (r == DPLL_OK) ? 0 : -1;
}

static void cm_init_bus_for_spi(int spi_fd)
{
    g_spi_fd = spi_fd;
    g_cm_bus.user   = &g_spi_fd;
    g_cm_bus.read8  = cm_spi_read8;
    g_cm_bus.write8 = cm_spi_write8;
    g_cm_bus.read   = cm_spi_read;
    g_cm_bus.write  = cm_spi_write;
}

// ---------------------------- Utilities ------------------------------------

static double now_monotonic_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void sleep_sec(double sec)
{
    if (sec <= 0.0) return;
    struct timespec ts;
    ts.tv_sec  = (time_t)sec;
    ts.tv_nsec = (long)((sec - (double)ts.tv_sec) * 1e9);
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {}
}

static double clamp_d(double x, double lo, double hi)
{
    return (x < lo) ? lo : (x > hi) ? hi : x;
}

static int64_t clamp_s42(int64_t x)
{
    const int64_t lo = -(1LL << 41);
    const int64_t hi =  (1LL << 41) - 1;
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static double wrap_phase_sec(double x)
{
    // Wrap to [-0.5, +0.5) seconds.
    while (x >= 0.5) x -= 1.0;
    while (x < -0.5) x += 1.0;
    return x;
}

static int parse_u32_list(const char *s, unsigned *out, size_t out_cap, size_t *out_n)
{
    // Parses comma-separated list like "9,10,11".
    // Returns 0 on success.
    if (!s || !*s) return -1;

    char *tmp = strdup(s);
    if (!tmp) return -1;

    size_t n = 0;
    char *save = NULL;
    for (char *tok = strtok_r(tmp, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        if (n >= out_cap) {
            free(tmp);
            return -1;
        }
        char *end = NULL;
        long v = strtol(tok, &end, 0);
        if (!end || *end != '\0' || v < 0) {
            free(tmp);
            return -1;
        }
        out[n++] = (unsigned)v;
    }
    free(tmp);
    *out_n = n;
    return (n > 0) ? 0 : -1;
}

// ---------------------------- Register helpers -----------------------------

// Read signed 36-bit PHASE_STATUS (stored in 5 bytes, sign in bit 35).
static int read_phase_status_s36(const cm_bus_t *bus, unsigned meas_dpll, int64_t *out_s36)
{
    char reg_name[64];
    snprintf(reg_name, sizeof(reg_name), "DPLL%u_PHASE_STATUS", meas_dpll);

    uint8_t buf[5] = {0};
    int rc = cm_string_read_bytes(bus, "Status", 0, reg_name, buf, sizeof(buf));
    if (rc != 0) return rc;

    // Little-endian 40 bits, but only bits[35:0] valid.
    uint64_t v = 0;
    v |= (uint64_t)buf[0] << 0;
    v |= (uint64_t)buf[1] << 8;
    v |= (uint64_t)buf[2] << 16;
    v |= (uint64_t)buf[3] << 24;
    v |= (uint64_t)buf[4] << 32;
    v &= ((1ULL << 36) - 1ULL);

    // Sign extend 36-bit.
    int64_t s = (int64_t)v;
    if (v & (1ULL << 35)) {
        s |= ~((int64_t)((1ULL << 36) - 1ULL));
    }

    *out_s36 = s;
    return 0;
}

static double phase_s36_to_seconds(int64_t phase_s36)
{
    return (double)phase_s36 * CM_ITDC_UI_SEC;
}

// DPLL_Ctrl.DPLL_FOD_FREQ is M/N (Hz). N==0 encodes 1.
static int read_dpll_fod_freq_hz(const cm_bus_t *bus, unsigned dpll_idx, double *out_hz, uint64_t *M_out, uint16_t *N_out)
{
    uint8_t bufM[6] = {0};
    uint8_t bufN[2] = {0};

    int rc = cm_string_read_bytes(bus, "DPLL_Ctrl", dpll_idx, "FOD_FREQ_M_7_0", bufM, sizeof(bufM));
    if (rc) return rc;
    rc = cm_string_read_bytes(bus, "DPLL_Ctrl", dpll_idx, "FOD_FREQ_N_7_0", bufN, sizeof(bufN));
    if (rc) return rc;

    uint64_t M = 0;
    M |= (uint64_t)bufM[0] << 0;
    M |= (uint64_t)bufM[1] << 8;
    M |= (uint64_t)bufM[2] << 16;
    M |= (uint64_t)bufM[3] << 24;
    M |= (uint64_t)bufM[4] << 32;
    M |= (uint64_t)bufM[5] << 40;

    uint16_t N = (uint16_t)((uint16_t)bufN[0] | ((uint16_t)bufN[1] << 8));
    if (N == 0) N = 1;

    if (M_out) *M_out = M;
    if (N_out) *N_out = N;

    *out_hz = (double)M / (double)N;
    return 0;
}

static int read_output_div_u32(const cm_bus_t *bus, unsigned out_idx, uint32_t *out_div)
{
    uint8_t b[4] = {0};
    int rc = cm_string_read_bytes(bus, "Output", out_idx, "OUT_DIV", b, sizeof(b));
    if (rc) return rc;
    uint32_t v = 0;
    v |= (uint32_t)b[0] << 0;
    v |= (uint32_t)b[1] << 8;
    v |= (uint32_t)b[2] << 16;
    v |= (uint32_t)b[3] << 24;
    *out_div = v;
    return 0;
}

static int read_output_phase_adj_s32(const cm_bus_t *bus, unsigned out_idx, int32_t *out_adj)
{
    uint8_t b[4] = {0};
    int rc = cm_string_read_bytes(bus, "Output", out_idx, "OUT_PHASE_ADJ_7_0", b, sizeof(b));
    if (rc) return rc;
    uint32_t u = 0;
    u |= (uint32_t)b[0] << 0;
    u |= (uint32_t)b[1] << 8;
    u |= (uint32_t)b[2] << 16;
    u |= (uint32_t)b[3] << 24;
    *out_adj = (int32_t)u;
    return 0;
}

static int write_output_phase_adj_s32(const cm_bus_t *bus, unsigned out_idx, int32_t adj,
                                      int trace, int dry_run)
{
    uint8_t b[4];
    b[0] = (uint8_t)((uint32_t)adj & 0xFF);
    b[1] = (uint8_t)(((uint32_t)adj >> 8) & 0xFF);
    b[2] = (uint8_t)(((uint32_t)adj >> 16) & 0xFF);
    b[3] = (uint8_t)(((uint32_t)adj >> 24) & 0xFF);

    if (trace) {
        fprintf(stdout,
                "WRITE: Output[%u].OUT_PHASE_ADJ <= %d (0x%08x) bytes=%02x %02x %02x %02x %s\n",
                out_idx, adj, (uint32_t)adj, b[0], b[1], b[2], b[3],
                dry_run ? "(dry-run)" : "");
        fflush(stdout);
    }

    if (dry_run) return 0;

    int rc = cm_string_write_bytes(bus, "Output", out_idx, "OUT_PHASE_ADJ_7_0", b, sizeof(b));

    if (trace) {
        fprintf(stdout, "WRITE: Output[%u].OUT_PHASE_ADJ rc=%d\n", out_idx, rc);
        fflush(stdout);
        if (rc == 0) {
            int32_t rb = 0;
            int rrc = read_output_phase_adj_s32(bus, out_idx, &rb);
            fprintf(stdout,
                    "WRITE: Output[%u].OUT_PHASE_ADJ readback=%d (0x%08x) rrc=%d\n",
                    out_idx, rb, (uint32_t)rb, rrc);
            fflush(stdout);
        }
    }

    return rc;
}

static int write_dpll_wr_freq_s42(const cm_bus_t *bus, unsigned dpll_idx, int64_t word_s42,
                                   int trace, int dry_run)
{
    // DPLL_WR_FREQ is a signed 42-bit FFO word in units of 2^(-53).
    // Stored as 6 bytes little-endian; device uses low 42 bits.
    uint64_t u = (uint64_t)word_s42 & ((1ULL << 42) - 1ULL);

    uint8_t b[6];
    b[0] = (uint8_t)(u & 0xFF);
    b[1] = (uint8_t)((u >> 8) & 0xFF);
    b[2] = (uint8_t)((u >> 16) & 0xFF);
    b[3] = (uint8_t)((u >> 24) & 0xFF);
    b[4] = (uint8_t)((u >> 32) & 0xFF);
    b[5] = (uint8_t)((u >> 40) & 0xFF);

    // For human readability: convert to fractional frequency and ppb.
    const double cmd_frac = ldexp((double)word_s42, -53);
    const double cmd_ppb = cmd_frac * 1e9;

    if (trace) {
        fprintf(stdout,
                "WRITE: DPLL_Freq_Write[%u].DPLL_WR_FREQ <= word_s42=%lld cmd=%.6f ppb bytes=%02x %02x %02x %02x %02x %02x %s\n",
                dpll_idx, (long long)word_s42, cmd_ppb,
                b[0], b[1], b[2], b[3], b[4], b[5],
                dry_run ? "(dry-run)" : "");
        fflush(stdout);
    }

    if (dry_run) return 0;

    int rc = cm_string_write_bytes(bus, "DPLL_Freq_Write", dpll_idx, "DPLL_WR_FREQ_7_0", b, sizeof(b));

    if (trace) {
        fprintf(stdout, "WRITE: DPLL_Freq_Write[%u].DPLL_WR_FREQ rc=%d\n", dpll_idx, rc);
        fflush(stdout);

        if (rc == 0) {
            uint8_t rb[6] = {0};
            int rrc = cm_string_read_bytes(bus, "DPLL_Freq_Write", dpll_idx, "DPLL_WR_FREQ_7_0", rb, sizeof(rb));
            uint64_t ru = 0;
            for (int i = 0; i < 6; i++) ru |= ((uint64_t)rb[i]) << (8 * i);
            ru &= ((1ULL << 42) - 1ULL);

            int64_t rs = (int64_t)ru;
            if (ru & (1ULL << 41)) rs |= ~((int64_t)((1ULL << 42) - 1ULL)); // sign extend
            double rfrac = ldexp((double)rs, -53);
            double rppb = rfrac * 1e9;

            fprintf(stdout,
                    "WRITE: DPLL_Freq_Write[%u].DPLL_WR_FREQ readback word_s42=%lld cmd=%.6f ppb bytes=%02x %02x %02x %02x %02x %02x rrc=%d\n",
                    dpll_idx, (long long)rs, rppb,
                    rb[0], rb[1], rb[2], rb[3], rb[4], rb[5], rrc);
            fflush(stdout);
        }
    }

    return rc;
}

// ---------------------------- Discipline logic -----------------------------

typedef enum {
    ST_S0_MEASURE = 0,
    ST_S1_STEP    = 1,
    ST_S2_SLEW    = 2,
} discipline_state_t;

typedef struct {
    // Measurement DPLL (phase measurement mode)
    unsigned meas_dpll;

    // Slew target DPLLs (write-frequency mode)
    unsigned wr_dplls[8];
    size_t   wr_dplls_n;

    // Step outputs (Q9/Q10/Q11 => out_idx 9/10/11)
    unsigned step_outs[16];
    size_t   step_outs_n;

    // Servo timing
    double interval_sec;   // loop period

    // S0
    double s0_window_sec;  // how long to average before choosing state

    // S1 thresholds
    double s1_enter_abs_sec;   // if |phase| > enter => step
    double s1_exit_abs_sec;    // after stepping, if |phase| <= exit => go S2
    double s1_max_step_sec;    // max absolute phase correction per single step action
    unsigned s1_max_iters;     // max step attempts before entering S2 (0 = unlimited; stay in S1)
    unsigned s1_verify_samples;// how many samples to take after step for verification

    // S2
    double kp;            // proportional gain (1/s)
    double ki;            // integral gain (1/s^2)
    double max_abs_ppb;   // clamp (<=0 disables)
    double s2_fallback_abs_sec; // if |phase| > this, go back to S1

    // Measurement conditioning
    double max_abs_phase_sec;  // ignore samples with |phase| above this (<=0 disables)

    // Polarity
    int invert;

    // Logging
    int print_each;
    int debug;

    // Safety
    int dry_run;

} discipline_cfg_t;

static void dbg(const discipline_cfg_t *cfg, const char *fmt, ...)
{
    if (!cfg->debug) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static int want_print(const discipline_cfg_t *cfg)
{
    return (cfg && (cfg->print_each || cfg->debug));
}



// Hardware-defined FOD mapping for coarse OUT_PHASE_ADJ steps on this board.
// Q9 steps are in FOD cycles of DPLL5; Q10/Q11 steps are in FOD cycles of DPLL6.
static unsigned fod_dpll_for_output(unsigned out_num)
{
    switch (out_num) {
        case 9:  return 5;
        case 10: return 6;
        case 11: return 6;
        default:
            // If you include other outputs in --step-outs, pick a safe default.
            // (You can extend this mapping as needed.)
            return 5;
    }
}

static void sanity_print_dpll_fod(const cm_bus_t *bus, const discipline_cfg_t *cfg, unsigned dpll_idx)
{
    if (!want_print(cfg)) return;

    double hz = 0.0;
    uint64_t M = 0;
    uint16_t N = 0;
    int rc = read_dpll_fod_freq_hz(bus, dpll_idx, &hz, &M, &N);
    if (rc) {
        fprintf(stderr, "Sanity: failed to read DPLL%u FOD freq (rc=%d)\n", dpll_idx, rc);
        return;
    }
    fprintf(stdout, "Sanity: DPLL%u FOD: M=%llu N=%u => FOD=%.6f Hz\n", dpll_idx, (unsigned long long)M, (unsigned)N, hz);
    fflush(stdout);
}

static void sanity_print_outputs(const cm_bus_t *bus, const discipline_cfg_t *cfg)
{
    if (!want_print(cfg)) return;

    fprintf(stdout, "Sanity: step_outs_n=%zu\n", cfg->step_outs_n);
    for (size_t i = 0; i < cfg->step_outs_n; i++) {
        const unsigned out_idx = cfg->step_outs[i];
        const unsigned fod_dpll = fod_dpll_for_output(out_idx);

        double fod_hz = 0.0;
        uint64_t M = 0;
        uint16_t N = 0;
        int rc_f = read_dpll_fod_freq_hz(bus, fod_dpll, &fod_hz, &M, &N);

        uint32_t out_div = 0;
        int32_t out_adj = 0;
        int rc_d = read_output_div_u32(bus, out_idx, &out_div);
        int rc_a = read_output_phase_adj_s32(bus, out_idx, &out_adj);

        double out_hz = 0.0;
        if (rc_f == 0 && rc_d == 0 && out_div != 0) out_hz = fod_hz / (double)out_div;

        fprintf(stdout,
                "Sanity: OUT%u uses FOD from DPLL%u (M=%llu N=%u FOD=%.6f Hz) "
                "OUT_DIV=%u => OUT_HZ=%.6f Hz OUT_PHASE_ADJ=%d "
                "(rc_f=%d rc_div=%d rc_adj=%d)\n",
                out_idx, fod_dpll,
                (unsigned long long)M, (unsigned)N, fod_hz,
                (unsigned)out_div, out_hz,
                (int)out_adj,
                rc_f, rc_d, rc_a);
    }
    fflush(stdout);
}


// Forward decl
static int read_phase_sec(const cm_bus_t *bus, const discipline_cfg_t *cfg, double *phase_raw, double *phase_wrapped);

static void sanity_print_phase_once(const cm_bus_t *bus, const discipline_cfg_t *cfg)
{
    if (!want_print(cfg)) return;
    double p_raw = 0.0, p = 0.0;
    int rc = read_phase_sec(bus, cfg, &p_raw, &p);
    if (rc) {
        fprintf(stderr, "Sanity: failed to read phase (rc=%d)\n", rc);
        return;
    }
    fprintf(stdout, "Sanity: phase_raw=%.9e sec phase_wrapped=%.9e sec (meas_dpll=%u)\n",
            
            p_raw, p, cfg->meas_dpll);
    fflush(stdout);
}



// Take a single phase measurement in seconds (wrapped to [-0.5,0.5)).
static int read_phase_sec(const cm_bus_t *bus, const discipline_cfg_t *cfg, double *phase_raw, double *phase_wrapped)
{
    int64_t s36 = 0;
    int rc = read_phase_status_s36(bus, cfg->meas_dpll, &s36);
    if (rc) return rc;

    double p = phase_s36_to_seconds(s36);
    if (cfg->invert) p = -p;

    *phase_raw = p;
    *phase_wrapped = wrap_phase_sec(p);
    return 0;
}

static int do_s0_measure(const cm_bus_t *bus, const discipline_cfg_t *cfg, double *out_avg_phase)
{
    const double t_start = now_monotonic_sec();
    double sum = 0.0;
    double sumw = 0.0;
    unsigned n = 0;

    while ((now_monotonic_sec() - t_start) < cfg->s0_window_sec) {
        double p_raw = 0.0, p = 0.0;
        int rc = read_phase_sec(bus, cfg, &p_raw, &p);
        if (rc) return rc;

        const int drop = (cfg->max_abs_phase_sec > 0.0 && fabs(p) > cfg->max_abs_phase_sec);
        if (want_print(cfg)) {
            const double t_rel = now_monotonic_sec() - t_start;
            fprintf(stdout, "S0: t=%.3f raw=%.9e sec wrap=%.9e sec %s\n",
                    t_rel, p_raw, p, drop ? "DROP" : "KEEP");
            fflush(stdout);
        }

        if (cfg->max_abs_phase_sec > 0.0 && fabs(p) > cfg->max_abs_phase_sec) {
            dbg(cfg, "S0: drop sample phase=%.9e (>max_abs_phase_sec)\n", p);
        } else {
            sum += p;
            sumw += 1.0;
            n++;
        }
        sleep_sec(cfg->interval_sec);
    }

    const double avg = (sumw > 0.0) ? (sum / sumw) : 0.0;
    if (want_print(cfg)) {
        fprintf(stdout, "S0: samples=%u avg_phase=%.9e sec\n", n, avg);
        fflush(stdout);
    }
    *out_avg_phase = avg;
    return 0;
}

// Apply a phase step to outputs by updating OUT_PHASE_ADJ for each output.
// Returns 0 on success.

// Apply a phase step to outputs by updating OUT_PHASE_ADJ for each output.
// OUT_PHASE_ADJ units are *FOD cycles* for the corresponding output clock tree.
// On this board:
//   - Q9 uses FOD from DPLL5
//   - Q10/Q11 use FOD from DPLL6
static int do_s1_step(const cm_bus_t *bus, const discipline_cfg_t *cfg, double phase_sec)
{
    // Negative feedback: step opposite the measured phase (wrapped in [-0.5,0.5)).
    double step_sec = -phase_sec;

    // Saturate step magnitude per-iteration
    if (cfg->s1_max_step_sec > 0.0) {
        step_sec = clamp_d(step_sec, -cfg->s1_max_step_sec, +cfg->s1_max_step_sec);
    }

    if (want_print(cfg)) {
        fprintf(stdout, "S1: phase=%.9e sec -> step_sec=%.9e sec\n", phase_sec, step_sec);
        fflush(stdout);
    }

    for (size_t i = 0; i < cfg->step_outs_n; i++) {
        const unsigned out_idx = cfg->step_outs[i];
        const unsigned fod_dpll = fod_dpll_for_output(out_idx);

        uint64_t M = 0;
        uint16_t N = 0;
        double fod_hz = 0.0;
        int rc = read_dpll_fod_freq_hz(bus, fod_dpll, &fod_hz, &M, &N);
        if (rc || fod_hz <= 0.0) {
            fprintf(stderr, "S1: failed to read FOD freq for OUT%u (uses DPLL%u) rc=%d fod_hz=%.6f\n", out_idx, fod_dpll, rc, fod_hz);
            return (rc != 0) ? rc : -1;
        }

        const double t_fod = 1.0 / fod_hz;
        const double step_cycles_f = step_sec / t_fod; // == step_sec * fod_hz
        int64_t step_cycles = (int64_t)llround(step_cycles_f);
        if (step_cycles > INT32_MAX) step_cycles = INT32_MAX;
        if (step_cycles < INT32_MIN) step_cycles = INT32_MIN;

        uint32_t out_div = 0;
        int32_t old_adj = 0;
        int rc1 = read_output_div_u32(bus, out_idx, &out_div);
        int rc2 = read_output_phase_adj_s32(bus, out_idx, &old_adj);
        if (rc1 || rc2) {
            fprintf(stderr, "S1: failed to read output %u (div rc=%d, adj rc=%d)\n", out_idx, rc1, rc2);
            return -1;
        }

        const double out_hz = (out_div > 0) ? (fod_hz / (double)out_div) : 0.0;

        int64_t new_adj64 = (int64_t)old_adj + step_cycles;
        if (new_adj64 > INT32_MAX) new_adj64 = INT32_MAX;
        if (new_adj64 < INT32_MIN) new_adj64 = INT32_MIN;
        const int32_t new_adj = (int32_t)new_adj64;

        if (want_print(cfg)) {
            fprintf(stdout, "S1: OUT%u uses DPLL%u FOD: M=%llu N=%u fod_hz=%.6f t_fod=%.9e\n", out_idx, fod_dpll, (unsigned long long)M, (unsigned)N, fod_hz, t_fod);
            fprintf(stdout, "S1: OUT%u step_cycles_f=%.3f -> step_cycles=%lld (FOD cycles)\n", out_idx, step_cycles_f, (long long)step_cycles);
            fprintf(stdout, "S1: OUT%u div=%u out_hz=%.6f old_adj=%d new_adj=%d (delta=%lld)\n", out_idx, out_div, out_hz, old_adj, new_adj, (long long)step_cycles);
            fflush(stdout);
        }

        {
            int wrc = write_output_phase_adj_s32(bus, out_idx, new_adj, want_print(cfg), cfg->dry_run);
            if (wrc) {
                fprintf(stderr, "S1: failed to write OUT%u OUT_PHASE_ADJ (rc=%d)\n", out_idx, wrc);
                return wrc;
            }
        }
    }

    // Give it a moment to settle.
    sleep_sec(0.2);
    return 0;
}


static int do_s2_slew(const cm_bus_t *bus, const discipline_cfg_t *cfg,
                     double *integ_io, double phase_sec, double dt_sec,
                     double *cmd_ppb_out, int64_t *word_out)
{
    // Standard PI on phase (seconds) -> fractional frequency command.
    // Command sign chosen so that positive phase (output late) increases frequency.
    // NOTE: --invert is already applied to the measurement in read_phase_sec().
    // Keep the servo polarity fixed here.
    const double polarity = CM_DEFAULT_POLARITY;

    double integ = *integ_io;
    integ += phase_sec * dt_sec;

    double cmd_frac = polarity * (cfg->kp * phase_sec + cfg->ki * integ);

    if (cfg->max_abs_ppb > 0.0) {
        const double max_frac = cfg->max_abs_ppb * 1e-9;
        cmd_frac = clamp_d(cmd_frac, -max_frac, +max_frac);
    }

    int64_t word = (int64_t)llround(ldexp(cmd_frac, CM_WR_FREQ_FRAC_BITS));
    word = clamp_s42(word);

    // Write to all requested DPLLs.
    int rc = 0;
    {
        for (size_t i = 0; i < cfg->wr_dplls_n; i++) {
            int wrc = write_dpll_wr_freq_s42(bus, cfg->wr_dplls[i], word, want_print(cfg), cfg->dry_run);
            if (wrc) {
                fprintf(stderr, "S2: write DPLL%u WR_FREQ failed (rc=%d)\n", cfg->wr_dplls[i], wrc);
                rc = wrc;
                break;
            }
        }
    }

    *integ_io = integ;
    if (cmd_ppb_out) *cmd_ppb_out = cmd_frac * 1e9;
    if (word_out) *word_out = word;
    return rc;
}

static int discipline_run(const cm_bus_t *bus, const discipline_cfg_t *cfg)
{
    discipline_state_t st = ST_S0_MEASURE;
    double integ = 0.0;

    double t0 = now_monotonic_sec();
    double phase_prev = 0.0;
    int have_prev = 0;

    unsigned s1_iter = 0;

    while (1) {
        if (st == ST_S0_MEASURE) {
            double avg_phase = 0.0;
            int rc = do_s0_measure(bus, cfg, &avg_phase);
            if (rc) return rc;

            if (fabs(avg_phase) > cfg->s1_enter_abs_sec) {
                if (want_print(cfg)) {
                    fprintf(stdout, "S0: |avg_phase|=%.9e > s1_enter=%.9e -> S1 (step)\n", fabs(avg_phase), cfg->s1_enter_abs_sec);
                    fflush(stdout);
                }
                st = ST_S1_STEP;
                s1_iter = 0;
            } else {
                if (want_print(cfg)) {
                    fprintf(stdout, "S0: |avg_phase|=%.9e <= s1_enter=%.9e -> S2 (slew) [note: S1->S2 uses s1_exit=%.9e]\n", fabs(avg_phase), cfg->s1_enter_abs_sec, cfg->s1_exit_abs_sec);
                    fflush(stdout);
                }
                st = ST_S2_SLEW;
                integ = 0.0;
                have_prev = 0;
            }
            continue;
        }

        double p_raw = 0.0, p = 0.0;
        int rc = read_phase_sec(bus, cfg, &p_raw, &p);
        if (rc) {
            fprintf(stderr, "read phase failed (rc=%d)\n", rc);
            return rc;
        }

        const double t_now = now_monotonic_sec();
        const double t_rel = t_now - t0;

        // Drop outliers if requested
        if (cfg->max_abs_phase_sec > 0.0 && fabs(p) > cfg->max_abs_phase_sec) {
            dbg(cfg, "drop sample |phase|=%.3f sec (>max_abs_phase_sec)\n", fabs(p));
            sleep_sec(cfg->interval_sec);
            continue;
        }

        if (st == ST_S1_STEP) {
            // Coarse alignment: keep stepping until we're within exit threshold.

            int rc1 = do_s1_step(bus, cfg, p);
            if (rc1) return rc1;

            // Verify
            double p_ver = 0.0;
            double prev_pw = 0.0;
            int have_prev_pw = 0;
            double drift_sum_ppb = 0.0;
            unsigned drift_n = 0;
            for (unsigned k = 0; k < cfg->s1_verify_samples; k++) {
                double pr = 0.0, pw = 0.0;
                int rcv = read_phase_sec(bus, cfg, &pr, &pw);
                if (rcv) return rcv;
                p_ver = pw;

                if (want_print(cfg)) {
                    if (have_prev_pw) {
                        const double dphi = wrap_phase_sec(pw - prev_pw);
                        const double drift_ppb = (cfg->interval_sec > 0.0) ? (dphi / cfg->interval_sec) * 1e9 : 0.0;
                        fprintf(stdout, "S1: verify[%u] phase=%.9e sec  dphi=%.9e sec  drift=%.3f ppb\n", k, pw, dphi, drift_ppb);
                    } else {
                        fprintf(stdout, "S1: verify[%u] phase=%.9e sec\n", k, pw);
                    }
                    fflush(stdout);
                }

                if (have_prev_pw) {
                    const double dphi = wrap_phase_sec(pw - prev_pw);
                    const double drift_ppb = (cfg->interval_sec > 0.0) ? (dphi / cfg->interval_sec) * 1e9 : 0.0;
                    drift_sum_ppb += drift_ppb;
                    drift_n++;
                }
                prev_pw = pw;
                have_prev_pw = 1;
                sleep_sec(cfg->interval_sec);
            }

            const double s1_avg_drift_ppb = (drift_n > 0) ? (drift_sum_ppb / (double)drift_n) : 0.0;

            s1_iter++;
            if (fabs(p_ver) <= cfg->s1_exit_abs_sec) {
                if (want_print(cfg)) {
                    fprintf(stdout, "S1: within exit threshold (|phase|<=%.3e), applying phase+freq and entering S2\n",
                            cfg->s1_exit_abs_sec);
                    fprintf(stdout, "S1: exit phase=%.9e sec, avg drift=%.3f ppb (from %u diffs)\n",
                            p_ver, s1_avg_drift_ppb, drift_n);
                    fflush(stdout);
                }

                // Final small phase correction (same stepping mechanism as S1).
                int rcf = do_s1_step(bus, cfg, p_ver);
                if (rcf) return rcf;

                // One-shot frequency initialization based on measured drift during S1 verify.
                // Sign convention matches do_s2_slew(): positive phase drift implies output is too slow -> increase frequency.
                double cmd_ppb0 = -s1_avg_drift_ppb; // cancel measured phase slope (flatten trend)

                if (cfg->max_abs_ppb > 0.0) {
                    cmd_ppb0 = clamp_d(cmd_ppb0, -cfg->max_abs_ppb, +cfg->max_abs_ppb);
                }

                double cmd_frac0 = cmd_ppb0 * 1e-9;
                int64_t word0 = (int64_t)llround(ldexp(cmd_frac0, CM_WR_FREQ_FRAC_BITS));
                word0 = clamp_s42(word0);

                if (want_print(cfg)) {
                    fprintf(stdout, "S1->S2: init WR_FREQ cmd=%.3f ppb  word_s42=%" PRId64 " (0x%016" PRIx64 ")\n",
                            cmd_ppb0, word0, (uint64_t)word0);
                    fflush(stdout);
                }

                for (size_t i = 0; i < cfg->wr_dplls_n; i++) {
                    int wrc = write_dpll_wr_freq_s42(bus, cfg->wr_dplls[i], word0, want_print(cfg), cfg->dry_run);
                    if (wrc) {
                        fprintf(stderr, "S1->S2: write DPLL%u WR_FREQ failed (rc=%d)\n", cfg->wr_dplls[i], wrc);
                        return wrc;
                    }
                }

                // Optional sign sanity-check: after applying initial WR_FREQ, measure the resulting phase slope.
                // If the slope magnitude gets worse, flip the sign once.
                if (!cfg->dry_run && cfg->debug && cfg->interval_sec > 0.0 && drift_n >= 2) {
                    double p0_raw = 0.0, p0 = 0.0, p1_raw = 0.0, p1 = 0.0;
                    int rc_p0 = read_phase_sec(bus, cfg, &p0_raw, &p0);
                    if (rc_p0) return rc_p0;
                    sleep_sec(cfg->interval_sec);
                    int rc_p1 = read_phase_sec(bus, cfg, &p1_raw, &p1);
                    if (rc_p1) return rc_p1;

                    const double dphi_chk = wrap_phase_sec(p1 - p0);
                    const double drift_chk_ppb = (dphi_chk / cfg->interval_sec) * 1e9;

                    dbg(cfg, "S1->S2: post-WR_FREQ drift check: p0=%.9e p1=%.9e dphi=%.9e drift=%.3f ppb (pre avg drift=%.3f)\n",
                        p0, p1, dphi_chk, drift_chk_ppb, s1_avg_drift_ppb);

                    if (fabs(drift_chk_ppb) > fabs(s1_avg_drift_ppb) * 1.20) {
                        // Flip once
                        cmd_ppb0 = -cmd_ppb0;
                        cmd_frac0 = (cmd_ppb0 * 1e-9) / 1.0;
                        word0 = (int64_t)llround(ldexp(cmd_frac0, CM_WR_FREQ_FRAC_BITS));
                        word0 = clamp_s42(word0);

                        fprintf(stdout, "S1->S2: drift got worse; flipping init WR_FREQ sign -> cmd=%.3f ppb word_s42=%" PRId64 " (0x%016" PRIx64 ")\n",
                                cmd_ppb0, word0, (uint64_t)word0);
                        fflush(stdout);

                        for (size_t i = 0; i < cfg->wr_dplls_n; i++) {
                            int wrc = write_dpll_wr_freq_s42(bus, cfg->wr_dplls[i], word0, want_print(cfg), cfg->dry_run);
                            if (wrc) {
                                fprintf(stderr, "S1->S2: write DPLL%u WR_FREQ failed (rc=%d)\n", cfg->wr_dplls[i], wrc);
                                return wrc;
                            }
                        }
                    }
                }


                // Initialize PI integrator so the first S2 iteration starts near cmd_ppb0.
                // cmd_frac = polarity*(kp*phase + ki*integ); with phase ~0, choose integ ~= cmd_frac/(polarity*ki).
                if (cfg->ki > 0.0) {
                    integ = cmd_frac0 / (CM_DEFAULT_POLARITY * cfg->ki);
                } else {
                    integ = 0.0;
                }

                st = ST_S2_SLEW;
                have_prev = 0;
            }
            continue;
        }

        // ST_S2_SLEW
        if (fabs(p) > cfg->s2_fallback_abs_sec) {
            if (want_print(cfg)) {
                fprintf(stdout, "S2: |phase|=%.6f exceeds fallback %.6f -> S1\n", fabs(p), cfg->s2_fallback_abs_sec);
                fflush(stdout);
            }
            st = ST_S1_STEP;
            s1_iter = 0;
            continue;
        }

        // Optional drift estimate for printing
        double drift_ppb = NAN;
        if (have_prev) {
            double dphi = wrap_phase_sec(p - phase_prev);
            drift_ppb = (dphi / cfg->interval_sec) * 1e9;
        }
        phase_prev = p;
        have_prev = 1;

        double cmd_ppb = 0.0;
        int64_t word = 0;
        int rc2 = do_s2_slew(bus, cfg, &integ, p, cfg->interval_sec, &cmd_ppb, &word);
        if (rc2) return rc2;

        if (want_print(cfg)) {
            char drift_str[64];
            if (isnan(drift_ppb)) {
                snprintf(drift_str, sizeof(drift_str), "nan");
            } else {
                snprintf(drift_str, sizeof(drift_str), "%.3fppb", drift_ppb);
            }

            fprintf(stdout,
                    "t=%.3f S2 phase_raw=%.9e phase=%.9e drift=%s cmd=%.3fppb word_s42=%lld%s\n",
                    t_rel,
                    p_raw,
                    p,
                    drift_str,
                    cmd_ppb,
                    (long long)word,
                    cfg->dry_run ? " (dry)" : "");
            fflush(stdout);
        }

        sleep_sec(cfg->interval_sec);
    }
}

// ----------------------------- CLI -----------------------------------------

static void usage(const char *argv0)
{
    fprintf(stderr,
        "Usage: %s [SPI opts] [discipline opts]\n\n"
        "SPI opts:\n"
        "  --spidev <path>        default /dev/spidev7.0\n"
        "  --busnum <n>           builds /dev/spidev<n>.<csnum>\n"
        "  --csnum <m>            default 0\n"
        "  --hz <Hz>              default 1000000\n"
        "  --mode <0-3>           default 0\n\n"
        "Discipline opts:\n"
        "  --meas-dpll <n>         DPLL index to read PHASE_STATUS from (default 5)\n"
        "  --wr-dplls <list>       DPLLs to write WR_FREQ to, e.g. 2 or 5,6 (default: target-dpll)\n"
        "  --target-dpll <n>       alias for --wr-dplls <n> (default 2)\n"
        "  --interval <sec>        loop period (default 1.0)\n"
        "  --invert                invert measurement polarity\n"
        "  --dry-run               don't write registers\n"
        "  --print                 print each adjustment\n"
        "  --debug                 extra debug prints\n\n"
        "S0 (measure):\n"
        "  --s0-window <sec>       averaging window (default 5.0)\n\n"
        "S1 (step):\n"
        "  --step-outs <list>       outputs to phase-step, e.g. 9,10,11 (default 9,10,11)\n"
        "  --s1-enter <sec>         if |phase| > enter -> step (default 0.05)\n"
        "  --s1-exit <sec>          if |phase| <= exit -> go to S2 (default 0.002)\n"
        "  --s1-max-step <sec>      max absolute phase correction per step (default 0.2)\n"
        "  --s1-max-iters <n>       max step iterations (default 0; 0 = unlimited)\n"
        "  --s1-verify <n>          verify samples after a step (default 3)\n\n"
        "S2 (slew):\n"
        "  --kp <1/s>               proportional gain on phase (default 0.0)\n"
        "  --ki <1/s^2>             integral gain on phase (default 0.0)\n"
        "  --max-ppb <ppb>          clamp frequency command (default 1000)\n"
        "  --s2-fallback <sec>      if |phase| exceeds this, go back to S1 (default 0.25)\n"
        "  --max-phase <sec>        drop samples with |phase| > max (default 0.5)\n\n",
        argv0);
}

int main(int argc, char **argv)
{
    // SPI defaults
    const char *spidev = "/dev/spidev7.0";
    int busnum = -1;
    int csnum = 0;
    int spi_hz = 1000000;
    int spi_mode = 0;

    discipline_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.meas_dpll = 5;
    cfg.interval_sec = 1.0;
    cfg.s0_window_sec = 5.0;

    cfg.step_outs[0] = 9;
    cfg.step_outs[1] = 10;
    cfg.step_outs[2] = 11;
    cfg.step_outs_n = 3;

    cfg.s1_enter_abs_sec = 0.05;
    cfg.s1_exit_abs_sec  = 0.002;
    cfg.s1_max_step_sec  = 0.05;
    cfg.s1_max_iters     = 0; // 0 = unlimited (stay in S1 until exit threshold)
    cfg.s1_verify_samples= 3;

    cfg.kp = 0.0;
    cfg.ki = 0.0;
    cfg.max_abs_ppb = 1000.0;
    cfg.s2_fallback_abs_sec = 0.25;
    cfg.max_abs_phase_sec = 0.5;

    cfg.print_each = 1;

    // wr_dplls default == target-dpll default 2
    cfg.wr_dplls[0] = 2;
    cfg.wr_dplls_n = 1;

    // Parse args
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
            usage(argv[0]);
            return 1;
        } else if (!strcmp(a, "--spidev") && i + 1 < argc) {
            spidev = argv[++i];
        } else if (!strcmp(a, "--busnum") && i + 1 < argc) {
            busnum = atoi(argv[++i]);
        } else if (!strcmp(a, "--csnum") && i + 1 < argc) {
            csnum = atoi(argv[++i]);
        } else if (!strcmp(a, "--hz") && i + 1 < argc) {
            spi_hz = atoi(argv[++i]);
        } else if (!strcmp(a, "--mode") && i + 1 < argc) {
            spi_mode = atoi(argv[++i]);

        } else if ((!strcmp(a, "--meas-dpll") || !strcmp(a, "--disc-meas-dpll")) && i + 1 < argc) {
            cfg.meas_dpll = (unsigned)strtoul(argv[++i], NULL, 0);
        } else if ((!strcmp(a, "--target-dpll") || !strcmp(a, "--disc-target-dpll")) && i + 1 < argc) {
            cfg.wr_dplls[0] = (unsigned)strtoul(argv[++i], NULL, 0);
            cfg.wr_dplls_n = 1;
        } else if (!strcmp(a, "--wr-dplls") && i + 1 < argc) {
            size_t n = 0;
            if (parse_u32_list(argv[++i], cfg.wr_dplls, 8, &n) != 0) {
                fprintf(stderr, "bad --wr-dplls list\n");
                return 1;
            }
            cfg.wr_dplls_n = n;
        } else if ((!strcmp(a, "--interval") || !strcmp(a, "--disc-interval")) && i + 1 < argc) {
            cfg.interval_sec = atof(argv[++i]);
        } else if (!strcmp(a, "--invert")) {
            cfg.invert = 1;
        } else if (!strcmp(a, "--dry-run") || !strcmp(a, "--disc-dry-run")) {
            cfg.dry_run = 1;
        } else if (!strcmp(a, "--print") || !strcmp(a, "--disc-print")) {
            cfg.print_each = 1;
        } else if (!strcmp(a, "--debug") || !strcmp(a, "--disc-debug")) {
            cfg.debug = 1;

        } else if (!strcmp(a, "--s0-window") && i + 1 < argc) {
            cfg.s0_window_sec = atof(argv[++i]);

        } else if (!strcmp(a, "--step-outs") && i + 1 < argc) {
            size_t n = 0;
            if (parse_u32_list(argv[++i], cfg.step_outs, 16, &n) != 0) {
                fprintf(stderr, "bad --step-outs list\n");
                return 1;
            }
            cfg.step_outs_n = n;

        } else if (!strcmp(a, "--s1-enter") && i + 1 < argc) {
            cfg.s1_enter_abs_sec = atof(argv[++i]);
        } else if (!strcmp(a, "--s1-exit") && i + 1 < argc) {
            cfg.s1_exit_abs_sec = atof(argv[++i]);
        } else if (!strcmp(a, "--s1-max-step") && i + 1 < argc) {
            cfg.s1_max_step_sec = atof(argv[++i]);
        } else if (!strcmp(a, "--s1-max-iters") && i + 1 < argc) {
            cfg.s1_max_iters = (unsigned)strtoul(argv[++i], NULL, 0);
        } else if (!strcmp(a, "--s1-verify") && i + 1 < argc) {
            cfg.s1_verify_samples = (unsigned)strtoul(argv[++i], NULL, 0);

        } else if (!strcmp(a, "--kp") && i + 1 < argc) {
            cfg.kp = atof(argv[++i]);
        } else if (!strcmp(a, "--ki") && i + 1 < argc) {
            cfg.ki = atof(argv[++i]);
        } else if ((!strcmp(a, "--max-ppb") || !strcmp(a, "--disc-max-ppb")) && i + 1 < argc) {
            cfg.max_abs_ppb = atof(argv[++i]);
        } else if (!strcmp(a, "--s2-fallback") && i + 1 < argc) {
            cfg.s2_fallback_abs_sec = atof(argv[++i]);
        } else if ((!strcmp(a, "--max-phase") || !strcmp(a, "--disc-max-phase")) && i + 1 < argc) {
            cfg.max_abs_phase_sec = atof(argv[++i]);
        } else {
            fprintf(stderr, "Unknown arg: %s\n", a);
            usage(argv[0]);
            return 1;
        }
    }

    // If busnum was provided, override spidev.
    char spidev_buf[64];
    if (busnum >= 0) {
        snprintf(spidev_buf, sizeof(spidev_buf), "/dev/spidev%d.%d", busnum, csnum);
        spidev = spidev_buf;
    }

    // Basic sanity
    if (cfg.interval_sec <= 0.0) cfg.interval_sec = 1.0;
    if (cfg.s0_window_sec < cfg.interval_sec) cfg.s0_window_sec = cfg.interval_sec;
    if (cfg.wr_dplls_n == 0) {
        cfg.wr_dplls[0] = 2;
        cfg.wr_dplls_n = 1;
    }
    if (cfg.step_outs_n == 0) {
        cfg.step_outs[0] = 9;
        cfg.step_outs[1] = 10;
        cfg.step_outs[2] = 11;
        cfg.step_outs_n = 3;
    }

    // Open SPI
    int fd = dpll_spi_open(spidev, spi_hz, spi_mode);
    if (fd < 0) {
        perror("dpll_spi_open");
        return 1;
    }
    cm_init_bus_for_spi(fd);

    if (want_print(&cfg)) {
        fprintf(stdout,
                "cmdiscipline: meas_dpll=%u wr_dplls=",
                cfg.meas_dpll);
        for (size_t i = 0; i < cfg.wr_dplls_n; i++) {
            fprintf(stdout, "%u%s", cfg.wr_dplls[i], (i + 1 < cfg.wr_dplls_n) ? "," : "");
        }
        fprintf(stdout, " step_outs=");
        for (size_t i = 0; i < cfg.step_outs_n; i++) {
            fprintf(stdout, "%u%s", cfg.step_outs[i], (i + 1 < cfg.step_outs_n) ? "," : "");
        }
        fprintf(stdout, " interval=%.3f dry=%d\n", cfg.interval_sec, cfg.dry_run);
        fflush(stdout);

        // Startup sanity checks (print even during S0 window when --print/--debug)
        sanity_print_dpll_fod(&g_cm_bus, &cfg, 5);
        sanity_print_dpll_fod(&g_cm_bus, &cfg, 6);
        sanity_print_outputs(&g_cm_bus, &cfg);
        sanity_print_phase_once(&g_cm_bus, &cfg);
    }

    int rc = discipline_run(&g_cm_bus, &cfg);

    dpll_spi_close(fd);
    return (rc == 0) ? 0 : 1;
}

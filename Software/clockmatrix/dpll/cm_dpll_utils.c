// cm_dpll_utils.c
// Shared ClockMatrix 8A3400x helpers.

#define _GNU_SOURCE

#include "cm_dpll_utils.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "linux_dpll.h"  // dpll_read*/write* used by SPI glue

// ---------------------------------------------------------------------------
// SPI -> cm_bus glue
// ---------------------------------------------------------------------------

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

void cm_bus_init_spi(cm_bus_t *bus, int *spi_fd_user_storage, int spi_fd)
{
    if (!bus || !spi_fd_user_storage) return;
    *spi_fd_user_storage = spi_fd;

    bus->user   = spi_fd_user_storage;
    bus->read8  = cm_spi_read8;
    bus->write8 = cm_spi_write8;
    bus->read   = cm_spi_read;
    bus->write  = cm_spi_write;
}

// ---------------------------------------------------------------------------
// Generic parsing helpers
// ---------------------------------------------------------------------------

int cm_parse_u32_list(const char *s, unsigned *out, size_t out_cap, size_t *out_n)
{
    if (!s || !*s || !out || !out_n) return -1;

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

// ---------------------------------------------------------------------------
// Register helpers
// ---------------------------------------------------------------------------

int cm_read_phase_status_s36(const cm_bus_t *bus, unsigned meas_dpll, int64_t *out_s36)
{
    if (!bus || !out_s36) return -1;

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

    // Sign-extend 36-bit.
    int64_t s = (int64_t)v;
    if (v & (1ULL << 35)) {
        s |= ~((int64_t)((1ULL << 36) - 1ULL));
    }

    *out_s36 = s;
    return 0;
}

double cm_phase_s36_to_seconds(int64_t phase_s36)
{
    return (double)phase_s36 * CM_ITDC_UI_SEC;
}

int cm_read_dpll_fod_freq_hz(const cm_bus_t *bus, unsigned dpll_idx,
                             double *out_hz, uint64_t *M_out, uint16_t *N_out)
{
    if (!bus || !out_hz) return -1;

    uint8_t bufM[6] = {0};
    uint8_t bufN[2] = {0};

    int rc = cm_string_read_bytes(bus, "DPLL_Ctrl", dpll_idx, "DPLL_FOD_FREQ_M_0_7", bufM, sizeof(bufM));
    if (rc) return rc;
    rc = cm_string_read_bytes(bus, "DPLL_Ctrl", dpll_idx, "DPLL_FOD_FREQ_N_0_7", bufN, sizeof(bufN));
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

int cm_read_output_div_u32(const cm_bus_t *bus, unsigned out_idx, uint32_t *out_div)
{
    if (!bus || !out_div) return -1;
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

int cm_read_output_phase_adj_s32(const cm_bus_t *bus, unsigned out_idx, int32_t *out_adj)
{
    if (!bus || !out_adj) return -1;
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

int cm_write_output_phase_adj_s32(const cm_bus_t *bus, unsigned out_idx, int32_t adj,
                                  int trace, int dry_run)
{
    if (!bus) return -1;

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
            int rrc = cm_read_output_phase_adj_s32(bus, out_idx, &rb);
            fprintf(stdout,
                    "WRITE: Output[%u].OUT_PHASE_ADJ readback=%d (0x%08x) rrc=%d\n",
                    out_idx, rb, (uint32_t)rb, rrc);
            fflush(stdout);
        }
    }

    return rc;
}


int cm_read_dpll_wr_freq_s42(const cm_bus_t *bus, unsigned dpll_idx, int64_t *out_word_s42,
                             int trace)
{
    if (!bus || !out_word_s42) return -1;

    uint8_t rb[6] = {0};
    int rc = cm_string_read_bytes(bus, "DPLL_Freq_Write", dpll_idx, "DPLL_WR_FREQ_7_0", rb, sizeof(rb));
    if (rc) return rc;

    uint64_t ru = 0;
    for (int i = 0; i < 6; i++) ru |= ((uint64_t)rb[i]) << (8 * i);
    ru &= ((1ULL << 42) - 1ULL);

    int64_t rs = (int64_t)ru;
    if (ru & (1ULL << 41)) rs |= ~((int64_t)((1ULL << 42) - 1ULL)); // sign extend

    *out_word_s42 = rs;

    if (trace) {
        const double frac = ldexp((double)rs, -CM_WR_FREQ_FRAC_BITS);
        const double ppb = frac * 1e9;
        fprintf(stdout,
                "READ:  DPLL_Freq_Write[%u].DPLL_WR_FREQ word...s42=%lld cmd=%.6f ppb bytes=%02x %02x %02x %02x %02x %02x rc=%d\n",
                dpll_idx, (long long)rs, ppb,
                rb[0], rb[1], rb[2], rb[3], rb[4], rb[5], rc);
        fflush(stdout);
    }

    return 0;
}

int cm_write_dpll_wr_freq_s42(const cm_bus_t *bus, unsigned dpll_idx, int64_t word_s42,
                              int trace, int dry_run)
{
    if (!bus) return -1;

    // Stored as 6 bytes little-endian; device uses low 42 bits.
    uint64_t u = (uint64_t)word_s42 & ((1ULL << 42) - 1ULL);

    uint8_t b[6];
    b[0] = (uint8_t)(u & 0xFF);
    b[1] = (uint8_t)((u >> 8) & 0xFF);
    b[2] = (uint8_t)((u >> 16) & 0xFF);
    b[3] = (uint8_t)((u >> 24) & 0xFF);
    b[4] = (uint8_t)((u >> 32) & 0xFF);
    b[5] = (uint8_t)((u >> 40) & 0xFF);

    // Human readability: convert to fractional frequency and ppb.
    const double cmd_frac = ldexp((double)word_s42, -CM_WR_FREQ_FRAC_BITS);
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
            double rfrac = ldexp((double)rs, -CM_WR_FREQ_FRAC_BITS);
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

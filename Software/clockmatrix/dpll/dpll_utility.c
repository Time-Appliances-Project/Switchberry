// dpll_utility.c
//
// 8A3400x DPLL SPI read/write/flash utility using Linux spidev,
// linux_dpll.{c,h}, tcs_dpll.{c,h}, and the auto-generated
// renesas_cm8a34001_tables.{c,h} for register and field access.
//
// Build (example):
//   gcc -O2 -g -Wall -Wextra -o dplltool
//       dpll_utility.c linux_dpll.c tcs_dpll.c renesas_cm8a34001_tables.c
//
// Examples (low-level):
//   dplltool --read  0xC024
//   dplltool --write 0xCBE4 0x50
//   dplltool --flash-hex SwitchberryV5_8a34004_eeprom.hex
//   dplltool --tcs-apply SwitchberryV5_8a34004_live.tcs --tcs-debug
//
// High-level examples (used by Python scripts):
//   dplltool set-input-freq   1 25000000
//   dplltool set-input-enable 1 enable
//   dplltool set-chan-input   2 1 1 enable
//   dplltool set-output-freq  3 10000000
//
//   (Also accepts the same forms with leading --, e.g. --set-input-freq.)
//

#include <limits.h>
#include <linux/spi/spidev.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cm_dpll_utils.h"
#include "linux_dpll.h"
#include "renesas_cm8a34001_tables.h"
#include "tcs_dpll.h"

typedef struct {
  struct timespec start;
  struct timespec last_print;
  size_t last_bytes;
} progress_ctx_t;

/* -------------------------------------------------------------------------- */
/* Global SPI fd + cm_bus for the table API                                   */
/* -------------------------------------------------------------------------- */

static int g_spi_fd = -1;
static cm_bus_t g_cm_bus;

/* user points to g_spi_fd */
static int cm_spi_read8(void *user, uint16_t addr, uint8_t *val) {
  int spi_fd = *(int *)user;
  uint8_t tmp = 0;
  dpll_result_t r = dpll_read8(spi_fd, addr, &tmp);
  if (r != DPLL_OK) {
    return -1;
  }
  *val = tmp;
  return 0;
}

static int cm_spi_write8(void *user, uint16_t addr, uint8_t val) {
  int spi_fd = *(int *)user;
  dpll_result_t r = dpll_write8(spi_fd, addr, val);
  return (r == DPLL_OK) ? 0 : -1;
}

static int cm_spi_read(void *user, uint16_t addr, uint8_t *buf, size_t len) {
  int spi_fd = *(int *)user;
  dpll_result_t r = dpll_read_seq(spi_fd, addr, buf, len);
  return (r == DPLL_OK) ? 0 : -1;
}

static int cm_spi_write(void *user, uint16_t addr, const uint8_t *buf,
                        size_t len) {
  int spi_fd = *(int *)user;
  dpll_result_t r = dpll_write_seq(spi_fd, addr, buf, len);
  return (r == DPLL_OK) ? 0 : -1;
}

/* Initialize global cm_bus for a given SPI fd */
static void cm_init_bus_for_spi(int spi_fd) {
  g_spi_fd = spi_fd;
  g_cm_bus.user = &g_spi_fd;
  g_cm_bus.read8 = cm_spi_read8;
  g_cm_bus.write8 = cm_spi_write8;
  g_cm_bus.read = cm_spi_read;
  g_cm_bus.write = cm_spi_write;
}

#if 0
/*
 * Example: using the generated tables (renesas_cm8a34001_tables.*)
 * with this utility's SPI layer.
 *
 * Suppose we want to:
 *   - Inspect the first INPUT block (Input instance 0)
 *   - Enable its IN_EN bit in INPUT_IN_MODE (last reg of the module)
 *
 * The generated tables give us:
 *   - cm_Input_module.bases[inst]   -> base address for that instance
 *   - cm_Input_module.regs[...]     -> registers (with offsets)
 *   - cm_field_read8/cm_field_write8-> bitfield helpers
 *
 * In code (inside main(), once SPI is open and cm_init_bus_for_spi()
 * has been called):
 *
 *   cm_bus_t *bus = &g_cm_bus;
 *
 *   unsigned inst = 0;
 *   uint16_t base = cm_Input_module.bases[inst];
 *
 *   // INPUT_IN_MODE is the last reg in cm_Input_module.regs (index 13 for your table)
 *   const cm_reg_desc_t *in_mode_reg = &cm_Input_module.regs[13];
 *   uint16_t in_mode_addr = (uint16_t)(base + in_mode_reg->offset);
 *
 *   uint8_t in_en = 0;
 *   cm_field_read8(bus, in_mode_addr, 0 , 1 , &in_en);
 *   printf("IN_EN before: %u\n", in_en);
 *
 *   // Enable the input:
 *   cm_field_write8(bus, in_mode_addr, 0 , 1 , 1);
 *
 *   // For debugging, we can dump an entire module instance:
 *   cm_dump_module(bus, &cm_Input_module, inst, printf);
 *
 * The high-level commands like --set-input-freq or --set-output-freq
 * can use the same cm_* helpers internally once you decide which
 * registers/fields control those behaviors.
 */

#endif
/* -------------------------------------------------------------------------- */

static void usage(const char *prog) {
  fprintf(
      stderr,
      "Usage:\n"
      "  (--read <addr> | --write <addr> <data> | --flash-hex <hex>\n"
      "      | --tcs-apply <tcs>\n"
      "      | --prog-file <txt>\n"
      "      | get_state <chan>\n"
      "      | get_statechg_sticky <chan>\n"
      "      | clear_statechg_sticky <chan>\n"
      "      | set_oper_state <chan> <NORMAL|FREERUN|HOLDOVER>\n"
      "      | get_phase <chan>\n"
      "      | --set-input-freq <input> <freq_hz>\n"
      "      | --set-input-enable <input> <enable|disable>\n"
      "      | --set-chan-input <chan> <input> <priority> <enable|disable>\n"
      "      | --set-output-freq <output3 freq_hz> <output4 freq_hz>\n"
      "      | --set-output-divider <output_idx> <divider>\n"
      "      | --set-combo-slave <chan> <master_chan> <enable|disable>\n"
      "     [--spidev <path>]\n"
      "     [--busnum <n> --csnum <m>]\n"
      "     [--hz <freq>] [--mode <0..3>] [--tcs-debug]\n"
      "\n"
      "Actions (exactly one required):\n"
      "  --read <addr>           Read 8-bit value from 16-bit DPLL register "
      "(hex or dec).\n"
      "  --write <addr> <data>   Write 8-bit value to 16-bit DPLL register.\n"
      "  --flash-hex <hex>       Program EEPROM via DPLL's I2C master using "
      "Intel HEX file.\n"
      "  --tcs-apply <tcs>       Apply a Timing Commander .tcs file live "
      "(register writes).\n"
      "  --prog-file <txt>       Apply a Timing Commander programming .txt "
      "file\n"
      "                          (Offset/Size/Data lines via "
      "dpll_apply_program_file).\n"
      "\n"
      "Monitor/daemon helper commands (script-friendly output):\n"
      "  get_state <chan>\n"
      "      Print channel state as a single token "
      "(LOCKED|LOCKACQ|LOCKREC|FREERUN|HOLDOVER|...).\n"
      "  get_statechg_sticky <chan>\n"
      "      Print 0/1 indicating whether the channel state changed since last "
      "clear.\n"
      "  clear_statechg_sticky <chan>\n"
      "      Clear the channel state-change sticky bit.\n"
      "  set_oper_state <chan> <NORMAL|FREERUN|HOLDOVER>\n"
      "      Force channel operating state (used to retrigger lock "
      "acquisition).\n"
      "  get_phase <chan>\n"
      "      Print signed phase measurement for <chan> in seconds (single "
      "float).\n"
      "\n"
      "High-level DPLL control (used by boot-time Python config):\n"
      "  set-input-freq <input> <freq_hz>   (or --set-input-freq)\n"
      "      Configure logical DPLL input index (1..4) nominal frequency in "
      "Hz.\n"
      "  set-input-enable <input> <enable|disable>   (or --set-input-enable)\n"
      "      Enable/disable a logical DPLL input (1..4).\n"
      "  set-chan-input <chan> <input> <priority> <enable|disable>   (or "
      "--set-chan-input)\n"
      "      Configure a DPLL channel's use of a given input:\n"
      "          chan    : DPLL channel index (e.g. 2, 5, 6)\n"
      "          input   : logical input index (1..4)\n"
      "          priority: integer, 1 = highest\n"
      "          state   : enable or disable\n"
      "  set-output-freq <freq3_hz> <freq4_hz>   (or --set-output-freq)\n"
      "      Configure logical outputs 3 and 4 simultaneously (they share a "
      "source/divider).\n"
      "      Each <freq*_hz> may be integer or floating-point (e.g. 10e6, "
      "10.000001e6).\n"
      "  set-output-divider <output_idx> <divider>   (or "
      "--set-output-divider)\n"
      "      Set integer output divider for specific output index.\n"
      "  set-combo-slave <chan> <master_chan> <enable|disable>   (or "
      "--set-combo-slave)\n"
      "      Configure combo bus slave settings (e.g. Ch6 slaves to Ch5).\n"
      "\n"
      "Connection options:\n"
      "  --spidev <path>         SPI node (e.g. /dev/spidev2.1). Overrides "
      "bus/cs.\n"
      "  --busnum <n>            SPI bus number -> /dev/spidev<n>.<csnum> if "
      "--spidev not used.\n"
      "  --csnum <m>             SPI chip-select -> /dev/spidev<busnum>.<m> if "
      "--spidev not used.\n"
      "  --hz <freq>             SPI clock (Hz), default 1000000.\n"
      "  --mode <0..3>           SPI mode, default 0.\n"
      "\n"
      "Debug options:\n"
      "  --tcs-debug             Make TCS/TXT parsers verbose (print "
      "parsed/written registers).\n"
      "\n"
      "Examples:\n"
      "  %s --read  0xC024\n"
      "  %s --spidev /dev/spidev2.1 --write 0xCBE4 0x50\n"
      "  %s --flash-hex SwitchberryV5_8a34004_eeprom.hex\n"
      "  %s --tcs-apply SwitchberryV5_8a34004_live.tcs --tcs-debug\n"
      "  %s --prog-file Test_FromScratch_Programming_11-26-2025.txt "
      "--tcs-debug\n"
      "  %s set-input-freq 1 25000000\n",
      prog, prog, prog, prog, prog, prog);
}

static int parse_u32(const char *s, uint32_t *out) {
  if (!s || !out)
    return -1;
  char *end = NULL;
  unsigned long v = strtoul(s, &end, 0);
  if (end == s || *end != '\0')
    return -1;
  *out = (uint32_t)v;
  return 0;
}

static int parse_u16(const char *s, uint16_t *out) {
  uint32_t tmp;
  if (parse_u32(s, &tmp) != 0 || tmp > 0xFFFF)
    return -1;
  *out = (uint16_t)tmp;
  return 0;
}

static int parse_u8(const char *s, uint8_t *out) {
  uint32_t tmp;
  if (parse_u32(s, &tmp) != 0 || tmp > 0xFF)
    return -1;
  *out = (uint8_t)tmp;
  return 0;
}

static int parse_s32(const char *s, int32_t *out) {
  if (!s || !out)
    return -1;
  char *end = NULL;
  long v = strtol(s, &end, 0);
  if (end == s || *end != '\0')
    return -1;
  if (v < INT32_MIN || v > INT32_MAX)
    return -1;
  *out = (int32_t)v;
  return 0;
}

static int parse_s64(const char *s, int64_t *out) {
  if (!s || !out)
    return -1;
  char *end = NULL;
  long long v = strtoll(s, &end, 0);
  if (end == s || *end != '\0')
    return -1;
  *out = (int64_t)v;
  return 0;
}

static int parse_double(const char *s, double *out) {
  if (!s || !out)
    return -1;
  char *end = NULL;
  double v = strtod(s, &end);
  if (end == s || *end != '\0') {
    return -1; /* no valid parse or trailing junk */
  }
  *out = v;
  return 0;
}

static double secs_since(struct timespec a, struct timespec b) {
  return (double)(a.tv_sec - b.tv_sec) + (double)(a.tv_nsec - b.tv_nsec) / 1e9;
}

static void fmt_hms(double s, char out[16]) {
  if (s < 0)
    s = 0;
  unsigned int total = (unsigned int)(s + 0.5);
  unsigned int h = total / 3600;
  unsigned int m = (total % 3600) / 60;
  unsigned int sec = total % 60;
  snprintf(out, 16, "%02u:%02u:%02u", h, m, sec);
}

/* Progress callback for EEPROM flashing */
static void flash_progress_cb(size_t written, size_t total, void *user) {
  progress_ctx_t *ctx = (progress_ctx_t *)user;
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);

  if (secs_since(now, ctx->last_print) < 0.10 && written != total)
    return;
  ctx->last_print = now;

  double elapsed = secs_since(now, ctx->start);
  double rate = (elapsed > 0) ? (double)written / elapsed : 0.0; // bytes/s

  char ebuf[16], tbuf[16];
  fmt_hms(elapsed, ebuf);

  if (total > 0) {
    double eta = (rate > 0) ? ((double)total - (double)written) / rate : 0.0;
    fmt_hms(eta, tbuf);

    double pct = 100.0 * (double)written / (double)total;

    fprintf(stderr, "\rFlashed %zu / %zu (%.1f%%) | elapsed %s | ETA %s    ",
            written, total, pct, ebuf, tbuf);
  } else {
    /* Unknown total: show bytes + rate only */
    double kbps = (rate > 0) ? rate / 1024.0 : 0.0;
    fprintf(stderr, "\rFlashed %zu bytes | elapsed %s | rate %.1f KiB/s    ",
            written, ebuf, kbps);
  }

  fflush(stderr);
  ctx->last_bytes = written;

  if (total > 0 && written >= total)
    fprintf(stderr, "\n");
}

/* -------------------------------------------------------------------------- */
/* High-level command stubs (fill in register-level code using g_cm_bus)      */
/* -------------------------------------------------------------------------- */

/*
 * Set nominal frequency for logical DPLL input index (1..4).
 * Implement the actual register programming using g_cm_bus when ready.
 */
static int dpll_cmd_set_input_freq(uint8_t input_idx, double freq_hz) {
  fprintf(stderr, "dpll_cmd_set_input_freq: input=%u freq=%.6f Hz\n", input_idx,
          freq_hz);

  double target = (double)freq_hz;
  uint64_t M = 0;
  uint16_t N_reg = 0;
  double actual = 0.0;
  double err_hz = 0.0;

  int rc = dpll_compute_input_ratio(target, &M, &N_reg, &actual, &err_hz);
  if (rc != 0) {
    fprintf(stderr, "dpll_compute_input_ratio failed for %.6f Hz (rc=%d)\n",
            target, rc);
    return rc;
  }

  double err_ppm = (target != 0.0) ? (err_hz / target) * 1e6 : 0.0;

  fprintf(stderr,
          "Input %u: target %.6f Hz, realized %.6f Hz "
          "(M=%llu, N_reg=%u) error %+g Hz (%.3f ppm)\n",
          (unsigned)input_idx, target, actual, (unsigned long long)M,
          (unsigned)N_reg, err_hz, err_ppm);

  /* Here you would:
   *   - break M (48-bit) and N_reg (16-bit) into the appropriate bytes
   *   - write them to the Input module registers using cm_string_write_bytes()
   *
   * Example for the "Input" module instance mapped from input_idx:
   */

  /* Pack M as 6 bytes LSB-first: M[0..7],8..15,...,40..47 */
  uint8_t buf_M[6];
  buf_M[0] = (uint8_t)(M & 0xFF);
  buf_M[1] = (uint8_t)((M >> 8) & 0xFF);
  buf_M[2] = (uint8_t)((M >> 16) & 0xFF);
  buf_M[3] = (uint8_t)((M >> 24) & 0xFF);
  buf_M[4] = (uint8_t)((M >> 32) & 0xFF);
  buf_M[5] = (uint8_t)((M >> 40) & 0xFF);

  rc = cm_string_write_bytes(&g_cm_bus, "Input", input_idx,
                             "INPUT_IN_FREQ_M_0_7", buf_M, sizeof(buf_M));
  if (rc) {
    fprintf(stderr, "Failed to write M for Input[%u], rc=%d\n", input_idx, rc);
    return rc;
  }

  /* Pack N as 2 bytes LSB-first: N[0..7], 8..15, stored in N_reg. */
  uint16_t N = N_reg; // N_reg is already encoded (0 means 1), but if N!=1 you
                      // just write it.
  uint8_t buf_N[2];
  buf_N[0] = (uint8_t)(N & 0xFF);
  buf_N[1] = (uint8_t)((N >> 8) & 0xFF);

  rc = cm_string_write_bytes(&g_cm_bus, "Input", input_idx,
                             "INPUT_IN_FREQ_N_0_7", buf_N, sizeof(buf_N));
  if (rc) {
    fprintf(stderr, "Failed to write N for Input[%u], rc=%d\n", input_idx, rc);
    return rc;
  }

  /* do trigger register access */
  cm_string_trigger_rw(&g_cm_bus, "Input", input_idx, "INPUT_IN_MODE");

  return 0;

  return 0; /* return non-zero on failure once implemented */
}

/*
 * Enable or disable a logical DPLL input (1..4).
 */
static int dpll_cmd_set_input_enable(uint8_t input_idx, int enable) {
  fprintf(stderr, "dpll_cmd_set_input_enable: input=%u state=%s \n", input_idx,
          enable ? "enable" : "disable");

  /* TODO:
   *   - Use g_cm_bus and cm_field_write8 on the appropriate INPUT_IN_MODE
   *     (or similar) register's IN_EN (or equivalent) bit.
   *   - Map input_idx -> Input instance index / field.
   */
  (void)input_idx;
  (void)enable;

  cm_string_field_write8(&g_cm_bus,
                         "Input",         // module name from tables
                         input_idx,       // instance
                         "INPUT_IN_MODE", // register name
                         "IN_EN",         // field name
                         enable);
  /* do trigger register access */
  cm_string_trigger_rw(&g_cm_bus, "Input", input_idx, "INPUT_IN_MODE");
  return 0;
}

/*
 * Configure a DPLL channel's use of a particular input:
 *   - chan     : DPLL channel index (e.g. 2, 5, 6)
 *   - input_idx: logical input index (1..4)
 *   - priority : 1 = highest
 *   - enable   : whether this chan should use this input in its priority list
 */
static int dpll_cmd_set_chan_input(uint8_t chan, uint8_t input_idx,
                                   uint8_t priority, int enable) {
  fprintf(stderr,
          "dpll_cmd_set_chan_input: chan=%u input=%u prio=%u state=%s\n ", chan,
          input_idx, priority, enable ? "enable" : "disable");

  char reg_name[32];
  sprintf(reg_name, "DPLL_REF_PRIORITY_%d", priority);

  /* Now need to edit DPLL input settings */
  cm_string_field_write8(&g_cm_bus,
                         "DPLL_Config",  // module name from tables
                         chan,           // instance
                         reg_name,       // register name
                         "PRIORITY_REF", // field name
                         input_idx);

  fprintf(stderr, "writing priority enable\n");
  cm_string_field_write8(&g_cm_bus,
                         "DPLL_Config", // module name from tables
                         chan,          // instance
                         reg_name,      // register name
                         "PRIORITY_EN", // field name
                         enable);

  /* do trigger register access */
  cm_string_trigger_rw(&g_cm_bus, "DPLL_Config", chan, "DPLL_MODE");
  return 0;
}

/*
 * Configure logical DPLL output index (e.g. 3,4) to a given frequency in Hz.
 * For your board, output 2 is usually 1PPS, dont allow user to change; outputs
 * 3 & 4 may be arbitrary freq.
 */
static int dpll_cmd_set_output_freq(double freq3_hz, double freq4_hz) {
  fprintf(stderr,
          "dpll_cmd_set_output_freq: request OUT3=%.6f Hz, OUT4=%.6f Hz\n",
          freq3_hz, freq4_hz);

  uint64_t M = 0;
  uint16_t N_reg = 0;
  uint32_t D3 = 0, D4 = 0;
  double fdco = 0.0;
  double out3_actual = 0.0, out4_actual = 0.0;
  double out3_err = 0.0, out4_err = 0.0;

  int rc = dpll_compute_output_mndiv(freq3_hz, freq4_hz, &M, &N_reg, &D3, &D4,
                                     &fdco, &out3_actual, &out4_actual,
                                     &out3_err, &out4_err);
  if (rc != 0) {
    fprintf(stderr, "dpll_compute_output_mndiv failed (rc=%d)\n", rc);
    return rc;
  }

  double e3_ppm = (freq3_hz != 0.0) ? (out3_err / freq3_hz) * 1e6 : 0.0;
  double e4_ppm = (freq4_hz != 0.0) ? (out4_err / freq4_hz) * 1e6 : 0.0;

  fprintf(stderr,
          "DCO: M=%llu, N_reg=%u => F_dco=%.6f Hz (%.3f MHz)\n"
          "  OUT3: divider=%u, actual=%.9f Hz, error=%+g Hz (%.3f ppm)\n"
          "  OUT4: divider=%u, actual=%.9f Hz, error=%+g Hz (%.3f ppm)\n",
          (unsigned long long)M, (unsigned)N_reg, fdco, fdco / 1e6, D3,
          out3_actual, out3_err, e3_ppm, D4, out4_actual, out4_err, e4_ppm);

  // pack M into 6 bytes LSB-first
  uint8_t m_bytes[6] = {
      (uint8_t)(M & 0xFF),         (uint8_t)((M >> 8) & 0xFF),
      (uint8_t)((M >> 16) & 0xFF), (uint8_t)((M >> 24) & 0xFF),
      (uint8_t)((M >> 32) & 0xFF), (uint8_t)((M >> 40) & 0xFF),
  };
  cm_string_write_bytes(&g_cm_bus, "DPLL_Ctrl", 6, "FOD_FREQ_M_7_0", m_bytes,
                        6);

  // N_reg (0..65535)
  uint8_t n_bytes[2] = {
      (uint8_t)(N_reg & 0xFF),
      (uint8_t)((N_reg >> 8) & 0xFF),
  };
  cm_string_write_bytes(&g_cm_bus, "DPLL_Ctrl", 6, "FOD_FREQ_N_7_0", n_bytes,
                        2);

  // don't need trigger, DPLL_Ctrl, every register is trigger register!

  // OUT3 divider D3 as 32-bit LSB-first
  uint8_t d3_bytes[4] = {
      (uint8_t)(D3 & 0xFF),
      (uint8_t)((D3 >> 8) & 0xFF),
      (uint8_t)((D3 >> 16) & 0xFF),
      (uint8_t)((D3 >> 24) & 0xFF),
  };
  cm_string_write_bytes(&g_cm_bus, "Output", 10, "OUT_DIV", d3_bytes, 4);

  // OUT4 divider D4 as 32-bit LSB-first
  uint8_t d4_bytes[4] = {
      (uint8_t)(D4 & 0xFF),
      (uint8_t)((D4 >> 8) & 0xFF),
      (uint8_t)((D4 >> 16) & 0xFF),
      (uint8_t)((D4 >> 24) & 0xFF),
  };
  cm_string_write_bytes(&g_cm_bus, "Output", 11, "OUT_DIV", d4_bytes, 4);

  // don't need trigger, OUTPUT, every register is trigger register!

  return 0;
}

/*
 * Set the integer divider for a specific output index.
 * Wrapper for OUT_DIV register.
 */
static int dpll_cmd_set_output_divider(uint8_t out_idx, uint32_t divider) {
  fprintf(stderr, "dpll_cmd_set_output_divider: output=%u divider=%u\n",
          out_idx, divider);

  // OUT_DIV is a 32-bit register. Pack LSB first.
  uint8_t div_bytes[4];
  div_bytes[0] = (uint8_t)(divider & 0xFF);
  div_bytes[1] = (uint8_t)((divider >> 8) & 0xFF);
  div_bytes[2] = (uint8_t)((divider >> 16) & 0xFF);
  div_bytes[3] = (uint8_t)((divider >> 24) & 0xFF);

  // Write to Output module, instance = out_idx
  int rc = cm_string_write_bytes(&g_cm_bus, "Output", out_idx, "OUT_DIV",
                                 div_bytes, 4);
  if (rc) {
    fprintf(stderr, "Failed to write OUT_DIV for Output[%u], rc=%d\n", out_idx,
            rc);
    return rc;
  }

  // No explicit trigger needed for Output module (usually instant or updated on
  // next edge)
  return 0;
}

/*
 * Configure the Combo Bus Slave settings for a DPLL channel.
 * Sets the Primary Combo Source ID and Enable bit.
 * Ref: Register DPLL_COMBO_SLAVE_CFG_0
 */
static int dpll_cmd_set_combo_slave(uint8_t chan, uint8_t master_chan,
                                    int enable) {
  fprintf(stderr, "dpll_cmd_set_combo_slave: chan=%u master=%u enable=%d\n",
          chan, master_chan, enable);

  // Filtered/Unfiltered config: Default to 0 (Unfiltered/Direct?)
  // The register field PRI_COMBO_SRC_FILTERED_CNFG (bit 4)
  // We'll leave it 0 for now as verified in hardware testing often works with
  // default. However, user only asked to set primary combo bus master.

  // 1. Set Source ID
  int rc = cm_string_field_write8(&g_cm_bus, "DPLL_Config", chan,
                                  "DPLL_COMBO_SLAVE_CFG_0", "PRI_COMBO_SRC_ID",
                                  master_chan);
  if (rc) {
    fprintf(stderr, "Failed to set PRI_COMBO_SRC_ID, rc=%d\n", rc);
    return rc;
  }

  // 2. Set Enable
  rc = cm_string_field_write8(&g_cm_bus, "DPLL_Config", chan,
                              "DPLL_COMBO_SLAVE_CFG_0", "PRI_COMBO_SRC_EN",
                              enable ? 1 : 0);
  if (rc) {
    fprintf(stderr, "Failed to set PRI_COMBO_SRC_EN, rc=%d\n", rc);
    return rc;
  }

  // 3. Trigger Update via DPLL_MODE
  fprintf(stderr, "Triggering update via DPLL_MODE...\n");
  rc = cm_string_trigger_rw(&g_cm_bus, "DPLL_Config", chan, "DPLL_MODE");
  if (rc) {
    fprintf(stderr, "Failed to trigger DPLL_MODE, rc=%d\n", rc);
    return rc;
  }

  return 0;
}

typedef enum {
  DPLL_LOCK_STATE_UNKNOWN = 7,
  DPLL_LOCK_STATE_LOCKED = 3,
  DPLL_LOCK_STATE_LOCKACQ = 1,
  DPLL_LOCK_STATE_LOCKREC = 2,
  DPLL_LOCK_STATE_FREERUN = 0,
  DPLL_LOCK_STATE_HOLDOVER = 4,
  DPLL_LOCK_STATE_DISABLED = 6,
} dpll_lock_state_t;

typedef enum {
  DPLL_OPER_STATE_NORMAL = 0,
  DPLL_OPER_STATE_FREERUN = 2,
  DPLL_OPER_STATE_HOLDOVER = 3,
} dpll_oper_state_t;

/*
 * Lower-level API hooks (you said you'll implement the register access).
 *
 * These should:
 *  - Read the DPLL channel's current state (LOCKED/LOCKACQ/LOCKREC/etc)
 *  - Read/clear a sticky "state changed" bit for the channel
 *  - Set the channel operating state (NORMAL/FREERUN/HOLDOVER)
 */
static int dpll_ll_get_lock_state(cm_bus_t *bus, unsigned chan,
                                  dpll_lock_state_t *out_state);
static int dpll_ll_get_statechg_sticky(cm_bus_t *bus, unsigned chan,
                                       uint8_t *out_sticky);
static int dpll_ll_clear_statechg_sticky(cm_bus_t *bus, unsigned chan);
static int dpll_ll_set_oper_state(cm_bus_t *bus, unsigned chan,
                                  dpll_oper_state_t state);

static const char *dpll_lock_state_to_str(dpll_lock_state_t st) {
  switch (st) {
  case DPLL_LOCK_STATE_LOCKED:
    return "LOCKED";
  case DPLL_LOCK_STATE_LOCKACQ:
    return "LOCKACQ";
  case DPLL_LOCK_STATE_LOCKREC:
    return "LOCKREC";
  case DPLL_LOCK_STATE_FREERUN:
    return "FREERUN";
  case DPLL_LOCK_STATE_HOLDOVER:
    return "HOLDOVER";
  case DPLL_LOCK_STATE_DISABLED:
    return "DISABLED";
  default:
    return "UNKNOWN";
  }
}

static int dpll_parse_oper_state(const char *s, dpll_oper_state_t *out) {
  if (!s || !out)
    return -1;
  if (!strcmp(s, "NORMAL")) {
    *out = DPLL_OPER_STATE_NORMAL;
    return 0;
  }
  if (!strcmp(s, "FREERUN")) {
    *out = DPLL_OPER_STATE_FREERUN;
    return 0;
  }
  if (!strcmp(s, "HOLDOVER")) {
    *out = DPLL_OPER_STATE_HOLDOVER;
    return 0;
  }
  return -1;
}

static int dpll_cmd_get_state(uint8_t chan) {
  dpll_lock_state_t st = DPLL_LOCK_STATE_UNKNOWN;
  int rc = dpll_ll_get_lock_state(&g_cm_bus, (unsigned)chan, &st);
  if (rc != 0) {
    fprintf(stderr, "get_state failed (chan=%u, rc=%d)\n", (unsigned)chan, rc);
    return rc;
  }
  /* Script-friendly: single token */
  printf("%s\n", dpll_lock_state_to_str(st));
  return 0;
}

static int dpll_cmd_get_statechg_sticky(uint8_t chan) {
  uint8_t sticky = 0;
  int rc = dpll_ll_get_statechg_sticky(&g_cm_bus, (unsigned)chan, &sticky);
  if (rc != 0) {
    fprintf(stderr, "get_statechg_sticky failed (chan=%u, rc=%d)\n",
            (unsigned)chan, rc);
    return rc;
  }
  printf("%u\n", (unsigned)(sticky ? 1 : 0));
  return 0;
}

static int dpll_cmd_clear_statechg_sticky(uint8_t chan) {
  int rc = dpll_ll_clear_statechg_sticky(&g_cm_bus, (unsigned)chan);
  if (rc != 0) {
    fprintf(stderr, "clear_statechg_sticky failed (chan=%u, rc=%d)\n",
            (unsigned)chan, rc);
    return rc;
  }
  return 0;
}

static int dpll_cmd_set_oper_state(uint8_t chan, const char *state_str) {
  dpll_oper_state_t st;
  if (dpll_parse_oper_state(state_str, &st) != 0) {
    fprintf(
        stderr,
        "set_oper_state: bad state '%s' (expected NORMAL|FREERUN|HOLDOVER)\n",
        state_str ? state_str : "(null)");
    return -1;
  }
  int rc = dpll_ll_set_oper_state(&g_cm_bus, (unsigned)chan, st);
  if (rc != 0) {
    fprintf(stderr, "set_oper_state failed (chan=%u, state=%s, rc=%d)\n",
            (unsigned)chan, state_str, rc);
    return rc;
  }
  return 0;
}

static int dpll_cmd_get_phase(uint8_t chan) {
  int64_t phase_s36 = 0;
  int rc = cm_read_phase_status_s36((const cm_bus_t *)&g_cm_bus, (unsigned)chan,
                                    &phase_s36);
  if (rc != 0) {
    fprintf(stderr, "get_phase failed (chan=%u, rc=%d)\n", (unsigned)chan, rc);
    return rc;
  }
  double secs = cm_phase_s36_to_seconds(phase_s36);
  /* Script-friendly: single float in seconds */
  printf("%.12e\n", secs);
  return 0;
}

/* -------------------------------------------------------------------------- */

int main(int argc, char **argv) {
  if (argc < 2) {
    usage(argv[0]);
    return 1;
  }

  int do_read = 0;
  int do_write = 0;
  int do_flash = 0;
  int do_tcs_apply = 0;
  int do_set_input_freq = 0;
  int do_set_input_enable = 0;
  int do_set_chan_input = 0;
  int do_set_output_freq = 0;
  int do_set_out2_dest = 0;
  int do_prog_file = 0;

  int do_set_output_divider = 0;
  int do_set_combo_slave = 0;

  /* Monitor/daemon helper commands */
  int do_get_state = 0;
  int do_get_statechg_sticky = 0;
  int do_clear_statechg_sticky = 0;
  int do_set_oper_state = 0;
  int do_get_phase = 0;

  int do_out_phase_adj_get = 0;
  int do_out_phase_adj_set = 0;
  int do_wr_freq_get = 0;
  int do_wr_freq_set_word = 0;
  int do_wr_freq_set_ppb = 0;

  uint16_t addr = 0;
  uint8_t wdata = 0;
  const char *hex_path = NULL;
  const char *tcs_path = NULL;
  const char *prog_path = NULL;

  /* High-level command parameters */
  uint8_t hl_input_idx = 0;
  uint8_t hl_chan = 0;
  uint8_t hl_input_for_chan = 0;
  uint8_t hl_priority = 0;
  double hl_freq_hz = 0;
  double hl_out3_hz = 0.0; /* used for set-output-freq */
  double hl_out4_hz = 0.0; /* used for set-output-freq */
  int hl_enable_flag = 0;

  uint32_t hl_divider = 0;
  uint8_t hl_master_chan = 0;

  /* Monitor helper parameters */
  uint8_t mon_chan = 0;
  const char *mon_oper_state = NULL;

  /* One-shot debug helpers */
  uint32_t hl_out_idx = 0;
  int32_t hl_phase_adj = 0;
  uint32_t hl_dpll_idx = 0;
  int64_t hl_wr_word_s42 = 0;
  double hl_wr_ppb = 0.0;

  /* Defaults */
  char spidev_path[64];
  snprintf(spidev_path, sizeof(spidev_path), "/dev/spidev7.0");

  uint32_t hz = 1000000; /* 1 MHz */
  int mode = 0;          /* SPI_MODE_0 */

  /* Optional bus/cs mapping -> /dev/spidev<busnum>.<csnum> */
  unsigned int busnum = 7;
  unsigned int csnum = 0;
  int have_busnum = 0;
  int have_csnum = 0;
  int spidev_overridden = 0;

  int tcs_debug = 0;

  /* Parse args */
  for (int i = 1; i < argc; i++) {

    /* Low-level: read/write/flash/tcs --------------------------------- */
    if (!strcmp(argv[i], "--read") && i + 1 < argc) {
      if (parse_u16(argv[++i], &addr) != 0) {
        fprintf(stderr, "Bad --read addr\n");
        return 1;
      }
      do_read = 1;

    } else if (!strcmp(argv[i], "--write") && i + 2 < argc) {
      if (parse_u16(argv[++i], &addr) != 0) {
        fprintf(stderr, "Bad --write addr\n");
        return 1;
      }
      if (parse_u8(argv[++i], &wdata) != 0) {
        fprintf(stderr, "Bad --write data\n");
        return 1;
      }
      do_write = 1;

    } else if (!strcmp(argv[i], "--flash-hex") && i + 1 < argc) {
      hex_path = argv[++i];
      do_flash = 1;

    } else if (!strcmp(argv[i], "--tcs-apply") && i + 1 < argc) {
      tcs_path = argv[++i];
      do_tcs_apply = 1;

      /* Monitor/daemon helper commands (accept with or without leading --) */
    } else if ((!strcmp(argv[i], "get_state") ||
                !strcmp(argv[i], "--get_state") ||
                !strcmp(argv[i], "get-state") ||
                !strcmp(argv[i], "--get-state")) &&
               i + 1 < argc) {
      uint32_t tmp;
      if (parse_u32(argv[++i], &tmp) != 0 || tmp > 255) {
        fprintf(stderr, "Bad get_state <chan>\n");
        return 1;
      }
      mon_chan = (uint8_t)tmp;
      do_get_state = 1;

    } else if ((!strcmp(argv[i], "get_statechg_sticky") ||
                !strcmp(argv[i], "--get_statechg_sticky") ||
                !strcmp(argv[i], "get-statechg-sticky") ||
                !strcmp(argv[i], "--get-statechg-sticky")) &&
               i + 1 < argc) {
      uint32_t tmp;
      if (parse_u32(argv[++i], &tmp) != 0 || tmp > 255) {
        fprintf(stderr, "Bad get_statechg_sticky <chan>\n");
        return 1;
      }
      mon_chan = (uint8_t)tmp;
      do_get_statechg_sticky = 1;

    } else if ((!strcmp(argv[i], "clear_statechg_sticky") ||
                !strcmp(argv[i], "--clear_statechg_sticky") ||
                !strcmp(argv[i], "clear-statechg-sticky") ||
                !strcmp(argv[i], "--clear-statechg-sticky")) &&
               i + 1 < argc) {
      uint32_t tmp;
      if (parse_u32(argv[++i], &tmp) != 0 || tmp > 255) {
        fprintf(stderr, "Bad clear_statechg_sticky <chan>\n");
        return 1;
      }
      mon_chan = (uint8_t)tmp;
      do_clear_statechg_sticky = 1;

    } else if ((!strcmp(argv[i], "set_oper_state") ||
                !strcmp(argv[i], "--set_oper_state") ||
                !strcmp(argv[i], "set-oper-state") ||
                !strcmp(argv[i], "--set-oper-state")) &&
               i + 2 < argc) {
      uint32_t tmp;
      if (parse_u32(argv[++i], &tmp) != 0 || tmp > 255) {
        fprintf(stderr, "Bad set_oper_state <chan>\n");
        return 1;
      }
      mon_chan = (uint8_t)tmp;
      mon_oper_state = argv[++i];
      do_set_oper_state = 1;

    } else if ((!strcmp(argv[i], "get_phase") ||
                !strcmp(argv[i], "--get_phase") ||
                !strcmp(argv[i], "get-phase") ||
                !strcmp(argv[i], "--get-phase")) &&
               i + 1 < argc) {
      uint32_t tmp;
      if (parse_u32(argv[++i], &tmp) != 0 || tmp > 255) {
        fprintf(stderr, "Bad get_phase <chan>\n");
        return 1;
      }
      mon_chan = (uint8_t)tmp;
      do_get_phase = 1;

      /* High-level commands (accept with or without leading --) --------- */
    } else if ((!strcmp(argv[i], "set-input-freq") ||
                !strcmp(argv[i], "--set-input-freq")) &&
               i + 2 < argc) {

      uint32_t tmp;
      if (parse_u32(argv[++i], &tmp) != 0) {
        fprintf(stderr, "Bad set-input-freq  index\n");
        return 1;
      }
      hl_input_idx = (uint8_t)tmp;

      double ftmp = 0.0;
      if (parse_double(argv[++i], &ftmp) != 0 || ftmp <= 0.0) {
        fprintf(stderr,
                "Bad set-input-freq freq_hz (must be > 0, can be float)\n");
        return 1;
      }
      hl_freq_hz = ftmp;

      do_set_input_freq = 1;

    } else if ((!strcmp(argv[i], "set-input-enable") ||
                !strcmp(argv[i], "--set-input-enable")) &&
               i + 2 < argc) {
      uint32_t tmp;
      if (parse_u32(argv[++i], &tmp) != 0) {
        fprintf(stderr, "Bad set-input-enable input index \n");
        return 1;
      }
      hl_input_idx = (uint8_t)tmp;
      const char *st = argv[++i];
      if (!strcmp(st, "enable")) {
        hl_enable_flag = 1;
      } else if (!strcmp(st, "disable")) {
        hl_enable_flag = 0;
      } else {
        fprintf(stderr,
                "Bad set-input-enable state (must be enable|disable)\n");
        return 1;
      }
      do_set_input_enable = 1;

    } else if ((!strcmp(argv[i], "set-chan-input") ||
                !strcmp(argv[i], "--set-chan-input")) &&
               i + 4 < argc) {
      uint32_t tmp;
      if (parse_u32(argv[++i], &tmp) != 0 || tmp > 255) {
        fprintf(stderr, "Bad set-chan-input chan\n");
        return 1;
      }
      hl_chan = (uint8_t)tmp;
      if (parse_u32(argv[++i], &tmp) != 0) {
        fprintf(stderr, "Bad set-chan-input input index\n");
        return 1;
      }
      hl_input_for_chan = (uint8_t)tmp;
      if (parse_u32(argv[++i], &tmp) != 0 || tmp > 18) {
        fprintf(stderr, "Bad set-chan-input priority\n");
        return 1;
      }
      hl_priority = (uint8_t)tmp;
      const char *st = argv[++i];
      if (!strcmp(st, "enable")) {
        hl_enable_flag = 1;
      } else if (!strcmp(st, "disable")) {
        hl_enable_flag = 0;
      } else {
        fprintf(stderr, "Bad set-chan-input state (must be enable|disable)\n");
        return 1;
      }
      do_set_chan_input = 1;

    } else if ((!strcmp(argv[i], "set-output-freq") ||
                !strcmp(argv[i], "--set-output-freq")) &&
               i + 2 < argc) {
      // specific utility for switchberry, could be adapted later for more
      // generic

      double f3 = 0.0;
      double f4 = 0.0;

      if (parse_double(argv[++i], &f3) != 0 || f3 <= 0.0) {
        fprintf(stderr,
                "Bad set-output-freq freq3_hz (must be > 0, can be float)\n");
        return 1;
      }
      if (parse_double(argv[++i], &f4) != 0 || f4 <= 0.0) {
        fprintf(stderr,
                "Bad set-output-freq freq4_hz (must be > 0, can be float)\n");
        return 1;
      }

      hl_out3_hz = f3;
      hl_out4_hz = f4;

      do_set_output_freq = 1;

    } else if ((!strcmp(argv[i], "set-output-divider") ||
                !strcmp(argv[i], "--set-output-divider")) &&
               i + 2 < argc) {
      if (parse_u32(argv[++i], &hl_out_idx) != 0) {
        fprintf(stderr, "Bad set-output-divider <out_idx>\n");
        return 1;
      }
      if (parse_u32(argv[++i], &hl_divider) != 0) {
        fprintf(stderr, "Bad set-output-divider <divider>\n");
        return 1;
      }
      do_set_output_divider = 1;

    } else if ((!strcmp(argv[i], "set-combo-slave") ||
                !strcmp(argv[i], "--set-combo-slave")) &&
               i + 3 < argc) {
      uint32_t tmp_ch, tmp_master;
      if (parse_u32(argv[++i], &tmp_ch) != 0 || tmp_ch > 255) {
        fprintf(stderr, "Bad set-combo-slave <chan>\n");
        return 1;
      }
      hl_chan = (uint8_t)tmp_ch;

      if (parse_u32(argv[++i], &tmp_master) != 0 || tmp_master > 255) {
        fprintf(stderr, "Bad set-combo-slave <master_chan>\n");
        return 1;
      }
      hl_master_chan = (uint8_t)tmp_master;

      const char *st = argv[++i];
      if (!strcmp(st, "enable") || !strcmp(st, "1")) {
        hl_enable_flag = 1;
      } else if (!strcmp(st, "disable") || !strcmp(st, "0")) {
        hl_enable_flag = 0;
      } else {
        fprintf(stderr, "Bad set-combo-slave <enable|disable>\n");
        return 1;
      }
      do_set_combo_slave = 1;

    } else if ((!strcmp(argv[i], "out-phase-adj-get") ||
                !strcmp(argv[i], "--out-phase-adj-get")) &&
               i + 1 < argc) {
      if (parse_u32(argv[++i], &hl_out_idx) != 0) {
        fprintf(stderr, "Bad out-phase-adj-get <out>\n");
        return 1;
      }
      do_out_phase_adj_get = 1;

    } else if ((!strcmp(argv[i], "out-phase-adj-set") ||
                !strcmp(argv[i], "--out-phase-adj-set")) &&
               i + 2 < argc) {
      if (parse_u32(argv[++i], &hl_out_idx) != 0) {
        fprintf(stderr, "Bad out-phase-adj-set <out>\n");
        return 1;
      }
      if (parse_s32(argv[++i], &hl_phase_adj) != 0) {
        fprintf(stderr, "Bad out-phase-adj-set <s32_value>\n");
        return 1;
      }
      do_out_phase_adj_set = 1;

    } else if ((!strcmp(argv[i], "wr-freq-get") ||
                !strcmp(argv[i], "--wr-freq-get")) &&
               i + 1 < argc) {
      if (parse_u32(argv[++i], &hl_dpll_idx) != 0) {
        fprintf(stderr, "Bad wr-freq-get <dpll>\n");
        return 1;
      }
      do_wr_freq_get = 1;

    } else if ((!strcmp(argv[i], "wr-freq-set-word") ||
                !strcmp(argv[i], "--wr-freq-set-word")) &&
               i + 2 < argc) {
      if (parse_u32(argv[++i], &hl_dpll_idx) != 0) {
        fprintf(stderr, "Bad wr-freq-set-word <dpll>\n");
        return 1;
      }
      if (parse_s64(argv[++i], &hl_wr_word_s42) != 0) {
        fprintf(stderr, "Bad wr-freq-set-word <s42_word>\n");
        return 1;
      }
      do_wr_freq_set_word = 1;

    } else if ((!strcmp(argv[i], "wr-freq-set-ppb") ||
                !strcmp(argv[i], "--wr-freq-set-ppb")) &&
               i + 2 < argc) {
      if (parse_u32(argv[++i], &hl_dpll_idx) != 0) {
        fprintf(stderr, "Bad wr-freq-set-ppb <dpll>\n");
        return 1;
      }
      if (parse_double(argv[++i], &hl_wr_ppb) != 0) {
        fprintf(stderr, "Bad wr-freq-set-ppb <ppb_float>\n");
        return 1;
      }
      do_wr_freq_set_ppb = 1;
      /* Connection / debug options ------------------------------------- */
    } else if (!strcmp(argv[i], "--spidev") && i + 1 < argc) {
      const char *p = argv[++i];
      strncpy(spidev_path, p, sizeof(spidev_path) - 1);
      spidev_path[sizeof(spidev_path) - 1] = '\0';
      spidev_overridden = 1;

    } else if (!strcmp(argv[i], "--busnum") && i + 1 < argc) {
      busnum = (unsigned int)strtoul(argv[++i], NULL, 0);
      have_busnum = 1;

    } else if (!strcmp(argv[i], "--csnum") && i + 1 < argc) {
      csnum = (unsigned int)strtoul(argv[++i], NULL, 0);
      have_csnum = 1;

    } else if (!strcmp(argv[i], "--hz") && i + 1 < argc) {
      if (parse_u32(argv[++i], &hz) != 0) {
        fprintf(stderr, "Bad --hz\n");
        return 1;
      }

    } else if (!strcmp(argv[i], "--mode") && i + 1 < argc) {
      mode = atoi(argv[++i]);
      if (mode < 0 || mode > 3) {
        fprintf(stderr, "Bad --mode (0..3)\n");
        return 1;
      }

    } else if (!strcmp(argv[i], "--tcs-debug")) {
      tcs_debug = 1;
      /* arg parsing */
    } else if (!strcmp(argv[i], "--prog-file") && i + 1 < argc) {
      prog_path = argv[++i];
      do_prog_file = 1;
    } else {
      fprintf(stderr, "Unknown/invalid arg: %s\n", argv[i]);
      usage(argv[0]);
      return 1;
    }
  }

  int action_count =
      do_read + do_write + do_flash + do_tcs_apply + do_get_state +
      do_get_statechg_sticky + do_clear_statechg_sticky + do_set_oper_state +
      do_get_phase + do_set_input_freq + do_set_input_enable +
      do_set_chan_input + do_set_output_freq + do_set_out2_dest + do_prog_file +
      do_out_phase_adj_get + do_out_phase_adj_set + do_wr_freq_get +
      do_wr_freq_set_word + do_wr_freq_set_ppb + do_set_output_divider +
      do_set_combo_slave;

  if (action_count != 1) {
    fprintf(stderr, "Specify exactly one action: "
                    "--read, --write, --flash-hex, --tcs-apply,\n"
                    "  or monitor helpers get_state, get_statechg_sticky, "
                    "clear_statechg_sticky, set_oper_state, get_phase,\n"
                    "  or one of the high-level commands "
                    "set-input-freq, set-input-enable, set-chan-input,\n"
                    "  set-output-freq, set-output-divider, set-combo-slave\n");
    usage(argv[0]);
    return 1;
  }

  /* Build spidev path from bus/cs if user didn't explicitly give --spidev */
  if (!spidev_overridden && (have_busnum || have_csnum)) {
    snprintf(spidev_path, sizeof(spidev_path), "/dev/spidev%u.%u", busnum,
             csnum);
  }

  /* Open spidev */
  int spi_fd = dpll_spi_open(spidev_path, hz, (uint8_t)mode);
  if (spi_fd < 0) {
    fprintf(stderr, "Failed to open SPI device %s\n", spidev_path);
    return 2;
  }

  /*
fprintf(stderr, "Using spidev: %s (hz=%u, mode=%d)\n",
      spidev_path, hz, mode);
  */
  /* Initialize global cm_bus for table-driven access */
  cm_init_bus_for_spi(spi_fd);

  int rc = 0;

  if (do_read) {
    uint8_t val = 0;
    if (dpll_read8(spi_fd, addr, &val) != DPLL_OK) {
      fprintf(stderr, "Read from 0x%04X failed\n", addr);
      rc = 1;
    } else {
      printf("Read 0x%02X from 0x%04X\n", val, addr);
    }

  } else if (do_write) {
    if (dpll_write8(spi_fd, addr, wdata) != DPLL_OK) {
      fprintf(stderr, "Write to 0x%04X failed\n", addr);
      rc = 1;
    } else {
      printf("Wrote 0x%02X to 0x%04X\n", wdata, addr);
    }

  } else if (do_flash) {
    fprintf(stderr, "Flashing EEPROM HEX: %s\n", hex_path);

    progress_ctx_t ctx;
    clock_gettime(CLOCK_MONOTONIC, &ctx.start);
    ctx.last_print = ctx.start;
    ctx.last_bytes = 0;

    dpll_result_t r =
        dpll_eeprom_flash_hex(spi_fd, hex_path, flash_progress_cb, &ctx);
    if (r != DPLL_OK) {
      fprintf(stderr, "Flash failed.\n");
      rc = 1;
    } else {
      fprintf(stderr, "Flash complete.\n");
    }

  } else if (do_tcs_apply) {
    fprintf(stderr, "Applying TCS file: %s\n", tcs_path);
    dpll_result_t r = dpll_apply_tcs_file(spi_fd, tcs_path, tcs_debug ? 1 : 0);
    if (r != DPLL_OK) {
      fprintf(stderr, "TCS apply failed.\n");
      rc = 1;
    } else {
      fprintf(stderr, "TCS apply complete.\n");
    }

  } else if (do_get_state) {
    if (dpll_cmd_get_state(mon_chan) != 0) {
      rc = 1;
    }

  } else if (do_get_statechg_sticky) {
    if (dpll_cmd_get_statechg_sticky(mon_chan) != 0) {
      rc = 1;
    }

  } else if (do_clear_statechg_sticky) {
    if (dpll_cmd_clear_statechg_sticky(mon_chan) != 0) {
      rc = 1;
    }

  } else if (do_set_oper_state) {
    if (dpll_cmd_set_oper_state(mon_chan, mon_oper_state) != 0) {
      rc = 1;
    }

  } else if (do_get_phase) {
    if (dpll_cmd_get_phase(mon_chan) != 0) {
      rc = 1;
    }

  } else if (do_set_input_freq) {
    if (dpll_cmd_set_input_freq(hl_input_idx, hl_freq_hz) != 0) {
      fprintf(stderr, "set-input-freq failed.\n");
      rc = 1;
    }

  } else if (do_set_input_enable) {
    if (dpll_cmd_set_input_enable(hl_input_idx, hl_enable_flag) != 0) {
      fprintf(stderr, "set-input-enable failed.\n");
      rc = 1;
    }

  } else if (do_set_chan_input) {
    if (dpll_cmd_set_chan_input(hl_chan, hl_input_for_chan, hl_priority,
                                hl_enable_flag) != 0) {
      fprintf(stderr, "set-chan-input failed.\n");
      rc = 1;
    }

  } else if (do_set_output_freq) {

    if (dpll_cmd_set_output_freq(hl_out3_hz, hl_out4_hz) != 0) {
      fprintf(stderr, "set-output-freq failed.\n");
      rc = 1;
    }

  } else if (do_set_output_divider) {
    if (dpll_cmd_set_output_divider((uint8_t)hl_out_idx, hl_divider) != 0) {
      fprintf(stderr, "set-output-divider failed.\n");
      rc = 1;
    }

  } else if (do_set_combo_slave) {
    if (dpll_cmd_set_combo_slave(hl_chan, hl_master_chan, hl_enable_flag) !=
        0) {
      fprintf(stderr, "set-combo-slave failed.\n");
      rc = 1;
    }

  } else if (do_out_phase_adj_get) {
    int32_t adj = 0;
    int rr =
        cm_read_output_phase_adj_s32(&g_cm_bus, (unsigned)hl_out_idx, &adj);
    if (rr != 0) {
      fprintf(stderr, "out-phase-adj-get failed (out=%u), rc=%d\n", hl_out_idx,
              rr);
      rc = 1;
    } else {
      printf("Output[%u].OUT_PHASE_ADJ = %d (0x%08x)\n", hl_out_idx, adj,
             (uint32_t)adj);
    }

  } else if (do_out_phase_adj_set) {
    int rr = cm_write_output_phase_adj_s32(&g_cm_bus, (unsigned)hl_out_idx,
                                           hl_phase_adj, 1, 0);
    if (rr != 0) {
      fprintf(stderr, "out-phase-adj-set failed (out=%u, adj=%d), rc=%d\n",
              hl_out_idx, hl_phase_adj, rr);
      rc = 1;
    } else {
      printf("Wrote Output[%u].OUT_PHASE_ADJ = %d (0x%08x)\n", hl_out_idx,
             hl_phase_adj, (uint32_t)hl_phase_adj);
    }

  } else if (do_wr_freq_get) {
    int64_t word = 0;
    int rr =
        cm_read_dpll_wr_freq_s42(&g_cm_bus, (unsigned)hl_dpll_idx, &word, 1);
    if (rr != 0) {
      fprintf(stderr, "wr-freq-get failed (dpll=%u), rc=%d\n", hl_dpll_idx, rr);
      rc = 1;
    } else {
      double frac = ldexp((double)word, -CM_WR_FREQ_FRAC_BITS);
      double ppb = frac * 1e9;
      printf("DPLL_Freq_Write[%u].DPLL_WR_FREQ word_s42=%lld  (~%.9f ppb)\n",
             hl_dpll_idx, (long long)word, ppb);
    }

  } else if (do_wr_freq_set_word) {
    int rr = cm_write_dpll_wr_freq_s42(&g_cm_bus, (unsigned)hl_dpll_idx,
                                       hl_wr_word_s42, 1, 0);
    if (rr != 0) {
      fprintf(stderr, "wr-freq-set-word failed (dpll=%u, word=%lld), rc=%d\n",
              hl_dpll_idx, (long long)hl_wr_word_s42, rr);
      rc = 1;
    } else {
      printf("Wrote DPLL_Freq_Write[%u].DPLL_WR_FREQ word_s42=%lld\n",
             hl_dpll_idx, (long long)hl_wr_word_s42);
    }

  } else if (do_wr_freq_set_ppb) {
    double word_d = (hl_wr_ppb / 1e9) * (double)(1ULL << CM_WR_FREQ_FRAC_BITS);
    int64_t word = (int64_t)llround(word_d);
    int rr =
        cm_write_dpll_wr_freq_s42(&g_cm_bus, (unsigned)hl_dpll_idx, word, 1, 0);
    if (rr != 0) {
      fprintf(stderr,
              "wr-freq-set-ppb failed (dpll=%u, ppb=%.9f, word=%lld), rc=%d\n",
              hl_dpll_idx, hl_wr_ppb, (long long)word, rr);
      rc = 1;
    } else {
      printf(
          "Wrote DPLL_Freq_Write[%u].DPLL_WR_FREQ ~%.9f ppb (word_s42=%lld)\n",
          hl_dpll_idx, hl_wr_ppb, (long long)word);
    }

  } else if (do_prog_file) {
    fprintf(stderr, "Applying programming file: %s\n", prog_path);
    dpll_result_t r =
        dpll_apply_program_file(spi_fd, prog_path, 1); // always verbose
    // dpll_result_t r = dpll_apply_program_file(spi_fd, prog_path, tcs_debug ?
    // 1 : 0);
    if (r != DPLL_OK) {
      fprintf(stderr, "Programming file apply failed.\n");
      rc = 1;
    } else {
      fprintf(stderr, "Programming file apply complete.\n");
    }
  } else if (do_set_out2_dest) {
  }

  dpll_spi_close(spi_fd);
  return rc;
}

/* -------------------------------------------------------------------------- */
/* Register-level hook stubs                                                  */
/*                                                                            */
/* Replace these with real implementations that read/write the 8A34004        */
/* channel state + sticky bits + operating mode.                              */
/*                                                                            */
/* These are kept in dpll_utility.c (static) so the bash monitor can rely on  */
/* stable CLI behavior while you iterate on the register mappings.            */
/* -------------------------------------------------------------------------- */

static int dpll_ll_get_lock_state(cm_bus_t *bus, unsigned chan,
                                  dpll_lock_state_t *out_state) {
  char reg_name[64];
  snprintf(reg_name, sizeof(reg_name), "DPLL%d_STATUS", chan);

  uint8_t buf = 0;
  int rc =
      cm_string_field_read8(bus, "Status", 0, reg_name, "DPLL_STATE", &buf);
  // fprintf(stderr, "Reg %s, read 0x%x\r\n", reg_name, buf);
  if (rc != 0)
    return rc;
  *out_state = (dpll_lock_state_t)buf;
  return rc;
}

static int dpll_ll_get_statechg_sticky(cm_bus_t *bus, unsigned chan,
                                       uint8_t *out_sticky) {
  char reg_name[64];
  snprintf(reg_name, sizeof(reg_name), "DPLL%d_STATUS", chan);

  uint8_t buf = 0;
  int rc = cm_string_field_read8(bus, "Status", 0, reg_name,
                                 "LOCK_STATE_CHANGE_STICKY", &buf);
  if (rc != 0)
    return rc;
  *out_sticky = buf;
  return rc;
}

static int dpll_ll_clear_statechg_sticky(cm_bus_t *bus, unsigned chan) {
  // a bit hacky, don't feel like updating the table code for this one case
  cm_write8(bus, 0xc164 + 0x2, 1 << chan);
  return 0;
}

static int dpll_ll_set_oper_state(cm_bus_t *bus, unsigned chan,
                                  dpll_oper_state_t state) {
  fprintf(stderr, "set_oper_state dpll %d state %d\r\n", chan, state);
  int rc = cm_string_field_write8(bus, "DPLL_Config", chan, "DPLL_MODE",
                                  "STATE_MODE", (uint8_t)state);
  return rc;
}

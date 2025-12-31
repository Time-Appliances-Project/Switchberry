#include "tcs_dpll.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

/* ---------- Small helpers ---------- */

/* Trim leading and trailing whitespace (in-place) */
static char *trim(char *s)
{
    char *end;

    while (*s && isspace((unsigned char)*s)) s++;

    if (*s == 0) return s;

    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    return s;
}

/* Parse 1–2 hex digits into a uint8_t */
static int parse_hex_u8(const char *s, uint8_t *out)
{
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 16);
    if (end == s || v > 0xFF) return -1;
    *out = (uint8_t)v;
    return 0;
}

/* Parse exactly 2 hex digits into a uint8_t */
static int parse_hex2_u8(const char *s, uint8_t *out)
{
    char buf[3];
    buf[0] = s[0];
    buf[1] = s[1];
    buf[2] = '\0';
    return parse_hex_u8(buf, out);
}

/* Try to parse a register line of the form:
 *
 *   C0.0A                                00000000       00 C0.0A
 *
 * We only care about:
 *   - page hex (2 chars)
 *   - byte hex (2 chars)
 *   - HexValue (2 chars)
 *
 * Returns:
 *   1 on success (and fills page, byte, value),
 *   0 if the line is not a register line,
 *  -1 on hard parse error.
 */
static int parse_reg_line(const char *line, uint8_t *page, uint8_t *byte, uint8_t *val)
{
    const char *p = line;

    /* Skip leading spaces */
    while (*p && isspace((unsigned char)*p)) ++p;
    if (*p == '\0') return 0;

    /* Quick shape check: must start "XX.YY" where X/Y are hex digits */
    if (!isxdigit((unsigned char)p[0]) ||
        !isxdigit((unsigned char)p[1]) ||
         p[2] != '.' ||
        !isxdigit((unsigned char)p[3]) ||
        !isxdigit((unsigned char)p[4])) {
        return 0;   /* not a register line */
    }

    char page_str[3] = {0};
    char byte_str[3] = {0};
    char val_str[3]  = {0};

    /* Use sscanf to pull:
     *   page_str, byte_str, val_str
     *   from: "C0.0A <spaces> 00000000 <spaces> 00 ..."
     */
    int n = sscanf(p,
                   " %2[0-9A-Fa-f].%2[0-9A-Fa-f] %*s %2[0-9A-Fa-f]",
                   page_str, byte_str, val_str);
    if (n == 3) {
        uint8_t pg, bt, hv;
        if (parse_hex2_u8(page_str, &pg) != 0) return -1;
        if (parse_hex2_u8(byte_str, &bt) != 0) return -1;
        if (parse_hex2_u8(val_str,  &hv) != 0) return -1;
        *page = pg;
        *byte = bt;
        *val  = hv;
        return 1;
    }

    return 0;
}

/* ---------- State machine ---------- */

typedef enum {
    TCS_STATE_BEFORE_TABLE = 0,   /* ignoring everything until header line */
    TCS_STATE_IN_TABLE,           /* parsing register rows */
    TCS_STATE_AFTER_TABLE         /* done; ignore rest */
} tcs_state_t;

dpll_result_t dpll_apply_tcs_file(int spi_fd, const char *path, int verbose)
{
    if (spi_fd < 0 || !path) return DPLL_ERR;

    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "[tcs] Failed to open '%s': %s\n", path, strerror(errno));
        return DPLL_ERR;
    }

    char line[512];
    unsigned long line_num = 0;
    dpll_result_t result = DPLL_OK;

    tcs_state_t state = TCS_STATE_BEFORE_TABLE;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        char *s = trim(line);
        if (*s == '\0') {
            /* blank line */
            continue;
        }

        switch (state) {
        case TCS_STATE_BEFORE_TABLE:
            /* Look for header line like:
             *   Page.Byte#                      BinaryFormat HexValue Page.Byte#
             */
            if (strstr(s, "Page.Byte#") && strstr(s, "HexValue")) {
                state = TCS_STATE_IN_TABLE;
                if (verbose) {
                    fprintf(stderr, "[tcs] Found register table header at line %lu\n",
                            line_num);
                }
            }
            break;

        case TCS_STATE_IN_TABLE:
        {
            /* End condition: line after register definitions, starting the next section. */
            if (strncmp(s, "Data Fields", 11) == 0) {
                if (verbose) {
                    fprintf(stderr,
                            "[tcs] Reached 'Data Fields' at line %lu; ending reg parse.\n",
                            line_num);
                }
                state = TCS_STATE_AFTER_TABLE;
                break;
            }

            uint8_t page, byte, value;
            int r = parse_reg_line(s, &page, &byte, &value);
            if (r < 0) {
                fprintf(stderr, "[tcs] Parse error at line %lu: '%s'\n", line_num, s);
                result = DPLL_ERR;
                state = TCS_STATE_AFTER_TABLE;
                break;
            } else if (r == 0) {
                /* Some non-reg line inside the block (divider, comment, etc.) */
                break;
            }

            uint16_t addr = ((uint16_t)page << 8) | (uint16_t)byte;

            if (verbose) {
                fprintf(stderr,
                        "[tcs] reg 0x%04X (page 0x%02X, byte 0x%02X) <- 0x%02X\n",
                        addr, page, byte, value);
            }

            if (dpll_write8(spi_fd, addr, value) != DPLL_OK) {
                fprintf(stderr,
                        "[tcs] dpll_write8 failed at line %lu, addr=0x%04X\n",
                        line_num, addr);
                result = DPLL_ERR;
                state = TCS_STATE_AFTER_TABLE;
            }
            break;
        }

        case TCS_STATE_AFTER_TABLE:
            /* We’re done with register parsing; ignore everything else. */
            break;
        }

        if (state == TCS_STATE_AFTER_TABLE && result != DPLL_OK) {
            break;
        }
    }

    fclose(fp);

    if (state == TCS_STATE_BEFORE_TABLE) {
        /* Never saw a header line */
        if (verbose) {
            fprintf(stderr, "[tcs] No register table header found in '%s'\n", path);
        }
        return DPLL_ERR;
    }

    return result;
}



/* -------------------------------------------------------------------------- */
/* Timing Commander "Programming File" (.txt) parser                          */
/* -------------------------------------------------------------------------- */

/* Parse a single programming-file line:
 *
 *   Size: 0x3, Offset: FFFD, Data: 0x001020
 *   Offset: CB30, Data: 0x00000000
 *
 * Returns:
 *   1 on success (fills addr, buf, len)
 *   0 if the line is not a programming line
 *  -1 on a hard parse error
 */
static int parse_program_line(const char *line,
                              uint16_t *addr,
                              uint8_t  *buf,
                              size_t   *len,
                              size_t    buf_cap)
{
    unsigned long size_ul = 0;
    unsigned int  offset_u = 0;
    char data_hex[512] = {0};
    int n;

    /* Try form with explicit Size: */
    n = sscanf(line,
               " Size: 0x%lx, Offset: %x, Data: 0x%511[0-9A-Fa-f]",
               &size_ul, &offset_u, data_hex);
    if (n != 3) {
        /* Try form without Size: */
        n = sscanf(line,
                   " Offset: %x, Data: 0x%511[0-9A-Fa-f]",
                   &offset_u, data_hex);
        if (n != 2) {
            /* Not a programming line */
            return 0;
        }
        /* Infer size from data length */
        size_ul = 0;
    }

    size_t hex_len = strlen(data_hex);
    if (hex_len == 0 || (hex_len & 1) != 0) {
        /* odd number of hex chars is suspicious */
        return -1;
    }

    size_t inferred_bytes = hex_len / 2;
    size_t size_bytes;

    if (size_ul == 0) {
        /* No explicit Size: use all data */
        size_bytes = inferred_bytes;
    } else {
        size_bytes = (size_t)size_ul;
        if (size_bytes > inferred_bytes) {
            /* File inconsistent: Size says more bytes than hex available */
            size_bytes = inferred_bytes; /* clamp but treat as error-ish */
        }
    }

    if (size_bytes == 0) {
        /* Nothing to write; treat as a no-op programming line */
        *addr = (uint16_t)offset_u;
        *len  = 0;
        return 1;
    }

    if (size_bytes > buf_cap) {
        /* Too big for our buffer */
        return -1;
    }

    /* Convert hex pairs to bytes */
    for (size_t i = 0; i < size_bytes; ++i) {
        char tmp[3];
        tmp[0] = data_hex[2*i];
        tmp[1] = data_hex[2*i + 1];
        tmp[2] = '\0';

        char *end = NULL;
        unsigned long v = strtoul(tmp, &end, 16);
        if (end == tmp || v > 0xFF) {
            return -1;
        }
        buf[i] = (uint8_t)v;
    }

    *addr = (uint16_t)offset_u;
    *len  = size_bytes;
    return 1;
}

dpll_result_t dpll_apply_program_file(int spi_fd, const char *path, int verbose)
{
    if (spi_fd < 0 || !path) {
        return DPLL_ERR;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "[prog] Failed to open '%s': %s\n",
                path, strerror(errno));
        return DPLL_ERR;
    }

    char line[512];
    unsigned long line_num   = 0;
    dpll_result_t result     = DPLL_OK;
    size_t total_bytes_written = 0;
    size_t total_records       = 0;

    /* Enough for the largest Size we expect (0x10, 0x38, etc.) */
    uint8_t data_buf[256];

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        char *s = trim(line);
        if (*s == '\0') {
            continue; /* blank */
        }

        /* Skip comment-style lines if present (e.g. starting with '#') */
        if (s[0] == '#') {
            continue;
        }

        uint16_t addr = 0;
        size_t   len  = 0;

        int r = parse_program_line(s, &addr, data_buf, &len,
                                   sizeof(data_buf));
        if (r < 0) {
            fprintf(stderr, "[prog] Parse error at line %lu: '%s'\n",
                    line_num, s);
            result = DPLL_ERR;
            break;
        } else if (r == 0) {
            /* Not a programming line; ignore */
            continue;
        }

        /* r == 1: we have addr + len bytes in data_buf */
        if (len == 0) {
            /* no-op line, but counted */
            if (verbose) {
                fprintf(stderr,
                        "[prog] line %lu: addr=0x%04X len=0 (no-op)\n",
                        line_num, addr);
            }
            continue;
        }

        if (verbose) {
            fprintf(stderr,
                    "[prog] line %lu: addr=0x%04X len=%zu data=",
                    line_num, addr, len);
            for (size_t i = 0; i < len; ++i) {
                fprintf(stderr, "%02X", data_buf[i]);
            }
            fprintf(stderr, "\n");
        }

        if (dpll_write_seq(spi_fd, addr, data_buf, len) != DPLL_OK) {
            fprintf(stderr,
                    "[prog] dpll_write_seq failed at line %lu, addr=0x%04X len=%zu\n",
                    line_num, addr, len);
            result = DPLL_ERR;
            break;
        }

        total_bytes_written += len;
        total_records++;
    }

    fclose(fp);

    if (result == DPLL_OK && verbose) {
        fprintf(stderr,
                "[prog] Finished. Records: %zu, total bytes: %zu\n",
                total_records, total_bytes_written);
    }

    return result;
}


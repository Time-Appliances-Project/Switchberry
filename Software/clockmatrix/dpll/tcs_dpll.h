#ifndef TCS_DPLL_H
#define TCS_DPLL_H

#include <stdint.h>
#include "linux_dpll.h"   /* for dpll_result_t, dpll_write8, etc. */

#ifdef __cplusplus
extern "C" {
#endif

/* Parse a Timing Commander .tcs file and write all register values via
 * dpll_write8() using the given SPI file descriptor.
 *
 * State machine:
 *   - BEFORE_TABLE: ignore lines until we see the register header line:
 *         Page.Byte#                      BinaryFormat HexValue Page.Byte#
 *
 *   - IN_TABLE: for each register line like:
 *         C0.0A                                00000000       00 C0.0A
 *
 *       we compute:
 *         page  = 0xC0
 *         byte  = 0x0A
 *         addr  = (page << 8) | byte  (0xC00A)
 *         value = 0x00
 *
 *       and call:
 *         dpll_write8(spi_fd, addr, value)
 *
 *   - AFTER_TABLE: stop when we see the line that starts the next section:
 *         Data Fields
 *
 * Parameters:
 *   spi_fd  - open spidev fd (from dpll_spi_open)
 *   path    - path to .tcs file
 *   verbose - if non-zero, prints debug info about parsing and writes
 *
 * Returns:
 *   DPLL_OK on success
 *   DPLL_ERR on I/O/parse error or on write failure
 */
dpll_result_t dpll_apply_tcs_file(int spi_fd, const char *path, int verbose);





/* ------------------------------------------------------------------------- */
/*  Timing Commander "Programming File" (.txt) support                       */
/* ------------------------------------------------------------------------- */
/*
 * Parse a Timing Commander "Programming File" text export and write all
 * register data via dpll_write_seq() using the given SPI file descriptor.
 *
 * The file contains lines of the form:
 *
 *   Size: 0x3, Offset: FFFD, Data: 0x001020
 *
 * or sometimes without an explicit Size:
 *
 *   Offset: CB30, Data: 0x00000000
 *
 * Interpretation:
 *   - Offset: 16-bit register address (hex, no 0x, e.g. FFFD -> 0xFFFD)
 *   - Size:   number of bytes in Data (hex, with 0x, e.g. 0x3 -> 3 bytes)
 *   - Data:   2*Size hex digits after 0x, MSB-first in the text but applied
 *             sequentially:
 *
 *       Size: 0x3, Offset: FFFD, Data: 0x001020
 *
 *     results in:
 *       addr=0xFFFD -> 0x00
 *       addr=0xFFFE -> 0x10
 *       addr=0xFFFF -> 0x20
 *
 * If Size is omitted, we infer the number of bytes from the Data hex string.
 *
 * Parameters:
 *   spi_fd  - open spidev fd (from dpll_spi_open)
 *   path    - path to the programming .txt file
 *   verbose - if non-zero, prints debug info for each parsed line and write
 *
 * Returns:
 *   DPLL_OK on success
 *   DPLL_ERR on I/O/parse error or on any write failure
 */
dpll_result_t dpll_apply_program_file(int spi_fd, const char *path, int verbose);


#ifdef __cplusplus
}
#endif

#endif /* TCS_DPLL_H */


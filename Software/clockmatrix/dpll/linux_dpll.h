#ifndef LINUX_DPLL_H
#define LINUX_DPLL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Result code (same semantics as ws_result_t) */
typedef enum {
    DPLL_OK  = 0,
    DPLL_ERR = -1
} dpll_result_t;

/* ---------- SPI device (Linux spidev) ---------- */

/* Open /dev/spidevX.Y with given speed (Hz) and mode (0..3).
 * Returns a file descriptor >= 0 on success, or -1 on error. */
int  dpll_spi_open(const char *dev_path, uint32_t hz, uint8_t mode);

/* Close fd from dpll_spi_open(). Safe to call with fd<0. */
void dpll_spi_close(int fd);

/* ---------- Page computation (unchanged from wiwistick) ---------- */
/* Compute the 4 page-register bytes for 8A34001 1-byte mode. */
static inline void dpll_compute_page_from_addr(uint16_t addr, uint8_t page4[4])
{
    uint8_t a7  = (addr & 0x0080) ? 1 : 0;       // A7 from low byte
    uint8_t hi  = (uint8_t)((addr >> 8) & 0xFF); // A15..A8
    page4[0] = a7 ? 0x80 : 0x00;
    page4[1] = hi;
    page4[2] = 0x10;
    page4[3] = 0x20;
}

/* ---------- Core DPLL register access (1B mode) ---------- */

/* Write the 4-byte Page Register burst at offset 0x7C. */
dpll_result_t dpll_write_page(int spi_fd, const uint8_t page4[4]);

/* Compute + write page register for a 16-bit address. */
dpll_result_t dpll_set_page_for_addr(int spi_fd, uint16_t addr);

/* Single-byte write to 16-bit address (computes+writes page). */
dpll_result_t dpll_write8(int spi_fd, uint16_t addr, uint8_t value);

/* Single-byte read from 16-bit address (computes+writes page). */
dpll_result_t dpll_read8(int spi_fd, uint16_t addr, uint8_t *value_out);

/* Sequential writes starting at address, auto-incrementing A6..A0. */
dpll_result_t dpll_write_seq(int spi_fd,
                             uint16_t start_addr,
                             const uint8_t *data,
                             size_t len);

/* Sequential reads starting at address, auto-incrementing A6..A0. */
dpll_result_t dpll_read_seq(int spi_fd,
                            uint16_t start_addr,
                            uint8_t *data_out,
                            size_t len);

/* Burst write across page boundaries if needed. */
dpll_result_t dpll_burst_write(int spi_fd,
                               uint16_t start_addr,
                               const uint8_t *data,
                               size_t len);

/* Burst read across page boundaries if needed. */
dpll_result_t dpll_burst_read(int spi_fd,
                              uint16_t start_addr,
                              uint8_t *data_out,
                              size_t len);

/* ---------- EEPROM API via DPLL I2C master ---------- */
/* (ported from wiwistick_dpll.c) */

/* Progress callback during HEX flashing:
 *   written = bytes written so far
 *   total   = total if known, or 0
 *   user    = your context pointer
 */
typedef void (*dpll_flash_progress_cb)(size_t written,
                                       size_t total,
                                       void  *user);

/* Write up to 'len' bytes starting at 24-bit EEPROM addr [0..0x1_FFFF]. */
dpll_result_t dpll_eeprom_write(int spi_fd,
                                uint32_t addr,
                                const uint8_t *data,
                                size_t len);

/* Read up to 'len' bytes starting at 24-bit EEPROM addr. */
dpll_result_t dpll_eeprom_read(int spi_fd,
                               uint32_t addr,
                               uint8_t *data_out,
                               size_t len);

/* Flash an Intel HEX file directly into the EEPROM. */
dpll_result_t dpll_eeprom_flash_hex(int spi_fd,
                                    const char *path,
                                    dpll_flash_progress_cb cb,
                                    void *cb_user);

#ifdef __cplusplus
}
#endif

#endif /* LINUX_DPLL_H */


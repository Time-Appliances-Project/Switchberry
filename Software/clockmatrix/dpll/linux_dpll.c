#include "linux_dpll.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>      // usleep, close
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

/* ---------- SPI helpers (spidev) ---------- */

int dpll_spi_open(const char *dev_path, uint32_t hz, uint8_t mode)
{
    int fd = open(dev_path, O_RDWR);
    if (fd < 0) {
        perror("open spidev");
        return -1;
    }

    /* Mode 0..3 only */
    uint8_t m = mode & 0x3;
    if (ioctl(fd, SPI_IOC_WR_MODE, &m) < 0) {
        perror("SPI_IOC_WR_MODE");
        close(fd);
        return -1;
    }

    uint8_t bits = 8;
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        perror("SPI_IOC_WR_BITS_PER_WORD");
        close(fd);
        return -1;
    }

    if (hz) {
        if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &hz) < 0) {
            perror("SPI_IOC_WR_MAX_SPEED_HZ");
            close(fd);
            return -1;
        }
    }

    return fd;
}

void dpll_spi_close(int fd)
{
    if (fd >= 0) close(fd);
}

static dpll_result_t dpll_spi_xfer(int fd,
                                   const uint8_t *tx,
                                   uint8_t       *rx,
                                   size_t         len)
{
    if (len == 0) return DPLL_OK;

    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf        = (unsigned long)tx;
    tr.rx_buf        = (unsigned long)rx;
    tr.len           = len;
    tr.bits_per_word = 8;
    tr.cs_change     = 0;   /* keep CS asserted only during this transfer */

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        perror("SPI_IOC_MESSAGE");
        return DPLL_ERR;
    }
    return DPLL_OK;
}

/* ---------- Core single ops (ported from wiwistick_dpll.c) ---------- */

dpll_result_t dpll_write_page(int spi_fd, const uint8_t page4[4])
{
    if (spi_fd < 0 || !page4) return DPLL_ERR;

    uint8_t buf[1 + 4];
    buf[0] = 0x7C;                      // Page Register offset (burst write)
    memcpy(&buf[1], page4, 4);

    return dpll_spi_xfer(spi_fd, buf, NULL, sizeof(buf));
}

dpll_result_t dpll_set_page_for_addr(int spi_fd, uint16_t addr)
{
    uint8_t page4[4];
    dpll_compute_page_from_addr(addr, page4);
    return dpll_write_page(spi_fd, page4);
}

dpll_result_t dpll_write8(int spi_fd, uint16_t addr, uint8_t value)
{
    if (spi_fd < 0) return DPLL_ERR;
    if (dpll_set_page_for_addr(spi_fd, addr) != DPLL_OK) return DPLL_ERR;

    uint8_t buf[2];
    buf[0] = (uint8_t)(addr & 0x7F);   // MSB=0 (write), A6..A0 in cmd
    buf[1] = value;

    return dpll_spi_xfer(spi_fd, buf, NULL, sizeof(buf));
}

dpll_result_t dpll_read8(int spi_fd, uint16_t addr, uint8_t *value_out)
{
    if (spi_fd < 0 || !value_out) return DPLL_ERR;
    if (dpll_set_page_for_addr(spi_fd, addr) != DPLL_OK) return DPLL_ERR;

    uint8_t tx[2];
    uint8_t rx[2];

    tx[0] = (uint8_t)(0x80 | (addr & 0x7F)); // MSB=1 (read)
    tx[1] = 0x00;
    rx[0] = rx[1] = 0;

    if (dpll_spi_xfer(spi_fd, tx, rx, sizeof(tx)) != DPLL_OK) return DPLL_ERR;

    *value_out = rx[1];
    return DPLL_OK;
}

dpll_result_t dpll_write_seq(int spi_fd,
                             uint16_t start_addr,
                             const uint8_t *data,
                             size_t len)
{
    if (spi_fd < 0 || (!data && len)) return DPLL_ERR;
    if (len == 0) return DPLL_OK;

    if (dpll_set_page_for_addr(spi_fd, start_addr) != DPLL_OK) return DPLL_ERR;

    /* Pack command + data in one transfer so CS stays low */
    uint8_t *buf = malloc(1 + len);
    if (!buf) return DPLL_ERR;

    buf[0] = (uint8_t)(start_addr & 0x7F); // write cmd
    memcpy(&buf[1], data, len);

    dpll_result_t r = dpll_spi_xfer(spi_fd, buf, NULL, 1 + len);
    free(buf);
    return r;
}

dpll_result_t dpll_read_seq(int spi_fd,
                            uint16_t start_addr,
                            uint8_t *data_out,
                            size_t len)
{
    if (spi_fd < 0 || (!data_out && len)) return DPLL_ERR;
    if (len == 0) return DPLL_OK;

    if (dpll_set_page_for_addr(spi_fd, start_addr) != DPLL_OK) return DPLL_ERR;

    /* tx: [cmd, 0, 0, ...], rx: [ignored, data...] */
    uint8_t *tx = calloc(1 + len, 1);
    uint8_t *rx = calloc(1 + len, 1);
    if (!tx || !rx) {
        free(tx);
        free(rx);
        return DPLL_ERR;
    }

    tx[0] = (uint8_t)(0x80 | (start_addr & 0x7F)); // read cmd

    dpll_result_t r = dpll_spi_xfer(spi_fd, tx, rx, 1 + len);
    if (r == DPLL_OK) {
        memcpy(data_out, &rx[1], len);
    }

    free(tx);
    free(rx);
    return r;
}

/* ---------- Cross-page helpers (burst read/write) ---------- */

dpll_result_t dpll_burst_write(int spi_fd,
                               uint16_t addr,
                               const uint8_t *data,
                               size_t len)
{
    if (spi_fd < 0 || (!data && len)) return DPLL_ERR;
    if (len == 0) return DPLL_OK;

    while (len > 0) {
        uint8_t off7  = (uint8_t)(addr & 0x7F);
        size_t  room  = (size_t)(0x80 - off7);
        size_t  chunk = (len < room) ? len : room;

        if (dpll_set_page_for_addr(spi_fd, addr) != DPLL_OK) return DPLL_ERR;

        /* One transfer: cmd + chunk data */
        uint8_t *buf = malloc(1 + chunk);
        if (!buf) return DPLL_ERR;

        buf[0] = (uint8_t)(addr & 0x7F); // write cmd
        memcpy(&buf[1], data, chunk);

        dpll_result_t r = dpll_spi_xfer(spi_fd, buf, NULL, 1 + chunk);
        free(buf);
        if (r != DPLL_OK) return r;

        addr += (uint16_t)chunk;
        data += chunk;
        len  -= chunk;
    }

    return DPLL_OK;
}

dpll_result_t dpll_burst_read(int spi_fd,
                              uint16_t addr,
                              uint8_t *data_out,
                              size_t len)
{
    if (spi_fd < 0 || (!data_out && len)) return DPLL_ERR;
    if (len == 0) return DPLL_OK;

    while (len > 0) {
        uint8_t off7  = (uint8_t)(addr & 0x7F);
        size_t  room  = (size_t)(0x80 - off7);
        size_t  chunk = (len < room) ? len : room;

        if (dpll_set_page_for_addr(spi_fd, addr) != DPLL_OK) return DPLL_ERR;

        /* tx: [cmd, 0..], rx: [ignored, data...] */
        uint8_t *tx = calloc(1 + chunk, 1);
        uint8_t *rx = calloc(1 + chunk, 1);
        if (!tx || !rx) {
            free(tx);
            free(rx);
            return DPLL_ERR;
        }

        tx[0] = (uint8_t)(0x80 | (addr & 0x7F)); // read cmd

        dpll_result_t r = dpll_spi_xfer(spi_fd, tx, rx, 1 + chunk);
        if (r == DPLL_OK) {
            memcpy(data_out, &rx[1], chunk);
        }

        free(tx);
        free(rx);
        if (r != DPLL_OK) return r;

        addr     += (uint16_t)chunk;
        data_out += chunk;
        len      -= chunk;
    }

    return DPLL_OK;
}

/* ============================================================
 * EEPROM interface (ported from wiwistick_dpll.c)
 * ============================================================ */

/* DPLL EEPROM register window via DPLL registers :contentReference[oaicite:3]{index=3} */

#define EE_IF_BASE         0xCF68
#define EE_IF_ADDR         (EE_IF_BASE + 0x00)  /* I2C device addr (0x54 or 0x55) */
#define EE_IF_SIZE         (EE_IF_BASE + 0x01)  /* number of bytes to xfer (1..128) */
#define EE_IF_OFF_L        (EE_IF_BASE + 0x02)  /* 16-bit offset (low) */
#define EE_IF_OFF_H        (EE_IF_BASE + 0x03)  /* 16-bit offset (high) */
#define EE_IF_CMD_L        (EE_IF_BASE + 0x04)  /* command low */
#define EE_IF_CMD_H        (EE_IF_BASE + 0x05)  /* command high */

#define EE_DATA_BASE       0xCF80               /* data window 0..127 */

#define EE_CMD_READ_L      0x01
#define EE_CMD_WRITE_L     0x02
#define EE_CMD_MAGIC_H     0xEE

#define EE_I2C_ADDR_BLOCK0 0x54  /* addresses 0x00000..0x0FFFF */
#define EE_I2C_ADDR_BLOCK1 0x55  /* addresses 0x10000..0x1FFFF */

#define EE_DELAY_WRITE_US  100000   /* 100ms for up-to-128-byte writes */
#define EE_DELAY_READ_US   10000    /* 10ms for reads */

/* ---- EEPROM helpers ---- */

static inline uint8_t block_addr_for(uint32_t a24)
{
    return (a24 > 0xFFFF) ? EE_I2C_ADDR_BLOCK1 : EE_I2C_ADDR_BLOCK0;
}

static inline uint16_t block_offset_for(uint32_t a24)
{
    return (uint16_t)(a24 & 0xFFFF);
}

static dpll_result_t ee_set_block_and_offset(int spi_fd,
                                             uint32_t a24,
                                             size_t size)
{
    if (size == 0 || size > 128) return DPLL_ERR;

    const uint8_t  dev = block_addr_for(a24);
    const uint16_t off = block_offset_for(a24);

    if (dpll_write8(spi_fd, EE_IF_ADDR,  dev) != DPLL_OK) return DPLL_ERR;
    if (dpll_write8(spi_fd, EE_IF_OFF_L, (uint8_t)(off & 0xFF)) != DPLL_OK) return DPLL_ERR;
    if (dpll_write8(spi_fd, EE_IF_OFF_H, (uint8_t)(off >> 8))   != DPLL_OK) return DPLL_ERR;
    if (dpll_write8(spi_fd, EE_IF_SIZE,  (uint8_t)size)         != DPLL_OK) return DPLL_ERR;

    return DPLL_OK;
}

static dpll_result_t ee_kick_cmd(int spi_fd, uint8_t cmd_lo)
{
    if (dpll_write8(spi_fd, EE_IF_CMD_L, cmd_lo)       != DPLL_OK) return DPLL_ERR;
    if (dpll_write8(spi_fd, EE_IF_CMD_H, EE_CMD_MAGIC_H) != DPLL_OK) return DPLL_ERR;
    return DPLL_OK;
}

/* ---- EEPROM public API ---- */

dpll_result_t dpll_eeprom_write(int spi_fd,
                                uint32_t addr,
                                const uint8_t *data,
                                size_t len)
{
    if (spi_fd < 0 || (!data && len)) return DPLL_ERR;
    if (len == 0) return DPLL_OK;

    size_t   remaining = len;
    uint32_t cur       = addr;

    while (remaining > 0) {
        /* respect 64K block boundary */
        size_t block_room = 0x10000u - (cur & 0xFFFFu);
        if (block_room == 0) block_room = 0x10000u;

        /* program up to 128 bytes per command and not across block boundary */
        size_t chunk = remaining;
        if (chunk > 128)      chunk = 128;
        if (chunk > block_room) chunk = block_room;

        if (ee_set_block_and_offset(spi_fd, cur, chunk) != DPLL_OK) return DPLL_ERR;

        /* load data bytes into EE_DATA window */
        if (dpll_burst_write(spi_fd, EE_DATA_BASE, data, chunk) != DPLL_OK) return DPLL_ERR;

        /* issue WRITE command */
        if (ee_kick_cmd(spi_fd, EE_CMD_WRITE_L) != DPLL_OK) return DPLL_ERR;

        /* conservative delay */
        usleep(EE_DELAY_WRITE_US);

        cur       += (uint32_t)chunk;
        data      += chunk;
        remaining -= chunk;
    }

    return DPLL_OK;
}

dpll_result_t dpll_eeprom_read(int spi_fd,
                               uint32_t addr,
                               uint8_t *data_out,
                               size_t len)
{
    if (spi_fd < 0 || (!data_out && len)) return DPLL_ERR;
    if (len == 0) return DPLL_OK;

    size_t   remaining = len;
    uint32_t cur       = addr;

    while (remaining > 0) {
        size_t block_room = 0x10000u - (cur & 0xFFFFu);
        if (block_room == 0) block_room = 0x10000u;

        size_t chunk = remaining;
        if (chunk > 128)      chunk = 128;
        if (chunk > block_room) chunk = block_room;

        if (ee_set_block_and_offset(spi_fd, cur, chunk) != DPLL_OK) return DPLL_ERR;

        if (ee_kick_cmd(spi_fd, EE_CMD_READ_L) != DPLL_OK) return DPLL_ERR;

        usleep(EE_DELAY_READ_US);

        if (dpll_burst_read(spi_fd, EE_DATA_BASE, data_out, chunk) != DPLL_OK) return DPLL_ERR;

        cur        += (uint32_t)chunk;
        data_out   += chunk;
        remaining  -= chunk;
    }

    return DPLL_OK;
}

/* ---- Intel HEX flasher (same logic as ws_dpll_eeprom_flash_hex) ---- */

static int parse_hex_byte(const char *p, uint8_t *out)
{
    char t[3] = { p[0], p[1], 0 };
    char *end = NULL;
    long v = strtol(t, &end, 16);
    if (!end || *end) return -1;
    if (v < 0 || v > 0xFF) return -1;
    *out = (uint8_t)v;
    return 0;
}

static int parse_hex_word(const char *p, uint16_t *out)
{
    char t[5] = { p[0], p[1], p[2], p[3], 0 };
    char *end = NULL;
    long v = strtol(t, &end, 16);
    if (!end || *end) return -1;
    if (v < 0 || v > 0xFFFF) return -1;
    *out = (uint16_t)v;
    return 0;
}



/* Pre-scan Intel HEX file to count total data bytes (type 00 records). */
static int ihex_count_data_bytes(const char *path, size_t *total_out)
{
    if (!path || !total_out) return -1;

    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("[dpll] open hex (count)");
        return -1;
    }

    char line[600];
    size_t total = 0;

    while (fgets(line, sizeof(line), fp)) {
        char *p = line;

        /* Trim leading whitespace */
        while (*p && isspace((unsigned char)*p)) ++p;
        if (*p == 0 || *p != ':')
            continue;

        /* Trim trailing CR/LF/whitespace */
        char *end = p + strlen(p);
        while (end > p && isspace((unsigned char)end[-1])) {
            *--end = '\0';
        }

        /* Need at least ':', count(2), addr(4), type(2), checksum(2) = 11 chars */
        size_t len = strlen(p);
        if (len < 11) {
            fclose(fp);
            return -1;
        }

        uint8_t count = 0;
        uint8_t type  = 0;

        if (parse_hex_byte(p + 1, &count) != 0) {
            fclose(fp);
            return -1;
        }
        if (parse_hex_byte(p + 7, &type) != 0) {
            fclose(fp);
            return -1;
        }

        if (type == 0x00) {
            total += count;
        } else if (type == 0x01) {
            /* EOF */
            break;
        }
        /* other record types ignored for counting */
    }

    fclose(fp);
    *total_out = total;
    return 0;
}

dpll_result_t dpll_eeprom_flash_hex(int spi_fd,
                                    const char *path,
                                    dpll_flash_progress_cb cb,
                                    void *cb_user)
{
    if (spi_fd < 0 || !path) return DPLL_ERR;

    /* Optional: pre-compute total data bytes for nicer progress */
    size_t total_bytes = 0;
    if (cb) {
        if (ihex_count_data_bytes(path, &total_bytes) != 0) {
            total_bytes = 0;  /* unknown */
        }
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("[dpll] open hex");
        return DPLL_ERR;
    }

    char line[600];
    uint32_t ext_lin_addr = 0;  /* upper 16 bits from type 04 */
    size_t total_written  = 0;

    while (fgets(line, sizeof(line), fp)) {
        char *p = line;

        /* Trim leading whitespace */
        while (*p && isspace((unsigned char)*p)) ++p;
        if (*p == 0) continue;
        if (*p != ':') continue;

        /* Trim trailing CR/LF/whitespace */
        char *end = p + strlen(p);
        while (end > p && isspace((unsigned char)end[-1])) {
            *--end = '\0';
        }

        size_t len = strlen(p);
        if (len < 11) {
            fprintf(stderr, "[dpll] Bad HEX line (too short): %s\n", p);
            fclose(fp);
            return DPLL_ERR;
        }

        /* Parse header: :CCAAAA TT ... */
        uint8_t  count  = 0;
        uint16_t offset = 0;
        uint8_t  type   = 0;

        if (parse_hex_byte(p + 1, &count)   != 0) {
            fprintf(stderr, "[dpll] Bad HEX line (count parse): %s\n", p);
            fclose(fp);
            return DPLL_ERR;
        }
        if (parse_hex_word(p + 3, &offset)  != 0) {
            fprintf(stderr, "[dpll] Bad HEX line (offset parse): %s\n", p);
            fclose(fp);
            return DPLL_ERR;
        }
        if (parse_hex_byte(p + 7, &type)    != 0) {
            fprintf(stderr, "[dpll] Bad HEX line (type parse): %s\n", p);
            fclose(fp);
            return DPLL_ERR;
        }

        /* Minimum record length check:
         * ':' + CC(2) + AAAA(4) + TT(2) + DATA(2*count) + CS(2)
         * so len (from ':') must be >= 11 + 2*count
         */
        if (len < 11 + (size_t)count * 2) {
            fprintf(stderr, "[dpll] Bad HEX line (length vs count): %s\n", p);
            fclose(fp);
            return DPLL_ERR;
        }

        const char *data_hex = p + 9;                 /* first data byte */
        const char *chk_hex  = data_hex + count * 2;  /* checksum byte (2 hex chars) */

        /* Parse data bytes (if any) */
        uint8_t data[256];
	/* should never happen just ignore
        if ( ( (int)count ) > ( (int)sizeof(data) ) ) {
            fprintf(stderr, "[dpll] Bad HEX line (count too large): %s\n", p);
            fclose(fp);
            return DPLL_ERR;
        }
	*/

        uint8_t sum = 0;
        sum += count;
        sum += (uint8_t)(offset >> 8);
        sum += (uint8_t)(offset & 0xFF);
        sum += type;

        for (uint8_t i = 0; i < count; ++i) {
            if (parse_hex_byte(data_hex + (i * 2), &data[i]) != 0) {
                fprintf(stderr, "[dpll] Bad HEX line (data parse): %s\n", p);
                fclose(fp);
                return DPLL_ERR;
            }
            sum += data[i];
        }

        /* Parse checksum */
        uint8_t chk = 0;
        if (parse_hex_byte(chk_hex, &chk) != 0) {
            fprintf(stderr, "[dpll] Bad HEX line (checksum parse): %s\n", p);
            fclose(fp);
            return DPLL_ERR;
        }
        sum += chk;

        if ((sum & 0xFF) != 0) {
            fprintf(stderr, "[dpll] Bad HEX line (checksum mismatch): %s\n", p);
            fclose(fp);
            return DPLL_ERR;
        }

        /* Now interpret the record type */
        if (type == 0x00) {
            /* Data record */
            uint32_t full_addr = (ext_lin_addr << 16) | offset;

            if (dpll_eeprom_write(spi_fd, full_addr, data, count) != DPLL_OK) {
                fclose(fp);
                return DPLL_ERR;
            }
            total_written += count;
            if (cb) {
                cb(total_written, total_bytes, cb_user);
            }

        } else if (type == 0x04) {
            /* Extended Linear Address: 2-byte upper address */
            if (count != 2) {
                fprintf(stderr, "[dpll] Bad HEX line (type 04 count != 2): %s\n", p);
                fclose(fp);
                return DPLL_ERR;
            }
            ext_lin_addr = ((uint32_t)data[0] << 8) | (uint32_t)data[1];

        } else if (type == 0x01) {
            /* EOF */
            break;
        } else {
            /* Other record types (02,03,05) are ignored for EEPROM flashing. */
        }
    }

    fclose(fp);
    return DPLL_OK;
}


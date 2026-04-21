/**
 * dwm3000.h — DW3000 (DWM3000 module) multi-instance driver for Teensy 4.1 bring-up
 *
 * Supports multiple DWM3000 modules on a SHARED SPI bus with
 * individual CS/RST/IRQ pins.  Each module is represented by a
 * dwm3000_inst_t instance.
 *
 * Headset topology:
 *   SPI1 bus (SCK/MOSI/MISO shared) → 3× DWM3000 (T / L / R)
 *   Each has its own CS, RST, IRQ GPIO.
 *
 * NOTE: Register addresses and bit masks are based on the DW3000
 * User Manual (v1.1).  Verify against your firmware revision.
 */

#ifndef DWM3000_H
#define DWM3000_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ================================================================
//  Register address encoding:  (file_id << 16) | sub_offset
// ================================================================

#define DW_REG(file, off)  (((uint32_t)(file) << 16) | (uint16_t)(off))
#define DW_FILE(addr)      ((uint8_t)((addr) >> 16))
#define DW_OFF(addr)       ((uint16_t)((addr) & 0xFFFF))

// ── Register File 0x00: General Configuration ───────────────────
#define DW_DEV_ID           DW_REG(0x00, 0x00)
#define DW_EUI_64           DW_REG(0x00, 0x04)
#define DW_PANADR           DW_REG(0x00, 0x0C)
#define DW_SYS_CFG          DW_REG(0x00, 0x10)
#define DW_FF_CFG           DW_REG(0x00, 0x14)
#define DW_SYS_TIME         DW_REG(0x00, 0x1C)
#define DW_TX_FCTRL         DW_REG(0x00, 0x24)
#define DW_TX_FCTRL_HI      DW_REG(0x00, 0x28)
#define DW_DX_TIME          DW_REG(0x00, 0x2C)
#define DW_RX_FWTO          DW_REG(0x00, 0x34)
#define DW_SYS_STATUS       DW_REG(0x00, 0x44)
#define DW_RX_FINFO         DW_REG(0x00, 0x4C)
#define DW_RX_TIME          DW_REG(0x00, 0x64)
#define DW_TX_TIME          DW_REG(0x00, 0x74)
#define DW_TX_ANTD          DW_REG(0x01, 0x04)   // Corrected: file 0x01, offset 0x04
#define DW_CHAN_CTRL        DW_REG(0x01, 0x14)

// ── Buffers ─────────────────────────────────────────────────────
#define DW_TX_BUFFER        DW_REG(0x14, 0x00)
#define DW_RX_BUFFER_0      DW_REG(0x12, 0x00)
#define DW_RX_BUFFER_1      DW_REG(0x13, 0x00)

// ── CIA Configuration (RX antenna delay) ────────────────────────
#define DW_CIA_CONF         DW_REG(0x0E, 0x00)   // bits [15:0] = RXANTD

// ================================================================
//  SYS_STATUS bit masks
// ================================================================
#define DW_TXFRB_BIT        (1ULL <<  4)
#define DW_TXPRS_BIT        (1ULL <<  5)
#define DW_TXPHS_BIT        (1ULL <<  6)
#define DW_TXFRS_BIT        (1ULL <<  7)
#define DW_RXPRD_BIT        (1ULL <<  8)
#define DW_RXSFDD_BIT       (1ULL <<  9)
#define DW_RXPHD_BIT        (1ULL << 11)
#define DW_RXPHE_BIT        (1ULL << 12)
#define DW_RXFR_BIT         (1ULL << 13)
#define DW_RXFCG_BIT        (1ULL << 14)
#define DW_RXFCE_BIT        (1ULL << 15)
#define DW_RXFSL_BIT        (1ULL << 16)
#define DW_RXFTO_BIT        (1ULL << 17)
#define DW_RXOVRR_BIT       (1ULL << 20)
#define DW_RXPTO_BIT        (1ULL << 21)
#define DW_SPIRDY_BIT       (1ULL << 23)   // Corrected: bit 23, not bit 1

#define DW_ALL_TX_BITS      (DW_TXFRB_BIT | DW_TXPRS_BIT | DW_TXPHS_BIT | DW_TXFRS_BIT)
#define DW_ALL_RX_GOOD      (DW_RXFCG_BIT)
#define DW_ALL_RX_ERR       (DW_RXFCE_BIT | DW_RXPHE_BIT | DW_RXFTO_BIT | \
                             DW_RXOVRR_BIT | DW_RXPTO_BIT | DW_RXFSL_BIT)
#define DW_ALL_RX_EVENTS    (DW_ALL_RX_GOOD | DW_ALL_RX_ERR)

// ================================================================
//  Fast commands  (5-bit command index; dwm3000_cmd() encodes the
//  SPI byte as: (cmd << 1) | 0x81  →  [1][0][cmd4:cmd0][1] )
//
//  Ref: DWM3000 Data Sheet §1.3.3, Fast command transaction format
// ================================================================
#define DW_CMD_TXRXOFF      0x00    // → SPI byte 0x81
#define DW_CMD_TX           0x01    // → 0x83
#define DW_CMD_RX           0x02    // → 0x85
#define DW_CMD_DTX          0x03    // → 0x87
#define DW_CMD_DRX          0x04    // → 0x89
#define DW_CMD_TX_W4R       0x0C    // → 0x99  TX then enable RX (wait-for-response)
#define DW_CMD_DTX_W4R      0x0D    // → 0x9B  Delayed TX then enable RX
#define DW_CMD_CLR_IRQS     0x12    // → 0xA5  Clear all IRQ events
#define DW_CMD_DB_TOGGLE    0x13    // → 0xA7  Double-buffer toggle

// ================================================================
//  Physical constants
// ================================================================
#define DW_TIME_UNITS       (1.0 / (128.0 * 499.2e6))
#define SPEED_OF_LIGHT_M_S  299702547.0
#define DW_DEFAULT_ANT_DLY  16385
#define DW3000_DEV_ID       0xDECA0302U      // DW3000 IC
#define DW3110_DEV_ID       0xDECA0312U      // DW3110 IC (inside DWM3000 module)

// ================================================================
//  Instance descriptor — one per physical DWM3000 module
// ================================================================

typedef struct {
    void       *spi;        // SPIClass* stored opaquely for C compatibility
    uint8_t     pin_cs;     // Chip-select GPIO
    uint8_t     pin_rst;    // Reset GPIO
    uint8_t     pin_irq;    // IRQ GPIO
    const char *label;      // Human-readable name ("T", "L", "R", "Tool")
    bool        ready;      // Set true after successful init
} dwm3000_inst_t;

// ================================================================
//  Public API — all functions take an instance pointer
// ================================================================

/**
 * One-time SPI bus initialization (call once for the shared bus).
 * Sets up SCK/MOSI/MISO pin functions.
 */
void dwm3000_spi_bus_init(void *spi, uint32_t baud,
                          uint8_t pin_sck, uint8_t pin_mosi, uint8_t pin_miso);

/**
 * Initialize one DWM3000 instance: configure CS/RST/IRQ GPIOs,
 * hard-reset, verify DEV_ID.
 */
bool dwm3000_init(dwm3000_inst_t *inst);

/** Apply default UWB radio config (CH5, PRF64, 6.8 Mbps). */
void dwm3000_configure_default(dwm3000_inst_t *inst);

uint32_t dwm3000_read_dev_id(dwm3000_inst_t *inst);

// ── Register access ─────────────────────────────────────────────
void     dwm3000_read_reg(dwm3000_inst_t *inst, uint32_t reg, uint8_t *buf, size_t len);
void     dwm3000_write_reg(dwm3000_inst_t *inst, uint32_t reg, const uint8_t *buf, size_t len);
uint32_t dwm3000_read32(dwm3000_inst_t *inst, uint32_t reg);
void     dwm3000_write32(dwm3000_inst_t *inst, uint32_t reg, uint32_t val);
uint16_t dwm3000_read16(dwm3000_inst_t *inst, uint32_t reg);
void     dwm3000_write16(dwm3000_inst_t *inst, uint32_t reg, uint16_t val);
uint8_t  dwm3000_read8(dwm3000_inst_t *inst, uint32_t reg);
void     dwm3000_write8(dwm3000_inst_t *inst, uint32_t reg, uint8_t val);

// ── Fast commands ───────────────────────────────────────────────
void dwm3000_cmd(dwm3000_inst_t *inst, uint8_t fast_cmd);

// ── TX primitives ───────────────────────────────────────────────
void dwm3000_write_tx_data(dwm3000_inst_t *inst, const uint8_t *data, uint16_t len, uint16_t offset);
void dwm3000_set_tx_frame_len(dwm3000_inst_t *inst, uint16_t data_len_with_crc);
void dwm3000_start_tx(dwm3000_inst_t *inst, bool wait4resp);
void dwm3000_start_tx_delayed(dwm3000_inst_t *inst, uint64_t tx_time, bool wait4resp);

// ── RX primitives ───────────────────────────────────────────────
void dwm3000_start_rx(dwm3000_inst_t *inst, uint32_t timeout_uus);
void dwm3000_force_trx_off(dwm3000_inst_t *inst);

// ── Timestamps ──────────────────────────────────────────────────
uint64_t dwm3000_read_tx_timestamp(dwm3000_inst_t *inst);
uint64_t dwm3000_read_rx_timestamp(dwm3000_inst_t *inst);
uint64_t dwm3000_read_sys_time(dwm3000_inst_t *inst);

// ── Status ──────────────────────────────────────────────────────
uint64_t dwm3000_read_sys_status(dwm3000_inst_t *inst);
void     dwm3000_clear_sys_status(dwm3000_inst_t *inst, uint64_t mask);

// ── RX data ─────────────────────────────────────────────────────
uint16_t dwm3000_get_rx_frame_len(dwm3000_inst_t *inst);
void     dwm3000_read_rx_data(dwm3000_inst_t *inst, uint8_t *buf, uint16_t len, uint16_t offset);

// ── Antenna delay ───────────────────────────────────────────────
void dwm3000_set_antenna_delay(dwm3000_inst_t *inst, uint16_t tx_delay, uint16_t rx_delay);

// ── Reset ───────────────────────────────────────────────────────
void dwm3000_hard_reset(dwm3000_inst_t *inst);
void dwm3000_soft_reset(dwm3000_inst_t *inst);

#ifdef __cplusplus
}
#endif

#endif // DWM3000_H

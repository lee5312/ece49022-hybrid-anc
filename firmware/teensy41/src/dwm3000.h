#ifndef DWM3000_H
#define DWM3000_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DW_REG(file, off)  (((uint32_t)(file) << 16) | (uint16_t)(off))
#define DW_FILE(addr)      ((uint8_t)((addr) >> 16))
#define DW_OFF(addr)       ((uint16_t)((addr) & 0xFFFF))

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

#define DW_TX_ANTD          DW_REG(0x01, 0x04)
#define DW_TX_POWER         DW_REG(0x01, 0x0C)
#define DW_CHAN_CTRL        DW_REG(0x01, 0x14)

#define DW_DGC_CFG          DW_REG(0x03, 0x18)
#define DW_DGC_CFG0         DW_REG(0x03, 0x1C)
#define DW_DGC_CFG1         DW_REG(0x03, 0x20)
#define DW_DGC_LUT_0        DW_REG(0x03, 0x38)
#define DW_DGC_LUT_1        DW_REG(0x03, 0x3C)
#define DW_DGC_LUT_2        DW_REG(0x03, 0x40)
#define DW_DGC_LUT_3        DW_REG(0x03, 0x44)
#define DW_DGC_LUT_4        DW_REG(0x03, 0x48)
#define DW_DGC_LUT_5        DW_REG(0x03, 0x4C)
#define DW_DGC_LUT_6        DW_REG(0x03, 0x50)

#define DW_RX_CAL_CFG       DW_REG(0x04, 0x0C)
#define DW_RX_CAL_CFG_2     DW_REG(0x04, 0x0E)
#define DW_RX_CAL_RESI      DW_REG(0x04, 0x14)
#define DW_RX_CAL_RESQ      DW_REG(0x04, 0x1C)
#define DW_RX_CAL_STS       DW_REG(0x04, 0x20)

#define DW_DTUNE0           DW_REG(0x06, 0x00)
#define DW_RX_SFD_TOC       DW_REG(0x06, 0x02)
#define DW_PRE_TOC          DW_REG(0x06, 0x04)
#define DW_DTUNE3           DW_REG(0x06, 0x0C)

#define DW_TX_CTRL_LO       DW_REG(0x07, 0x18)
#define DW_RF_TX_CTRL_2     DW_REG(0x07, 0x1C)
#define DW_LDO_CTRL         DW_REG(0x07, 0x48)
#define DW_LDO_RLOAD        DW_REG(0x07, 0x51)

#define DW_PLL_CFG          DW_REG(0x09, 0x00)
#define DW_PLL_CAL          DW_REG(0x09, 0x08)

#define DW_CIA_CONF         DW_REG(0x0E, 0x00)
#define DW_SYS_STATE        DW_REG(0x0F, 0x30)
#define DW_SEQ_CTRL         DW_REG(0x11, 0x08)

#define DW_TX_BUFFER        DW_REG(0x14, 0x00)
#define DW_RX_BUFFER_0      DW_REG(0x12, 0x00)
#define DW_RX_BUFFER_1      DW_REG(0x13, 0x00)

#define DW_TXFRB_BIT        (1ULL << 4)
#define DW_TXPRS_BIT        (1ULL << 5)
#define DW_TXPHS_BIT        (1ULL << 6)
#define DW_TXFRS_BIT        (1ULL << 7)
#define DW_RXPRD_BIT        (1ULL << 8)
#define DW_RXSFDD_BIT       (1ULL << 9)
#define DW_RXPHD_BIT        (1ULL << 11)
#define DW_RXPHE_BIT        (1ULL << 12)
#define DW_RXFR_BIT         (1ULL << 13)
#define DW_RXFCG_BIT        (1ULL << 14)
#define DW_RXFCE_BIT        (1ULL << 15)
#define DW_RXFSL_BIT        (1ULL << 16)
#define DW_RXFTO_BIT        (1ULL << 17)
#define DW_RXOVRR_BIT       (1ULL << 20)
#define DW_RXPTO_BIT        (1ULL << 21)
#define DW_SPIRDY_BIT       (1ULL << 23)
#define DW_RXSTO_BIT        (1ULL << 26)

#define DW_ALL_TX_BITS      (DW_TXFRB_BIT | DW_TXPRS_BIT | DW_TXPHS_BIT | DW_TXFRS_BIT)
#define DW_ALL_RX_GOOD      (DW_RXFCG_BIT)
#define DW_ALL_RX_ERR       (DW_RXFCE_BIT | DW_RXPHE_BIT | DW_RXFTO_BIT | DW_RXSTO_BIT | \
                             DW_RXOVRR_BIT | DW_RXPTO_BIT | DW_RXFSL_BIT)
#define DW_ALL_RX_EVENTS    (DW_ALL_RX_GOOD | DW_ALL_RX_ERR)

#define DW_CMD_TXRXOFF      0x00
#define DW_CMD_TX           0x01
#define DW_CMD_RX           0x02
#define DW_CMD_DTX          0x03
#define DW_CMD_DRX          0x04
#define DW_CMD_TX_W4R       0x0C
#define DW_CMD_DTX_W4R      0x0D
#define DW_CMD_CLR_IRQS     0x12
#define DW_CMD_DB_TOGGLE    0x13

#define DW_TIME_UNITS       (1.0 / (128.0 * 499.2e6))
#define SPEED_OF_LIGHT_M_S  299702547.0
#define DW_DEFAULT_ANT_DLY  16385U
#define DW3000_DEV_ID       0xDECA0302U
#define DW3110_DEV_ID       0xDECA0312U

typedef struct {
    void *spi;
    uint8_t pin_cs;
    uint8_t pin_rst;
    uint8_t pin_irq;
    const char *label;
    bool ready;
    uint16_t tx_ant_dly;
    uint16_t rx_ant_dly;
} dwm3000_inst_t;

void dwm3000_spi_bus_init(void *spi, uint32_t baud,
                          uint8_t pin_sck, uint8_t pin_mosi, uint8_t pin_miso);
bool dwm3000_init(dwm3000_inst_t *inst);
void dwm3000_configure_default(dwm3000_inst_t *inst);

uint32_t dwm3000_read_dev_id(dwm3000_inst_t *inst);
void dwm3000_read_reg(dwm3000_inst_t *inst, uint32_t reg, uint8_t *buf, size_t len);
void dwm3000_write_reg(dwm3000_inst_t *inst, uint32_t reg, const uint8_t *buf, size_t len);
uint32_t dwm3000_read32(dwm3000_inst_t *inst, uint32_t reg);
void dwm3000_write32(dwm3000_inst_t *inst, uint32_t reg, uint32_t val);
uint32_t dwm3000_read24(dwm3000_inst_t *inst, uint32_t reg);
void dwm3000_write24(dwm3000_inst_t *inst, uint32_t reg, uint32_t val);
uint16_t dwm3000_read16(dwm3000_inst_t *inst, uint32_t reg);
void dwm3000_write16(dwm3000_inst_t *inst, uint32_t reg, uint16_t val);
uint8_t dwm3000_read8(dwm3000_inst_t *inst, uint32_t reg);
void dwm3000_write8(dwm3000_inst_t *inst, uint32_t reg, uint8_t val);

void dwm3000_cmd(dwm3000_inst_t *inst, uint8_t fast_cmd);
void dwm3000_write_tx_data(dwm3000_inst_t *inst, const uint8_t *data, uint16_t len, uint16_t offset);
void dwm3000_set_tx_frame_len(dwm3000_inst_t *inst, uint16_t data_len_with_crc);
void dwm3000_start_tx(dwm3000_inst_t *inst, bool wait4resp);
void dwm3000_start_tx_delayed(dwm3000_inst_t *inst, uint64_t tx_time, bool wait4resp);
void dwm3000_start_rx(dwm3000_inst_t *inst, uint32_t timeout_uus);
void dwm3000_force_trx_off(dwm3000_inst_t *inst);

uint64_t dwm3000_read_tx_timestamp(dwm3000_inst_t *inst);
uint64_t dwm3000_read_rx_timestamp(dwm3000_inst_t *inst);
uint64_t dwm3000_read_sys_time(dwm3000_inst_t *inst);
uint32_t dwm3000_read_sys_state(dwm3000_inst_t *inst);
uint64_t dwm3000_read_sys_status(dwm3000_inst_t *inst);
void dwm3000_clear_sys_status(dwm3000_inst_t *inst, uint64_t mask);
uint16_t dwm3000_get_rx_frame_len(dwm3000_inst_t *inst);
void dwm3000_read_rx_data(dwm3000_inst_t *inst, uint8_t *buf, uint16_t len, uint16_t offset);

void dwm3000_set_antenna_delay(dwm3000_inst_t *inst, uint16_t tx_delay, uint16_t rx_delay);
int32_t dwm3000_run_pgf_cal(dwm3000_inst_t *inst);
void dwm3000_hard_reset(dwm3000_inst_t *inst);
void dwm3000_soft_reset(dwm3000_inst_t *inst);

#ifdef __cplusplus
}
#endif

#endif

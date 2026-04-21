#include "dwm3000.h"

#include <Arduino.h>
#include <SPI.h>

#include "board_config.h"

namespace {

constexpr uint32_t UWB_SPI_BAUD_SLOW = 2UL * 1000UL * 1000UL;
constexpr uint32_t DW_SYS_CFG_FFEN_BIT = (1u << 0);
constexpr uint32_t DW_SYS_CFG_PHR_MODE_BIT = (1u << 4);
constexpr uint32_t DW_TX_FCTRL_TXBR_BIT = (1u << 10);
constexpr uint32_t DW_TX_FCTRL_TR_BIT = (1u << 11);
constexpr uint32_t DW_TX_FCTRL_TXPSR_MASK = (0xFu << 12);
constexpr uint16_t DW_CHAN_CTRL_RF_CHAN_CH9_BIT = (1u << 0);

uint32_t s_bus_baud_hz = UWB_SPI_BAUD_SLOW;

SPIClass *as_spi_bus(void *spi_ptr)
{
    return static_cast<SPIClass *>(spi_ptr);
}

SPISettings make_spi_settings(void)
{
    return SPISettings(s_bus_baud_hz, MSBFIRST, SPI_MODE0);
}

uint8_t txpsr_field_from_symbols(uint16_t preamble_symbols)
{
    switch (preamble_symbols) {
    case 32:
        return 0x4u;
    case 64:
        return 0x1u;
    case 128:
        return 0x5u;
    case 256:
        return 0x9u;
    case 512:
        return 0xDu;
    case 1024:
        return 0x2u;
    case 1536:
        return 0x6u;
    case 2048:
        return 0xAu;
    case 4096:
        return 0x3u;
    default:
        return 0x5u;
    }
}

uint16_t make_chan_ctrl_value(void)
{
    uint16_t value = 0;

    if (UWB_CHANNEL == 9U) {
        value |= DW_CHAN_CTRL_RF_CHAN_CH9_BIT;
    }

    value |= static_cast<uint16_t>((UWB_SFD_TYPE & 0x3u) << 1);
    value |= static_cast<uint16_t>((UWB_PREAMBLE_CODE & 0x1Fu) << 3);
    value |= static_cast<uint16_t>((UWB_PREAMBLE_CODE & 0x1Fu) << 8);
    return value;
}

void cs_select(dwm3000_inst_t *inst)
{
    digitalWrite(inst->pin_cs, LOW);
}

void cs_deselect(dwm3000_inst_t *inst)
{
    digitalWrite(inst->pin_cs, HIGH);
}

int build_header(uint32_t reg_addr, bool is_read, uint8_t *hdr)
{
    const uint8_t file = DW_FILE(reg_addr) & 0x1F;
    const uint16_t off = DW_OFF(reg_addr);

    if (off == 0) {
        hdr[0] = static_cast<uint8_t>(file << 1);
        if (!is_read) {
            hdr[0] |= 0x80u;
        }
        return 1;
    }

    hdr[0] = static_cast<uint8_t>(0x40u | (file << 1));
    if (!is_read) {
        hdr[0] |= 0x80u;
    }

    if (off <= 127) {
        hdr[1] = static_cast<uint8_t>(off & 0x7Fu);
        return 2;
    }

    hdr[1] = static_cast<uint8_t>(0x80u | (off & 0x7Fu));
    hdr[2] = static_cast<uint8_t>((off >> 7) & 0xFFu);
    return 3;
}

} // namespace

void dwm3000_spi_bus_init(void *spi, uint32_t baud,
                          uint8_t pin_sck, uint8_t pin_mosi, uint8_t pin_miso)
{
    SPIClass *bus = as_spi_bus(spi);
    s_bus_baud_hz = (baud > 0) ? baud : UWB_SPI_BAUD_SLOW;

    bus->setMISO(pin_miso);
    bus->setMOSI(pin_mosi);
    bus->setSCK(pin_sck);
    bus->begin();
}

void dwm3000_read_reg(dwm3000_inst_t *inst, uint32_t reg_addr,
                      uint8_t *buf, size_t len)
{
    uint8_t hdr[3];
    const int hdr_len = build_header(reg_addr, true, hdr);
    SPIClass *bus = as_spi_bus(inst->spi);

    bus->beginTransaction(make_spi_settings());
    cs_select(inst);
    for (int i = 0; i < hdr_len; i++) {
        bus->transfer(hdr[i]);
    }
    for (size_t i = 0; i < len; i++) {
        buf[i] = bus->transfer(0x00);
    }
    cs_deselect(inst);
    bus->endTransaction();
}

void dwm3000_write_reg(dwm3000_inst_t *inst, uint32_t reg_addr,
                       const uint8_t *buf, size_t len)
{
    uint8_t hdr[3];
    const int hdr_len = build_header(reg_addr, false, hdr);
    SPIClass *bus = as_spi_bus(inst->spi);

    bus->beginTransaction(make_spi_settings());
    cs_select(inst);
    for (int i = 0; i < hdr_len; i++) {
        bus->transfer(hdr[i]);
    }
    for (size_t i = 0; i < len; i++) {
        bus->transfer(buf[i]);
    }
    cs_deselect(inst);
    bus->endTransaction();
}

uint32_t dwm3000_read32(dwm3000_inst_t *inst, uint32_t reg_addr)
{
    uint8_t buf[4];
    dwm3000_read_reg(inst, reg_addr, buf, sizeof(buf));
    return static_cast<uint32_t>(buf[0]) |
           (static_cast<uint32_t>(buf[1]) << 8) |
           (static_cast<uint32_t>(buf[2]) << 16) |
           (static_cast<uint32_t>(buf[3]) << 24);
}

void dwm3000_write32(dwm3000_inst_t *inst, uint32_t reg_addr, uint32_t val)
{
    const uint8_t buf[4] = {
        static_cast<uint8_t>(val),
        static_cast<uint8_t>(val >> 8),
        static_cast<uint8_t>(val >> 16),
        static_cast<uint8_t>(val >> 24),
    };
    dwm3000_write_reg(inst, reg_addr, buf, sizeof(buf));
}

uint16_t dwm3000_read16(dwm3000_inst_t *inst, uint32_t reg_addr)
{
    uint8_t buf[2];
    dwm3000_read_reg(inst, reg_addr, buf, sizeof(buf));
    return static_cast<uint16_t>(buf[0]) |
           (static_cast<uint16_t>(buf[1]) << 8);
}

void dwm3000_write16(dwm3000_inst_t *inst, uint32_t reg_addr, uint16_t val)
{
    const uint8_t buf[2] = {
        static_cast<uint8_t>(val),
        static_cast<uint8_t>(val >> 8),
    };
    dwm3000_write_reg(inst, reg_addr, buf, sizeof(buf));
}

uint8_t dwm3000_read8(dwm3000_inst_t *inst, uint32_t reg_addr)
{
    uint8_t value = 0;
    dwm3000_read_reg(inst, reg_addr, &value, sizeof(value));
    return value;
}

void dwm3000_write8(dwm3000_inst_t *inst, uint32_t reg_addr, uint8_t val)
{
    dwm3000_write_reg(inst, reg_addr, &val, sizeof(val));
}

void dwm3000_cmd(dwm3000_inst_t *inst, uint8_t fast_cmd)
{
    const uint8_t byte = static_cast<uint8_t>((fast_cmd << 1) | 0x81u);
    SPIClass *bus = as_spi_bus(inst->spi);

    bus->beginTransaction(make_spi_settings());
    cs_select(inst);
    bus->transfer(byte);
    cs_deselect(inst);
    bus->endTransaction();
}

void dwm3000_hard_reset(dwm3000_inst_t *inst)
{
    digitalWrite(inst->pin_rst, LOW);
    delay(2);
    digitalWrite(inst->pin_rst, HIGH);
    delay(10);
}

void dwm3000_soft_reset(dwm3000_inst_t *inst)
{
    dwm3000_hard_reset(inst);
}

uint32_t dwm3000_read_dev_id(dwm3000_inst_t *inst)
{
    return dwm3000_read32(inst, DW_DEV_ID);
}

bool dwm3000_init(dwm3000_inst_t *inst)
{
    pinMode(inst->pin_cs, OUTPUT);
    digitalWrite(inst->pin_cs, HIGH);

    pinMode(inst->pin_rst, OUTPUT);
    digitalWrite(inst->pin_rst, HIGH);

    pinMode(inst->pin_irq, INPUT);

    inst->ready = false;
    dwm3000_hard_reset(inst);

    const uint32_t dev_id = dwm3000_read_dev_id(inst);
    if (((dev_id >> 16) != 0xDECAu) || (((dev_id >> 8) & 0xFFu) != 0x03u)) {
        return false;
    }

    dwm3000_cmd(inst, DW_CMD_CLR_IRQS);
    inst->ready = true;
    return true;
}

void dwm3000_configure_default(dwm3000_inst_t *inst)
{
    uint32_t sys_cfg = dwm3000_read32(inst, DW_SYS_CFG);
    sys_cfg &= ~(DW_SYS_CFG_FFEN_BIT | DW_SYS_CFG_PHR_MODE_BIT);
    dwm3000_write32(inst, DW_SYS_CFG, sys_cfg);

    uint32_t tx_fctrl = dwm3000_read32(inst, DW_TX_FCTRL);
    tx_fctrl &= ~(DW_TX_FCTRL_TXBR_BIT | DW_TX_FCTRL_TR_BIT | DW_TX_FCTRL_TXPSR_MASK);
    tx_fctrl |= DW_TX_FCTRL_TXBR_BIT;
    tx_fctrl |= DW_TX_FCTRL_TR_BIT;
    tx_fctrl |= static_cast<uint32_t>(txpsr_field_from_symbols(UWB_PREAMBLE_LENGTH)) << 12;
    dwm3000_write32(inst, DW_TX_FCTRL, tx_fctrl);

    dwm3000_write16(inst, DW_TX_FCTRL_HI, 0u);
    dwm3000_write16(inst, DW_CHAN_CTRL, make_chan_ctrl_value());
    dwm3000_set_antenna_delay(inst, DW_DEFAULT_ANT_DLY, DW_DEFAULT_ANT_DLY);
    dwm3000_write32(inst, DW_PANADR, 0x0000DECAu);
}

void dwm3000_set_antenna_delay(dwm3000_inst_t *inst,
                               uint16_t tx_delay, uint16_t rx_delay)
{
    dwm3000_write16(inst, DW_TX_ANTD, tx_delay);

    uint32_t cia = dwm3000_read32(inst, DW_CIA_CONF);
    cia = (cia & 0xFFFF0000u) | static_cast<uint32_t>(rx_delay);
    dwm3000_write32(inst, DW_CIA_CONF, cia);
}

void dwm3000_write_tx_data(dwm3000_inst_t *inst,
                           const uint8_t *data, uint16_t len, uint16_t offset)
{
    uint32_t reg = DW_TX_BUFFER;
    if (offset > 0) {
        reg = DW_REG(DW_FILE(DW_TX_BUFFER), DW_OFF(DW_TX_BUFFER) + offset);
    }
    dwm3000_write_reg(inst, reg, data, len);
}

void dwm3000_set_tx_frame_len(dwm3000_inst_t *inst, uint16_t data_len_with_crc)
{
    uint32_t fctrl = dwm3000_read32(inst, DW_TX_FCTRL);
    fctrl &= ~0x000003FFu;
    fctrl |= static_cast<uint32_t>(data_len_with_crc & 0x03FFu);
    dwm3000_write32(inst, DW_TX_FCTRL, fctrl);
}

void dwm3000_start_tx(dwm3000_inst_t *inst, bool wait4resp)
{
    dwm3000_cmd(inst, wait4resp ? DW_CMD_TX_W4R : DW_CMD_TX);
}

void dwm3000_start_tx_delayed(dwm3000_inst_t *inst,
                              uint64_t tx_time, bool wait4resp)
{
    const uint32_t dx = static_cast<uint32_t>(tx_time >> 8);
    dwm3000_write32(inst, DW_DX_TIME, dx);
    dwm3000_cmd(inst, wait4resp ? DW_CMD_DTX_W4R : DW_CMD_DTX);
}

void dwm3000_start_rx(dwm3000_inst_t *inst, uint32_t timeout_uus)
{
    uint32_t cfg = dwm3000_read32(inst, DW_SYS_CFG);
    if (timeout_uus > 0) {
        dwm3000_write32(inst, DW_RX_FWTO, timeout_uus);
        cfg |= (1u << 9);
    } else {
        cfg &= ~(1u << 9);
    }
    dwm3000_write32(inst, DW_SYS_CFG, cfg);
    dwm3000_cmd(inst, DW_CMD_RX);
}

void dwm3000_force_trx_off(dwm3000_inst_t *inst)
{
    dwm3000_cmd(inst, DW_CMD_TXRXOFF);
}

uint64_t dwm3000_read_sys_status(dwm3000_inst_t *inst)
{
    uint8_t buf[6];
    dwm3000_read_reg(inst, DW_SYS_STATUS, buf, sizeof(buf));
    return static_cast<uint64_t>(buf[0]) |
           (static_cast<uint64_t>(buf[1]) << 8) |
           (static_cast<uint64_t>(buf[2]) << 16) |
           (static_cast<uint64_t>(buf[3]) << 24) |
           (static_cast<uint64_t>(buf[4]) << 32) |
           (static_cast<uint64_t>(buf[5]) << 40);
}

void dwm3000_clear_sys_status(dwm3000_inst_t *inst, uint64_t mask)
{
    const uint8_t buf[6] = {
        static_cast<uint8_t>(mask),
        static_cast<uint8_t>(mask >> 8),
        static_cast<uint8_t>(mask >> 16),
        static_cast<uint8_t>(mask >> 24),
        static_cast<uint8_t>(mask >> 32),
        static_cast<uint8_t>(mask >> 40),
    };
    dwm3000_write_reg(inst, DW_SYS_STATUS, buf, sizeof(buf));
}

uint64_t dwm3000_read_tx_timestamp(dwm3000_inst_t *inst)
{
    uint8_t buf[5];
    dwm3000_read_reg(inst, DW_TX_TIME, buf, sizeof(buf));
    return static_cast<uint64_t>(buf[0]) |
           (static_cast<uint64_t>(buf[1]) << 8) |
           (static_cast<uint64_t>(buf[2]) << 16) |
           (static_cast<uint64_t>(buf[3]) << 24) |
           (static_cast<uint64_t>(buf[4]) << 32);
}

uint64_t dwm3000_read_rx_timestamp(dwm3000_inst_t *inst)
{
    uint8_t buf[5];
    dwm3000_read_reg(inst, DW_RX_TIME, buf, sizeof(buf));
    return static_cast<uint64_t>(buf[0]) |
           (static_cast<uint64_t>(buf[1]) << 8) |
           (static_cast<uint64_t>(buf[2]) << 16) |
           (static_cast<uint64_t>(buf[3]) << 24) |
           (static_cast<uint64_t>(buf[4]) << 32);
}

uint64_t dwm3000_read_sys_time(dwm3000_inst_t *inst)
{
    const uint32_t time32 = dwm3000_read32(inst, DW_SYS_TIME);
    return static_cast<uint64_t>(time32) << 8;
}

uint16_t dwm3000_get_rx_frame_len(dwm3000_inst_t *inst)
{
    const uint32_t finfo = dwm3000_read32(inst, DW_RX_FINFO);
    return static_cast<uint16_t>(finfo & 0x03FFu);
}

void dwm3000_read_rx_data(dwm3000_inst_t *inst,
                          uint8_t *buf, uint16_t len, uint16_t offset)
{
    uint32_t reg = DW_RX_BUFFER_0;
    if (offset > 0) {
        reg = DW_REG(DW_FILE(DW_RX_BUFFER_0), DW_OFF(DW_RX_BUFFER_0) + offset);
    }
    dwm3000_read_reg(inst, reg, buf, len);
}

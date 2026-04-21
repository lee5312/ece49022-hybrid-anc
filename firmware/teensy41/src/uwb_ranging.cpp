#include "uwb_ranging.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

namespace {

uint8_t s_poll_msg[TWR_POLL_MSG_LEN - 2] = {
    0x41, 0x88,
    0x00,
    0xCA, 0xDE,
    0x00, 0x00,
    0x00, 0x00,
    TWR_FUNC_POLL,
};

uint8_t s_resp_msg[TWR_RESP_MSG_LEN - 2] = {
    0x41, 0x88,
    0x00,
    0xCA, 0xDE,
    0x00, 0x00,
    0x00, 0x00,
    TWR_FUNC_RESP,
    0, 0, 0, 0,
    0, 0, 0, 0,
};

uint8_t s_seq_num = 0;

void put_le16(uint8_t *buf, uint16_t val)
{
    buf[0] = static_cast<uint8_t>(val);
    buf[1] = static_cast<uint8_t>(val >> 8);
}

uint16_t get_le16(const uint8_t *buf)
{
    return static_cast<uint16_t>(buf[0]) |
           (static_cast<uint16_t>(buf[1]) << 8);
}

void put_le32(uint8_t *buf, uint32_t val)
{
    buf[0] = static_cast<uint8_t>(val);
    buf[1] = static_cast<uint8_t>(val >> 8);
    buf[2] = static_cast<uint8_t>(val >> 16);
    buf[3] = static_cast<uint8_t>(val >> 24);
}

uint32_t get_le32(const uint8_t *buf)
{
    return static_cast<uint32_t>(buf[0]) |
           (static_cast<uint32_t>(buf[1]) << 8) |
           (static_cast<uint32_t>(buf[2]) << 16) |
           (static_cast<uint32_t>(buf[3]) << 24);
}

uint64_t poll_status(dwm3000_inst_t *inst, uint64_t wait_mask, uint32_t timeout_ms)
{
    const uint32_t start_ms = millis();
    while ((millis() - start_ms) < timeout_ms) {
        const uint64_t status = dwm3000_read_sys_status(inst);
        if (status & wait_mask) {
            return status;
        }
        yield();
    }
    return 0;
}

} // namespace

twr_result_t twr_initiate(dwm3000_inst_t *inst,
                          uint16_t my_addr, uint16_t remote_addr)
{
    twr_result_t result = { false, 0.0f, 0.0f, remote_addr, TWR_STATUS_TX_TIMEOUT };

    dwm3000_force_trx_off(inst);
    dwm3000_clear_sys_status(inst, DW_ALL_RX_EVENTS | DW_ALL_TX_BITS);

    s_poll_msg[2] = s_seq_num++;
    put_le16(&s_poll_msg[5], remote_addr);
    put_le16(&s_poll_msg[7], my_addr);

    dwm3000_write_tx_data(inst, s_poll_msg, sizeof(s_poll_msg), 0);
    dwm3000_set_tx_frame_len(inst, TWR_POLL_MSG_LEN);
    dwm3000_start_tx(inst, true);

    uint64_t status = poll_status(inst, DW_TXFRS_BIT, 10);
    if (!(status & DW_TXFRS_BIT)) {
        dwm3000_force_trx_off(inst);
        result.status = TWR_STATUS_TX_TIMEOUT;
        return result;
    }

    const uint64_t t1 = dwm3000_read_tx_timestamp(inst);
    dwm3000_clear_sys_status(inst, DW_ALL_TX_BITS);

    status = poll_status(inst, DW_ALL_RX_GOOD | DW_ALL_RX_ERR, 20);
    if (!(status & DW_RXFCG_BIT)) {
        dwm3000_force_trx_off(inst);
        dwm3000_clear_sys_status(inst, DW_ALL_RX_EVENTS | DW_ALL_TX_BITS);
        result.status = TWR_STATUS_RESP_RX_TIMEOUT;
        return result;
    }

    const uint64_t t4 = dwm3000_read_rx_timestamp(inst);
    uint8_t rx_buf[TWR_RESP_MSG_LEN];
    uint16_t rx_len = dwm3000_get_rx_frame_len(inst);
    if (rx_len > sizeof(rx_buf)) {
        rx_len = sizeof(rx_buf);
    }
    dwm3000_read_rx_data(inst, rx_buf, rx_len, 0);

    dwm3000_clear_sys_status(inst, DW_ALL_RX_EVENTS);
    dwm3000_force_trx_off(inst);

    if ((rx_len < (TWR_RESP_MSG_LEN - 2)) || (rx_buf[9] != TWR_FUNC_RESP)) {
        result.status = TWR_STATUS_RESP_FRAME_ERROR;
        return result;
    }

    if (get_le16(&rx_buf[7]) != remote_addr) {
        result.status = TWR_STATUS_RESP_ADDR_MISMATCH;
        return result;
    }

    const uint32_t t2_lo = get_le32(&rx_buf[10]);
    const uint32_t t3_lo = get_le32(&rx_buf[14]);
    const uint32_t t1_lo = static_cast<uint32_t>(t1 & 0xFFFFFFFFu);
    const uint32_t t4_lo = static_cast<uint32_t>(t4 & 0xFFFFFFFFu);

    const int32_t t_round = static_cast<int32_t>(t4_lo - t1_lo);
    const int32_t t_reply = static_cast<int32_t>(t3_lo - t2_lo);
    const double tof_ticks = (static_cast<double>(t_round) - static_cast<double>(t_reply)) / 2.0;
    double dist = tof_ticks * DW_TIME_UNITS * SPEED_OF_LIGHT_M_S;

    bool clamped = false;
    if (dist < 0.0) {
        dist = 0.0;
        clamped = true;
    }
    if (dist > 100.0) {
        dist = 100.0;
        clamped = true;
    }

    result.valid = true;
    result.distance_m = static_cast<float>(dist);
    result.status = clamped ? TWR_STATUS_RANGE_CLAMPED : TWR_STATUS_OK;
    return result;
}

bool twr_respond(dwm3000_inst_t *inst,
                 uint16_t my_addr, uint32_t timeout_uus)
{
    dwm3000_force_trx_off(inst);
    dwm3000_clear_sys_status(inst, DW_ALL_RX_EVENTS | DW_ALL_TX_BITS);

    dwm3000_start_rx(inst, timeout_uus);

    uint64_t status = poll_status(inst, DW_ALL_RX_GOOD | DW_ALL_RX_ERR, 1000);
    if (!(status & DW_RXFCG_BIT)) {
        dwm3000_force_trx_off(inst);
        dwm3000_clear_sys_status(inst, DW_ALL_RX_EVENTS);
        return false;
    }

    const uint64_t t2 = dwm3000_read_rx_timestamp(inst);
    uint8_t rx_buf[TWR_POLL_MSG_LEN];
    uint16_t rx_len = dwm3000_get_rx_frame_len(inst);
    if (rx_len > sizeof(rx_buf)) {
        rx_len = sizeof(rx_buf);
    }
    dwm3000_read_rx_data(inst, rx_buf, rx_len, 0);
    dwm3000_clear_sys_status(inst, DW_ALL_RX_EVENTS);

    if ((rx_len < (TWR_POLL_MSG_LEN - 2)) || (rx_buf[9] != TWR_FUNC_POLL)) {
        dwm3000_force_trx_off(inst);
        return false;
    }

    const uint16_t dst_addr = get_le16(&rx_buf[5]);
    if ((dst_addr != my_addr) && (dst_addr != 0xFFFFu)) {
        dwm3000_force_trx_off(inst);
        return false;
    }

    const uint16_t initiator_addr = get_le16(&rx_buf[7]);
    const uint64_t t3 = (t2 + TWR_RESP_DELAY_DWT) & 0xFFFFFFFE00ULL;

    s_resp_msg[2] = rx_buf[2];
    put_le16(&s_resp_msg[5], initiator_addr);
    put_le16(&s_resp_msg[7], my_addr);
    put_le32(&s_resp_msg[10], static_cast<uint32_t>(t2 & 0xFFFFFFFFu));
    put_le32(&s_resp_msg[14], static_cast<uint32_t>(t3 & 0xFFFFFFFFu));

    dwm3000_write_tx_data(inst, s_resp_msg, sizeof(s_resp_msg), 0);
    dwm3000_set_tx_frame_len(inst, TWR_RESP_MSG_LEN);
    dwm3000_start_tx_delayed(inst, t3, false);

    status = poll_status(inst, DW_TXFRS_BIT, 10);
    dwm3000_clear_sys_status(inst, DW_ALL_TX_BITS);

    if (!(status & DW_TXFRS_BIT)) {
        dwm3000_force_trx_off(inst);
        return false;
    }

    return true;
}

twr_result_t twr_loopback(dwm3000_inst_t *initiator, uint16_t init_addr,
                          dwm3000_inst_t *responder, uint16_t resp_addr)
{
    twr_result_t result = { false, 0.0f, 0.0f, resp_addr, TWR_STATUS_TX_TIMEOUT };

    dwm3000_force_trx_off(initiator);
    dwm3000_force_trx_off(responder);
    dwm3000_clear_sys_status(initiator, DW_ALL_RX_EVENTS | DW_ALL_TX_BITS);
    dwm3000_clear_sys_status(responder, DW_ALL_RX_EVENTS | DW_ALL_TX_BITS);

    dwm3000_start_rx(responder, TWR_RX_TIMEOUT_UUS);

    s_poll_msg[2] = s_seq_num++;
    put_le16(&s_poll_msg[5], resp_addr);
    put_le16(&s_poll_msg[7], init_addr);

    dwm3000_write_tx_data(initiator, s_poll_msg, sizeof(s_poll_msg), 0);
    dwm3000_set_tx_frame_len(initiator, TWR_POLL_MSG_LEN);
    dwm3000_start_tx(initiator, true);

    uint64_t status = poll_status(initiator, DW_TXFRS_BIT, 10);
    if (!(status & DW_TXFRS_BIT)) {
        dwm3000_force_trx_off(initiator);
        dwm3000_force_trx_off(responder);
        result.status = TWR_STATUS_TX_TIMEOUT;
        return result;
    }

    const uint64_t t1 = dwm3000_read_tx_timestamp(initiator);
    dwm3000_clear_sys_status(initiator, DW_ALL_TX_BITS);

    status = poll_status(responder, DW_ALL_RX_GOOD | DW_ALL_RX_ERR, 10);
    if (!(status & DW_RXFCG_BIT)) {
        dwm3000_force_trx_off(initiator);
        dwm3000_force_trx_off(responder);
        dwm3000_clear_sys_status(responder, DW_ALL_RX_EVENTS);
        result.status = TWR_STATUS_RESP_RX_TIMEOUT;
        return result;
    }

    const uint64_t t2 = dwm3000_read_rx_timestamp(responder);
    dwm3000_clear_sys_status(responder, DW_ALL_RX_EVENTS);

    const uint64_t t3 = (t2 + TWR_RESP_DELAY_DWT) & 0xFFFFFFFE00ULL;

    s_resp_msg[2] = s_poll_msg[2];
    put_le16(&s_resp_msg[5], init_addr);
    put_le16(&s_resp_msg[7], resp_addr);
    put_le32(&s_resp_msg[10], static_cast<uint32_t>(t2 & 0xFFFFFFFFu));
    put_le32(&s_resp_msg[14], static_cast<uint32_t>(t3 & 0xFFFFFFFFu));

    dwm3000_write_tx_data(responder, s_resp_msg, sizeof(s_resp_msg), 0);
    dwm3000_set_tx_frame_len(responder, TWR_RESP_MSG_LEN);
    dwm3000_start_tx_delayed(responder, t3, false);

    status = poll_status(responder, DW_TXFRS_BIT, 10);
    if (!(status & DW_TXFRS_BIT)) {
        dwm3000_force_trx_off(initiator);
        dwm3000_force_trx_off(responder);
        dwm3000_clear_sys_status(responder, DW_ALL_TX_BITS);
        result.status = TWR_STATUS_RESP_TX_TIMEOUT;
        return result;
    }
    dwm3000_clear_sys_status(responder, DW_ALL_TX_BITS);

    status = poll_status(initiator, DW_ALL_RX_GOOD | DW_ALL_RX_ERR, 20);
    if (!(status & DW_RXFCG_BIT)) {
        dwm3000_force_trx_off(initiator);
        dwm3000_force_trx_off(responder);
        dwm3000_clear_sys_status(initiator, DW_ALL_RX_EVENTS | DW_ALL_TX_BITS);
        result.status = TWR_STATUS_INIT_RX_TIMEOUT;
        return result;
    }

    const uint64_t t4 = dwm3000_read_rx_timestamp(initiator);
    uint8_t rx_buf[TWR_RESP_MSG_LEN];
    uint16_t rx_len = dwm3000_get_rx_frame_len(initiator);
    if (rx_len > sizeof(rx_buf)) {
        rx_len = sizeof(rx_buf);
    }
    dwm3000_read_rx_data(initiator, rx_buf, rx_len, 0);
    dwm3000_clear_sys_status(initiator, DW_ALL_RX_EVENTS);
    dwm3000_force_trx_off(initiator);
    dwm3000_force_trx_off(responder);

    if ((rx_len < (TWR_RESP_MSG_LEN - 2)) || (rx_buf[9] != TWR_FUNC_RESP)) {
        result.status = TWR_STATUS_INIT_FRAME_ERROR;
        return result;
    }

    if (get_le16(&rx_buf[7]) != resp_addr) {
        result.status = TWR_STATUS_RESP_ADDR_MISMATCH;
        return result;
    }

    const uint32_t t1_lo = static_cast<uint32_t>(t1 & 0xFFFFFFFFu);
    const uint32_t t2_lo = static_cast<uint32_t>(t2 & 0xFFFFFFFFu);
    const uint32_t t3_lo = static_cast<uint32_t>(t3 & 0xFFFFFFFFu);
    const uint32_t t4_lo = static_cast<uint32_t>(t4 & 0xFFFFFFFFu);

    const int32_t t_round = static_cast<int32_t>(t4_lo - t1_lo);
    const int32_t t_reply = static_cast<int32_t>(t3_lo - t2_lo);
    const double tof_ticks = (static_cast<double>(t_round) - static_cast<double>(t_reply)) / 2.0;
    double dist = tof_ticks * DW_TIME_UNITS * SPEED_OF_LIGHT_M_S;

    bool clamped = false;
    if (dist < 0.0) {
        dist = 0.0;
        clamped = true;
    }
    if (dist > 100.0) {
        dist = 100.0;
        clamped = true;
    }

    result.valid = true;
    result.distance_m = static_cast<float>(dist);
    result.status = clamped ? TWR_STATUS_RANGE_CLAMPED : TWR_STATUS_OK;
    return result;
}

/**
 * uwb_ranging.h — Single-Sided Two-Way Ranging (SS-TWR) protocol
 *
 * Multi-instance: all functions take a dwm3000_inst_t* so the
 * headset can range from each of its 3 UWBs to the single tool.
 *
 * SS-TWR timing diagram:
 *
 *   Initiator (headset UWB)            Responder (tool UWB)
 *   ────────────────────               ────────────────────
 *   Poll TX  ────────────────────────>  Poll RX   (T2)
 *   (T1)                                │
 *                                       │ reply delay
 *                                       v
 *   Resp RX  <────────────────────────  Resp TX   (T3)
 *   (T4)
 *
 *   Distance = c × (T_round - T_reply) / 2
 *   where T_round = T4 - T1,  T_reply = T3 - T2
 */

#ifndef UWB_RANGING_H
#define UWB_RANGING_H

#include <stdint.h>
#include <stdbool.h>
#include "dwm3000.h"

#ifdef __cplusplus
extern "C" {
#endif

// ================================================================
//  TWR protocol constants
// ================================================================

// Responder reply delay (DW3000 time units, ≈ 400 μs)
#define TWR_RESP_DELAY_UUS   400
#define TWR_UUS_TO_DWT       63898ULL
#define TWR_RESP_DELAY_DWT   (TWR_RESP_DELAY_UUS * TWR_UUS_TO_DWT)

// RX timeout after sending Poll, waiting for Response (μs units)
#define TWR_RX_TIMEOUT_UUS   5000     // 5 ms

// ── IEEE 802.15.4 frame constants ───────────────────────────────
#define TWR_FC_DATA          0x8841   // Data frame, PAN ID compress, short addr
#define TWR_PAN_ID           0xDECA   // PAN identifier
#define TWR_FUNC_POLL        0x21     // Function code: Poll
#define TWR_FUNC_RESP        0x10     // Function code: Response

// Frame sizes (including 2-byte FCS auto-appended by DW3000)
#define TWR_POLL_MSG_LEN     12       // FC(2)+Seq(1)+PAN(2)+Dst(2)+Src(2)+Func(1)+FCS(2)
#define TWR_RESP_MSG_LEN     20       // above + poll_rx_ts(4) + resp_tx_ts(4)

// ================================================================
//  Ranging result
// ================================================================

typedef enum {
    TWR_STATUS_OK = 0,
    TWR_STATUS_TX_TIMEOUT = 1,
    TWR_STATUS_RESP_RX_TIMEOUT = 2,
    TWR_STATUS_RESP_FRAME_ERROR = 3,
    TWR_STATUS_RESP_ADDR_MISMATCH = 4,
    TWR_STATUS_RESP_TX_TIMEOUT = 5,
    TWR_STATUS_INIT_RX_TIMEOUT = 6,
    TWR_STATUS_INIT_FRAME_ERROR = 7,
    TWR_STATUS_RANGE_CLAMPED = 8,
    TWR_STATUS_NOT_READY = 9,
} twr_status_t;

typedef struct {
    bool         valid;          // true if ranging succeeded
    float        distance_m;     // measured distance in metres
    float        rssi_est;       // rough RSSI estimate (dBm), 0 if unavailable
    uint16_t     remote_addr;    // address of the remote node
    twr_status_t status;         // success/failure cause for bring-up
} twr_result_t;

// ================================================================
//  Initiator API  (runs on headset — one call per UWB module)
// ================================================================

/**
 * Perform one SS-TWR exchange with a specific remote node.
 * This is a BLOCKING call (waits for response or timeout).
 *
 * @param inst         DWM3000 instance to use (T, L, or R on headset)
 * @param my_addr      Short address assigned to this UWB module
 * @param remote_addr  Short address of the remote (tool) UWB
 * @return             Ranging result (check .valid)
 */
twr_result_t twr_initiate(dwm3000_inst_t *inst,
                           uint16_t my_addr, uint16_t remote_addr);

// ================================================================
//  Responder API  (runs on tool's MCU — or loopback)
// ================================================================

/**
 * Listen for a Poll and automatically respond.
 * Blocking until a valid exchange completes or timeout.
 *
 * @param inst          DWM3000 instance
 * @param my_addr       This node's short address
 * @param timeout_uus   RX listen timeout (0 = wait forever)
 * @return              true if a Poll was serviced
 */
bool twr_respond(dwm3000_inst_t *inst,
                 uint16_t my_addr, uint32_t timeout_uus);

// ================================================================
//  Loopback API  (initiator + responder on same MCU, shared SPI bus)
// ================================================================

/**
 * Perform SS-TWR with both sides driven by the same MCU.
 * The MCU orchestrates the exchange by talking to each DW3000
 * via different CS pins on the shared SPI bus.
 *
 * Sequence:
 *   1. Arm responder RX
 *   2. TX Poll from initiator (with W4R)
 *   3. Service responder: read poll_rx_ts, build response, delayed TX
 *   4. Wait for initiator to receive response, compute distance
 *
 * @param initiator      Headset UWB instance (T, L, or R)
 * @param init_addr      Short address of initiator
 * @param responder      Tool UWB instance
 * @param resp_addr      Short address of responder
 * @return               Ranging result (check .valid)
 */
twr_result_t twr_loopback(dwm3000_inst_t *initiator, uint16_t init_addr,
                           dwm3000_inst_t *responder, uint16_t resp_addr);

#ifdef __cplusplus
}
#endif

#endif // UWB_RANGING_H

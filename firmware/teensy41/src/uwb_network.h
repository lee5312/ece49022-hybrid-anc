/**
 * uwb_network.h — Headset UWB ranging network (reverse trilateration)
 *
 * Topology (all orchestrated by the Teensy 4.1 via shared SPI1 bus):
 *   3× DWM3000 headset modules (T / L / R)  — initiators
 *   1× DWM3000 tool module                  — responder
 *
 * Each headset UWB initiates SS-TWR loopback to the tool UWB:
 *
 *   UWB_T ↔ Tool  |  UWB_L ↔ Tool  |  UWB_R ↔ Tool
 *
 * The MCU orchestrates both sides since all 4 DW3000s share
 * one SPI bus with different CS pins.
 *
 * Produces 3 range measurements per cycle (~15–20 Hz).
 */

#ifndef UWB_NETWORK_H
#define UWB_NETWORK_H

#include <stdint.h>
#include <stdbool.h>
#include "dwm3000.h"

#ifdef __cplusplus
extern "C" {
#endif

// ================================================================
//  Network topology
// ================================================================

#define UWB_NUM_HEADSET     3       // UWBs on the headset

// Headset UWB indices (used as array indices)
#define UWB_IDX_T           0       // Top
#define UWB_IDX_L           1       // Left
#define UWB_IDX_R           2       // Right

// Default short addresses
#define UWB_ADDR_T          0x0001  // Headset Top UWB
#define UWB_ADDR_L          0x0002  // Headset Left UWB
#define UWB_ADDR_R          0x0003  // Headset Right UWB
#define UWB_TOOL_ADDR       0x0010  // Tool (responder)
#define UWB_RANGE_MASK_ALL  0x07u   // T | L | R

// ================================================================
//  Ranging cycle result
// ================================================================

typedef struct {
    float    range_m[UWB_NUM_HEADSET];     // distance from each UWB to tool (m)
    bool     valid[UWB_NUM_HEADSET];       // per-UWB validity flag
    uint32_t range_timestamp_us[UWB_NUM_HEADSET]; // completion time of each exchange
    uint32_t timestamp_us;                 // system time at cycle end
    uint8_t  num_valid;                    // count of valid measurements
} uwb_cycle_result_t;

typedef struct {
    uint32_t total_attempts;
    uint32_t total_successes;
    uint32_t total_failures;
    uint32_t consecutive_failures;
    uint32_t last_attempt_timestamp_us;
    uint32_t last_success_timestamp_us;
    float    last_distance_m;
    uint8_t  last_attempts_used;
    uint8_t  last_status;              // twr_status_t encoded as uint8_t
    bool     last_valid;
} uwb_link_diag_t;

// ================================================================
//  API
// ================================================================

/**
 * Initialize UWB network with 3 headset + 1 tool instances.
 * Must call dwm3000_spi_bus_init() and dwm3000_init() for each
 * instance first.
 *
 * @param t     Pointer to the Top UWB instance
 * @param l     Pointer to the Left UWB instance
 * @param r     Pointer to the Right UWB instance
 * @param tool  Pointer to the Tool UWB instance (responder)
 */
void uwb_net_init(dwm3000_inst_t *t, dwm3000_inst_t *l,
                  dwm3000_inst_t *r, dwm3000_inst_t *tool);

/**
 * Set UWB body-frame position (for ESKF measurement model).
 *
 * @param uwb_idx  UWB_IDX_T, UWB_IDX_L, or UWB_IDX_R
 * @param x, y, z  Body-frame coordinates (meters) relative to headset origin
 */
void uwb_net_set_uwb_body_pos(uint8_t uwb_idx, float x, float y, float z);

/**
 * Perform one full TDMA ranging cycle.
 * Sequentially ranges from UWB_T, UWB_L, UWB_R to the tool.
 * Blocking — takes ~3 × (TX + reply + RX) ≈ 3–5 ms.
 *
 * @return  Cycle result with up to 3 range measurements.
 */
uwb_cycle_result_t uwb_net_range_cycle(void);

/**
 * Perform a masked ranging cycle.
 *
 * @param range_mask Bitmask of initiators to run, bit0=T bit1=L bit2=R.
 */
uwb_cycle_result_t uwb_net_range_masked(uint8_t range_mask);

void uwb_net_set_retry_limit(uint8_t retries);
void uwb_net_get_link_diag(uint8_t uwb_idx, uwb_link_diag_t *diag);

/**
 * Get the body-frame position of a headset UWB module.
 */
void uwb_net_get_uwb_body_pos(uint8_t uwb_idx, float *x, float *y, float *z);

#ifdef __cplusplus
}
#endif

#endif // UWB_NETWORK_H

#ifndef TF_MANAGER_H
#define TF_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "lms_filter.h"

// --- World Configuration (same as before) ---
#define WORLD_X_METERS   5.0f
#define WORLD_Y_METERS   5.0f
#define WORLD_Z_METERS   3.0f
#define GRID_RESOLUTION_CM 50
#define GRID_X_SIZE ((int)(WORLD_X_METERS * 100 / GRID_RESOLUTION_CM))
#define GRID_Y_SIZE ((int)(WORLD_Y_METERS * 100 / GRID_RESOLUTION_CM))
#define GRID_Z_SIZE ((int)(WORLD_Z_METERS * 100 / GRID_RESOLUTION_CM))

#define LMS_TAPS 1

// --- Structures ---
typedef struct {
    bool is_valid;
    float coefficients[1];
} SpatialTF;

// Status codes returned by the process function
typedef enum {
    SPATIAL_STATUS_NO_CHANGE,       // Device is still in the same grid cell
    SPATIAL_STATUS_INITIALIZED,     // First time a valid coordinate has been processed
    SPATIAL_STATUS_BOUNDARY_CROSSED // Device moved to a new cell; save/fetch was performed
} SpatialStatus;


// --- Function Prototypes ---

/**
 * @brief Initializes the spatial map manager. Call once at startup.
 */
void spatial_map_init(void);

/**
 * @brief This is the main processing function to be called continuously from the main loop.
 *        It automatically handles saving and fetching transfer functions when the device
 *        crosses the boundary of a grid cell.
 * 
 * @param x_cm The current real-world X coordinate in centimeters.
 * @param y_cm The current real-world Y coordinate in centimeters.
 * @param z_cm The current real-world Z coordinate in centimeters.
 * @param lms_filter A pointer to the currently active LMS filter.
 * @return A SpatialStatus code indicating what action was taken.
 */
SpatialStatus spatial_map_process_movement(float x_cm, float y_cm, float z_cm, LMSFilter* lms_filter);

#endif // TF_MANAGER_H

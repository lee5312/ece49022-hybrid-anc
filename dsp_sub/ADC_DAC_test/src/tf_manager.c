#include "tf_manager.h"
#include <string.h>

// --- Static (Private) Global Variables ---
static SpatialTF g_spatial_map[GRID_X_SIZE][GRID_Y_SIZE][GRID_Z_SIZE];

// State variables to remember the last position
static int g_last_ix = -1;
static int g_last_iy = -1;
static int g_last_iz = -1;

// --- Internal Helper Functions (from before, but now static) ---
static bool get_indices_from_coords(float x_cm, float y_cm, float z_cm, 
                                    int* ix, int* iy, int* iz) {
    if (x_cm < 0 || x_cm >= (WORLD_X_METERS * 100) ||
        y_cm < 0 || y_cm >= (WORLD_Y_METERS * 100) ||
        z_cm < 0 || z_cm >= (WORLD_Z_METERS * 100)) {
        return false;
    }
    *ix = (int)(x_cm / GRID_RESOLUTION_CM);
    *iy = (int)(y_cm / GRID_RESOLUTION_CM);
    *iz = (int)(z_cm / GRID_RESOLUTION_CM);
    return true;
}

// --- Public API Functions ---

void spatial_map_init(void) {
    memset(g_spatial_map, 0, sizeof(g_spatial_map));
    // Initialize last known position to an invalid state
    g_last_ix = -1;
    g_last_iy = -1;
    g_last_iz = -1;
}

SpatialStatus spatial_map_process_movement(float x_cm, float y_cm, float z_cm, LMSFilter* lms_filter) {
    int current_ix, current_iy, current_iz;

    // 1. Convert current real-world coordinates to grid indices
    if (!get_indices_from_coords(x_cm, y_cm, z_cm, &current_ix, &current_iy, &current_iz)) {
        return SPATIAL_STATUS_NO_CHANGE; // Out of bounds, do nothing
    }

    // 2. Handle the very first time we get a valid coordinate
    if (g_last_ix == -1) {
        // Try to fetch a profile for this starting location
        SpatialTF* cell = &g_spatial_map[current_ix][current_iy][current_iz];
        if (cell->is_valid) {
            memcpy(lms_filter->g, cell->coefficients, sizeof(float) * LMS_TAPS);
        }
        // Update our last known position
        g_last_ix = current_ix;
        g_last_iy = current_iy;
        g_last_iz = current_iz;
        return SPATIAL_STATUS_INITIALIZED;
    }

    // 3. Check if we have moved to a new grid cell
    if (current_ix != g_last_ix || current_iy != g_last_iy || current_iz != g_last_iz) {
        // --- BOUNDARY CROSSED ---

        // a) SAVE the current state of the filter to the OLD cell
        SpatialTF* old_cell = &g_spatial_map[g_last_ix][g_last_iy][g_last_iz];
        memcpy(old_cell->coefficients, lms_filter->g, sizeof(float) * LMS_TAPS);
        old_cell->is_valid = true;

        // b) FETCH the profile for the NEW cell (if it exists)
        SpatialTF* new_cell = &g_spatial_map[current_ix][current_iy][current_iz];
        if (new_cell->is_valid) {
            memcpy(lms_filter->g, new_cell->coefficients, sizeof(float) * LMS_TAPS);
        } else {
            // Optional: If no profile exists for the new cell, you could reset the filter
            // to learn from scratch, or let it adapt from its current state.
            // lms_filter_init(lms_filter); // <-- Uncomment to force re-learning
        }

        // c) UPDATE our last known position to the new cell
        g_last_ix = current_ix;
        g_last_iy = current_iy;
        g_last_iz = current_iz;
        
        return SPATIAL_STATUS_BOUNDARY_CROSSED;
    }

    // 4. If we reach here, we are still in the same cell. Do nothing.
    return SPATIAL_STATUS_NO_CHANGE;
}

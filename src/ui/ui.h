#pragma once

#include <stdbool.h>  // bool
#include "os.h"
#include "ux.h"
#include "glyphs.h"

#define INSIDE_BORDERS 0
#define OUT_OF_BORDERS 1
/**
 * Callback to reuse action with approve/reject in step FLOW.
 */
typedef void (*action_validate_cb)(bool);

/**
 * Display address on the device and ask confirmation to export.
 *
 * @return 0 if success, negative integer otherwise.
 *
 */
int ui_display_address();

/**
 * Show main menu (ready screen, version, about, quit).
 */
void ui_menu_main();

/**
 * TODO:
 *
 * @return 0 if success, negative integer otherwise.
 *
 */
int ui_tx_hash_signing();

int ui_approve_tx_init();
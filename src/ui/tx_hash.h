#pragma once

#include <stdbool.h>  // bool

/**
 * Callback to reuse action with approve/reject in step FLOW.
 */
typedef void (*action_validate_cb)(bool);

/**
 * TODO:
 *
 * @return 0 if success, negative integer otherwise.
 *
 */
int ui_tx_blind_signing();
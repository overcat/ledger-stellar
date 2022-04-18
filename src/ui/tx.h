#pragma once

#include <stdbool.h>  // bool

/**
 * Callback to reuse action with approve/reject in step FLOW.
 */
typedef void (*action_validate_cb)(bool);

int ui_approve_tx_init(void);
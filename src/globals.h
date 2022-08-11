#pragma once

#include <stdint.h>
#include "types.h"

/**
 * Global context for user requests.
 */
extern global_ctx_t G_context;

extern swap_values_t G_swap_values;
extern bool G_called_from_swap;

extern char G_ui_detail_caption[DETAIL_CAPTION_MAX_LENGTH];
extern char G_ui_detail_value[DETAIL_VALUE_MAX_LENGTH];
extern volatile uint8_t G_ui_current_state;  // Dynamic screen?
extern uint8_t G_ui_current_data_index;
extern ui_action_validate_cb G_ui_validate_callback;
#include "globals.h"

global_ctx_t G_context;
swap_values_t G_swap_values;
bool G_called_from_swap;

// We define these variables as global variables to reduce memory usage.
char G_ui_detail_caption[DETAIL_CAPTION_MAX_LENGTH];
char G_ui_detail_value[DETAIL_VALUE_MAX_LENGTH];
volatile uint8_t G_ui_current_state;
uint8_t G_ui_current_data_index;
ui_action_validate_cb G_ui_validate_callback;
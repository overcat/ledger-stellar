#pragma once

#include <stdbool.h>  // bool

/*
 * the formatter prints the details and defines the order of the details
 * by setting the next formatter to be called
 */
typedef void (*format_function_t)(tx_ctx_t *txCtx);

/* 16 formatters in a row ought to be enough for everybody*/
#define MAX_FORMATTERS_PER_OPERATION 16

/* the current formatter */
extern format_function_t formatter_stack[MAX_FORMATTERS_PER_OPERATION];
extern int8_t formatter_index;

/* the current details printed by the formatter */
extern char op_caption[OPERATION_CAPTION_MAX_SIZE];

#ifdef TEST
extern uint8_t G_ui_current_data_index;
extern char G_ui_detail_caption[DETAIL_CAPTION_MAX_SIZE];
extern char G_ui_detail_value[DETAIL_VALUE_MAX_SIZE];
#endif  // TEST

void set_state_data(bool forward);
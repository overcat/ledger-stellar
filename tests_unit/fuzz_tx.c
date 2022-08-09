#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "transaction/transaction_parser.h"
#include "transaction/transaction_formatter.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    memset(&G_context, 0, sizeof(global_ctx_t));
    if (size > sizeof(G_context.tx_info.raw)) {
        return 0;
    }
    memcpy(&G_context.tx_info.raw, data, size);
    G_context.req_type = CONFIRM_TRANSACTION;
    G_context.tx_info.raw_length = size;
    if (!parse_tx_xdr(G_context.tx_info.raw, G_context.tx_info.raw_length, &G_context.tx_info)) {
        return 0;
    }
    G_context.state = STATE_PARSED;

    set_state_data(true);
    while (formatter_stack[formatter_index] != NULL) {
        printf("%s: %s\n", G_ui_detail_caption, G_ui_detail_value);
        formatter_index++;

        if (formatter_stack[formatter_index] != NULL) {
            set_state_data(true);
        }
    }
    return 0;
}

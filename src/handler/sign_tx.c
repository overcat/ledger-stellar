#include "sign_tx.h"
#include "../globals.h"
#include "sw.h"
#include "transaction/transaction_parser.h"
#include "send_response.h"
#include "crypto.h"
#include "common/format.h"
#include "../ui/ui.h"

int handler_sign_tx(buffer_t *cdata, bool is_first_chunk, bool more) {
    if (is_first_chunk) {
        explicit_bzero(&G_context, sizeof(G_context));
        G_context.req_type = CONFIRM_TRANSACTION;
        G_context.state = STATE_NONE;

        if (!buffer_read_u8(cdata, &G_context.bip32_path_len) ||
            !buffer_read_bip32_path(cdata,
                                    G_context.bip32_path,
                                    (size_t) G_context.bip32_path_len)) {
            return io_send_sw(SW_WRONG_DATA_LENGTH);
        }
        size_t data_length = cdata->size - cdata->offset;
        memcpy(G_context.tx_info.raw, cdata->ptr + cdata->offset, data_length);
        G_context.tx_info.rawLength += data_length;
    } else {
        if (G_context.req_type != CONFIRM_TRANSACTION) {
            return io_send_sw(SW_BAD_STATE);
        }
        memcpy(G_context.tx_info.raw + G_context.tx_info.rawLength, cdata->ptr, cdata->size);
        G_context.tx_info.rawLength += cdata->size;
    }

    if (more) {
        return io_send_sw(SW_OK);
    }
    cx_hash_sha256(G_context.tx_info.raw, G_context.tx_info.rawLength, G_context.hash, HASH_SIZE);

    if (!parse_tx_xdr(G_context.tx_info.raw, G_context.tx_info.rawLength, &G_context.tx_info)) {
        THROW(0x6800);
    }

    G_context.state = STATE_PARSED;
    PRINTF("tx parsed\n");

    return ui_approve_tx_init();
};

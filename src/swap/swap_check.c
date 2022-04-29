#include "../globals.h"
#include "../utils.h"
#include "../types.h"
#include "../sw.h"

void swap_check() {
    char *tmp_buf = G_ui_detail_value;

    tx_ctx_t *txCtx = &G_context.tx_info;

    // tx type
    if (txCtx->envelopeType != ENVELOPE_TYPE_TX) {
        io_send_sw(SW_DENY);
    }

    // A XLM swap consist of only one "send" operation
    if (txCtx->txDetails.opCount > 1) {
        io_send_sw(SW_DENY);
    }

    // op type
    if (txCtx->txDetails.opDetails.type != XDR_OPERATION_TYPE_PAYMENT) {
        io_send_sw(SW_DENY);
    }

    // amount
    if (txCtx->txDetails.opDetails.payment.asset.type != ASSET_TYPE_NATIVE ||
        txCtx->txDetails.opDetails.payment.amount != (int64_t) G_swap_values.amount) {
        io_send_sw(SW_DENY);
    }

    // destination addr
    print_muxed_account(&txCtx->txDetails.opDetails.payment.destination,
                        tmp_buf,
                        0,
                        0,
                        DETAIL_VALUE_MAX_LENGTH);
    if (strcmp(tmp_buf, G_swap_values.destination) != 0) {
        io_send_sw(SW_DENY);
    }

    if (txCtx->txDetails.opDetails.sourceAccountPresent) {
        io_send_sw(SW_DENY);
    }

    // memo
    if (txCtx->txDetails.memo.type != MEMO_TEXT ||
        strcmp(txCtx->txDetails.memo.text, G_swap_values.memo) != 0) {
        io_send_sw(SW_DENY);
    }

    // fees
    if (txCtx->network != NETWORK_TYPE_PUBLIC || txCtx->txDetails.fee != G_swap_values.fees) {
        io_send_sw(SW_DENY);
    }

    // we don't do any check on "TX Source" field
    // If we've reached this point without failure, we're good to go!
    if (crypto_sign_message() < 0) {
        G_context.state = STATE_NONE;
        io_send_sw(SW_SIGNATURE_FAIL);
    } else {
        send_response_sig();
    }
    os_sched_exit(0);
}

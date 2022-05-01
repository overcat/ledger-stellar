#include "../globals.h"
#include "../utils.h"
#include "../types.h"
#include "../sw.h"

bool swap_check() {
    PRINTF("swap_check invoked.\n");
    char *tmp_buf = G_ui_detail_value;

    tx_ctx_t *txCtx = &G_context.tx_info;

    // tx type
    if (txCtx->envelopeType != ENVELOPE_TYPE_TX) {
        return false;
    }

    // A XLM swap consist of only one "send" operation
    if (txCtx->txDetails.opCount != 1) {
        return false;
    }

    // op type
    if (txCtx->txDetails.opDetails.type != XDR_OPERATION_TYPE_PAYMENT) {
        return false;
    }

    // amount
    if (txCtx->txDetails.opDetails.payment.asset.type != ASSET_TYPE_NATIVE ||
        txCtx->txDetails.opDetails.payment.amount != (int64_t) G_swap_values.amount) {
        return false;
    }

    // destination addr
    if (!print_muxed_account(&txCtx->txDetails.opDetails.payment.destination,
                             tmp_buf,
                             DETAIL_VALUE_MAX_LENGTH,
                             0,
                             0)) {
        return false;
    };

    if (strcmp(tmp_buf, G_swap_values.destination) != 0) {
        return false;
    }

    if (txCtx->txDetails.opDetails.sourceAccountPresent) {
        return false;
    }

    // memo
    if (txCtx->txDetails.memo.type != MEMO_TEXT ||
        strcmp(txCtx->txDetails.memo.text, G_swap_values.memo) != 0) {
        return false;
    }

    // fees
    if (txCtx->network != NETWORK_TYPE_PUBLIC || txCtx->txDetails.fee != G_swap_values.fees) {
        return false;
    }

    // we don't do any check on "TX Source" field
    // If we've reached this point without failure, we're good to go!
    return true;
}

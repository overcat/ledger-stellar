#include <stdbool.h>  // bool
#include <string.h>   // memset

#include "ui.h"
#include "../globals.h"
#include "../sw.h"
#include "../utils.h"
#include "action/validate.h"
#include "../common/format.h"

// TODO: MEMCLEAR
static char g_address[ENCODED_ED25519_PUBLIC_KEY_LENGTH];
static char g_hash[64 + 1];
static action_validate_cb g_validate_callback;

// Step with icon and text
UX_STEP_NOCB(ux_tx_hash_signing_review_step,
             pnn,
             {
                 &C_icon_eye,
                 "Review",
                 "Transaction",
             });
UX_STEP_NOCB(ux_tx_hash_signing_warning_step,
             pbb,
             {
                 &C_icon_warning,
                 "Hash",
                 "Signing",
             });
// Step with title/text for address
UX_STEP_NOCB(ux_tx_hash_display_address_step,
             bnnn_paging,
             {
                 .title = "Address",
                 .text = g_address,
             });
// Step with approve button
UX_STEP_NOCB(ux_tx_hash_display_hash_step,
             bnnn_paging,
             {
                 .title = "Hash",
                 .text = g_hash,
             });
// Step with approve button
UX_STEP_CB(ux_tx_hash_display_approve_step,
           pb,
           (*g_validate_callback)(true),
           {
               &C_icon_validate_14,
               "Approve",
           });
// Step with reject button
UX_STEP_CB(ux_tx_hash_display_reject_step,
           pb,
           (*g_validate_callback)(false),
           {
               &C_icon_crossmark,
               "Reject",
           });
// FLOW to display hash signing
// #1 screen: eye icon + "Review Transaction"
// #1 screen: warning icon + "Hash Signing"
// #2 screen: display address
// #3 screen: display hash
// #4 screen: approve button
// #5 screen: reject button
UX_FLOW(ux_tx_hash_signing_flow,
        &ux_tx_hash_signing_review_step,
        &ux_tx_hash_signing_warning_step,
        &ux_tx_hash_display_address_step,
        &ux_tx_hash_display_hash_step,
        &ux_tx_hash_display_approve_step,
        &ux_tx_hash_display_reject_step);

int ui_tx_hash_signing() {
    if (G_context.req_type != CONFIRM_TRANSACTION_HASH || G_context.state != STATE_NONE) {
        G_context.state = STATE_NONE;
        return io_send_sw(SW_BAD_STATE);
    }

    memset(g_address, 0, sizeof(g_address));
    if (!encode_ed25519_public_key(G_context.raw_public_key, g_address, sizeof(g_address))) {
        return io_send_sw(SW_DISPLAY_ADDRESS_FAIL);
    }

    memset(g_hash, 0, sizeof(g_hash));
    if (!format_hex(G_context.hash, 32, g_hash, sizeof(g_hash))) {
        return io_send_sw(SW_DISPLAY_TRANSACTION_HASH_FAIL);
    }
    g_validate_callback = &ui_action_validate_transaction;
    ux_flow_init(0, ux_tx_hash_signing_flow, NULL);
    return 0;
}
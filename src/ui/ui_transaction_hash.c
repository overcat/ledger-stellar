#include <stdbool.h>  // bool
#include <string.h>   // memset

#include "ui.h"
#include "../globals.h"
#include "../sw.h"
#include "../utils.h"
#include "action/validate.h"
#include "../common/format.h"

static action_validate_cb g_validate_callback;

static void display_next_state(bool is_upper_border);

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
UX_STEP_INIT(ux_tx_init_upper_border, NULL, NULL, { display_next_state(true); });
UX_STEP_NOCB(ux_tx_variable_display,
             bnnn_paging,
             {
                 .title = detailCaption,
                 .text = detailValue,
             });
UX_STEP_INIT(ux_tx_init_lower_border, NULL, NULL, { display_next_state(false); });
// // Step with title/text for address
// UX_STEP_NOCB(ux_tx_hash_display_address_step,
//              bnnn_paging,
//              {
//                  .title = "Address",
//                  .text = g_address,
//              });
// // Step with approve button
// UX_STEP_NOCB(ux_tx_hash_display_hash_step,
//              bnnn_paging,
//              {
//                  .title = "Hash",
//                  .text = g_hash,
//              });
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
        &ux_tx_init_upper_border,
        &ux_tx_variable_display,
        &ux_tx_init_lower_border,
        &ux_tx_hash_display_approve_step,
        &ux_tx_hash_display_reject_step);

static uint8_t page_index;
static bool get_next_data(char *caption, char *value, bool forward) {
    if (forward) {
        page_index++;
    } else {
        page_index--;
    }
    switch (page_index) {
        case 1:
            strlcpy(caption, "Address", DETAIL_CAPTION_MAX_SIZE);
            if (!encode_ed25519_public_key(G_context.raw_public_key,
                                           value,
                                           DETAIL_VALUE_MAX_SIZE)) {
                return io_send_sw(SW_DISPLAY_ADDRESS_FAIL);
            }
            break;
        case 2:
            strlcpy(caption, "Hash", DETAIL_CAPTION_MAX_SIZE);
            if (!format_hex(G_context.hash, 32, value, DETAIL_VALUE_MAX_SIZE)) {
                return io_send_sw(SW_DISPLAY_TRANSACTION_HASH_FAIL);
            }
            break;
        default:
            return false;
    }
    return true;
}

// This is a special function you must call for bnnn_paging to work properly in an edgecase.
// It does some weird stuff with the `G_ux` global which is defined by the SDK.
// No need to dig deeper into the code, a simple copy-paste will do.
static void bnnn_paging_edgecase() {
    G_ux.flow_stack[G_ux.stack_count - 1].prev_index =
        G_ux.flow_stack[G_ux.stack_count - 1].index - 2;
    G_ux.flow_stack[G_ux.stack_count - 1].index--;
    ux_flow_relayout();
}

// Main function that handles all the business logic for our new display architecture.
static void display_next_state(bool is_upper_delimiter) {
    if (is_upper_delimiter) {  // We're called from the upper delimiter.
        if (current_state == OUT_OF_BORDERS) {
            // Fetch new data.
            bool dynamic_data = get_next_data(detailCaption, detailValue, true);

            if (dynamic_data) {
                // We found some data to display so we now enter in dynamic mode.
                current_state = INSIDE_BORDERS;
            }

            // Move to the next step, which will display the screen.
            ux_flow_next();
        } else {
            // The previous screen was NOT a static screen, so we were already in a dynamic screen.

            // Fetch new data.
            bool dynamic_data = get_next_data(detailCaption, detailValue, false);
            if (dynamic_data) {
                // We found some data so simply display it.
                ux_flow_next();
            } else {
                // There's no more dynamic data to display, so
                // update the current state accordingly.
                current_state = OUT_OF_BORDERS;

                // Display the previous screen which should be a static one.
                ux_flow_prev();
            }
        }
    } else {
        // We're called from the lower delimiter.

        if (current_state == OUT_OF_BORDERS) {
            // Fetch new data.
            bool dynamic_data = get_next_data(detailCaption, detailValue, false);

            if (dynamic_data) {
                // We found some data to display so enter in dynamic mode.
                current_state = INSIDE_BORDERS;
            }

            // Display the data.
            ux_flow_prev();
        } else {
            // We're being called from a dynamic screen, so the user was already browsing the
            // array.

            // Fetch new data.
            bool dynamic_data = get_next_data(detailCaption, detailValue, true);
            if (dynamic_data) {
                // We found some data, so display it.
                // Similar to `ux_flow_prev()` but updates layout to account for `bnnn_paging`'s
                // weird behaviour.
                bnnn_paging_edgecase();
            } else {
                // We found no data so make sure we update the state accordingly.
                current_state = OUT_OF_BORDERS;

                // Display the next screen
                ux_flow_next();
            }
        }
    }
}

int ui_tx_hash_signing() {
    if (G_context.req_type != CONFIRM_TRANSACTION_HASH || G_context.state != STATE_NONE) {
        G_context.state = STATE_NONE;
        return io_send_sw(SW_BAD_STATE);
    }
    current_state = OUT_OF_BORDERS;
    page_index = 0;

    // memset(g_address, 0, sizeof(g_address));
    // if (!encode_ed25519_public_key(G_context.raw_public_key, g_address, sizeof(g_address))) {
    //     return io_send_sw(SW_DISPLAY_ADDRESS_FAIL);
    // }

    // memset(g_hash, 0, sizeof(g_hash));
    // if (!format_hex(G_context.hash, 32, g_hash, sizeof(g_hash))) {
    //     return io_send_sw(SW_DISPLAY_TRANSACTION_HASH_FAIL);
    // }
    g_validate_callback = &ui_action_validate_transaction;
    ux_flow_init(0, ux_tx_hash_signing_flow, NULL);
    return 0;
}
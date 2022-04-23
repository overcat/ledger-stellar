#include <stdbool.h>  // bool
#include <string.h>   // memset

#include "ui.h"
#include "../globals.h"
#include "../sw.h"
#include "../utils.h"
#include "action/validate.h"
#include "../common/format.h"

static action_validate_cb g_validate_callback;
static char g_address[ENCODED_ED25519_PUBLIC_KEY_LENGTH] = {0};

// Step with icon and text
UX_STEP_NOCB(ux_display_confirm_addr_step, pn, {&C_icon_eye, "Confirm Address"});  // pnnn?
// Step with title/text for address
UX_STEP_NOCB(ux_display_address_step,
             bnnn_paging,
             {
                 .title = "Address",
                 .text = g_address,
             });
// Step with approve button
UX_STEP_CB(ux_display_approve_step,
           pb,
           (*g_validate_callback)(true),
           {
               &C_icon_validate_14,
               "Approve",
           });
// Step with reject button
UX_STEP_CB(ux_display_reject_step,
           pb,
           (*g_validate_callback)(false),
           {
               &C_icon_crossmark,
               "Reject",
           });

// FLOW to display address and BIP32 path:
// #1 screen: eye icon + "Confirm Address"
// #2 screen: display BIP32 Path
// #3 screen: display address
// #4 screen: approve button
// #5 screen: reject button
UX_FLOW(ux_display_pubkey_flow,
        &ux_display_confirm_addr_step,
        &ux_display_address_step,
        &ux_display_approve_step,
        &ux_display_reject_step);

int ui_display_address() {
    if (G_context.req_type != CONFIRM_ADDRESS || G_context.state != STATE_NONE) {
        G_context.state = STATE_NONE;
        return io_send_sw(SW_BAD_STATE);
    }

    memset(g_address, 0, sizeof(g_address));
    if (!encode_ed25519_public_key(G_context.raw_public_key, g_address, sizeof(g_address))) {
        return io_send_sw(SW_DISPLAY_ADDRESS_FAIL);
    }
    g_validate_callback = &ui_action_validate_pubkey;
    ux_flow_init(0, ux_display_pubkey_flow, NULL);
    return 0;
}

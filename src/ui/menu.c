#include "ui.h"
#include "globals.h"
#include "app_mode.h"

void ui_idle(void);
void display_settings(const ux_flow_step_t* const start_step);
void switch_settings_blind_signing();

static char g_hash_signing[12] = {0};

// FLOW for the settings menu:
// #1 screen: enable hash signing
// #2 screen: quit
UX_STEP_CB(ux_settings_hash_signing_step,
           bnnn_paging,
           switch_settings_blind_signing(),
           {
               .title = "Hash signing",
               .text = g_hash_signing,
           });

UX_STEP_CB(ux_settings_exit_step,
           pb,
           ui_idle(),
           {
               &C_icon_back_x,
               "Back",
           });
UX_FLOW(ux_settings_flow, &ux_settings_hash_signing_step, &ux_settings_exit_step);

// We have a screen with the icon and "Stellar is ready"
UX_STEP_NOCB(ux_menu_ready_step, pnn, {&C_icon_stellar, "Stellar", "is ready"});
UX_STEP_NOCB(ux_menu_version_step, bn, {"Version", APPVERSION});
UX_STEP_CB(ux_menu_settings_step, pb, display_settings(NULL), {&C_icon_coggle, "Settings"});
UX_STEP_VALID(ux_menu_exit_step, pb, os_sched_exit(-1), {&C_icon_dashboard_x, "Quit"});

// FLOW for the main menu:
// #1 screen: ready
// #2 screen: version of the app
// #3 screen: quit
UX_FLOW(ux_menu_main_flow,
        &ux_menu_ready_step,
        &ux_menu_settings_step,
        &ux_menu_version_step,
        &ux_menu_exit_step,
        FLOW_LOOP);

void ui_menu_main(void) {
    if (G_ux.stack_count == 0) {
        ux_stack_push();
    }
    ux_flow_init(0, ux_menu_main_flow, NULL);
};

void ui_idle(void) {
    // reserve a display stack slot if none yet
    if (G_ux.stack_count == 0) {
        ux_stack_push();
    }
    ux_flow_init(0, ux_menu_main_flow, NULL);
}

void display_settings(const ux_flow_step_t* const start_step) {
    strlcpy(g_hash_signing, (app_mode_hash_signing_enabled() ? "Enabled" : "NOT Enabled"), 12);
    ux_flow_init(0, ux_settings_flow, start_step);
}

void switch_settings_blind_signing() {
    app_mode_reverse_hash_signing_enabled();
    display_settings(&ux_settings_hash_signing_step);
}
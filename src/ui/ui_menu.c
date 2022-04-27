/*****************************************************************************
 *   Ledger Stellar App.
 *   (c) 2022 Ledger SAS.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *****************************************************************************/

#include "ui.h"
#include "globals.h"
#include "app_mode.h"

void ui_idle(void);
void display_settings(const ux_flow_step_t* const start_step);
void switch_settings_hash_signing();

// FLOW for the settings menu:
// #1 screen: enable hash signing
// #2 screen: quit
#if defined(TARGET_NANOS)
UX_STEP_CB(ux_settings_hash_signing_step,
           bnnn_paging,
           switch_settings_hash_signing(),
           {
               .title = "Hash signing",
               .text = G_ui_detail_value,
           });
#else
UX_STEP_CB(ux_settings_hash_signing_step,
           bnnn,
           switch_settings_hash_signing(),
           {
               "Hash signing",
               "Enable transaction",
               "hash signing",
               G_ui_detail_value,
           });
#endif
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
    strlcpy(G_ui_detail_value,
            (app_mode_hash_signing_enabled() ? "Enabled" : "NOT Enabled"),
            DETAIL_VALUE_MAX_LENGTH);
    ux_flow_init(0, ux_settings_flow, start_step);
}

void switch_settings_hash_signing() {
    app_mode_change_hash_signing_setting();
    display_settings(&ux_settings_hash_signing_step);
}
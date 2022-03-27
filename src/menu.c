#include "os.h"
#include "ux.h"
#include "glyphs.h"

#include "globals.h"
#include "menu.h"

// We have a screen with the icon and "Stellar is ready"
UX_STEP_NOCB(ux_menu_ready_step, pnn, {&C_icon_stellar, "Stellar", "is ready"});
UX_STEP_NOCB(ux_menu_version_step, bn, {"Version", APPVERSION});
UX_STEP_VALID(ux_menu_exit_step, pb, os_sched_exit(-1), {&C_icon_dashboard_x, "Quit"});

// FLOW for the main menu:
// #1 screen: ready
// #2 screen: version of the app
// #3 screen: quit
UX_FLOW(ux_menu_main_flow,
        &ux_menu_ready_step,
        &ux_menu_version_step,
        &ux_menu_exit_step,
        FLOW_LOOP);

void ui_menu_main(void) {
    if (G_ux.stack_count == 0) {
        ux_stack_push();
    }
    ux_flow_init(0, ux_menu_main_flow, NULL);
};
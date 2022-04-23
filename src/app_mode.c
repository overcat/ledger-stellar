#include "app_mode.h"

typedef struct {
    uint8_t hashSigningEnabled;
} app_mode_persistent_t;

app_mode_persistent_t const N_appmode_impl __attribute__((aligned(64)));
#define N_appmode (*(volatile app_mode_persistent_t *) PIC(&N_appmode_impl))

bool app_mode_hash_signing_enabled() {
    return N_appmode.hashSigningEnabled;
}

void app_mode_reverse_hash_signing_enabled() {
    app_mode_persistent_t mode;
    mode.hashSigningEnabled = !app_mode_hash_signing_enabled();
    nvm_write((void *) PIC(&N_appmode_impl), (void *) &mode, sizeof(app_mode_persistent_t));
}

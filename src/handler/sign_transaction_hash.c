#include "sign_transaction_hash.h"
#include "../globals.h"
#include "../app_mode.h"
#include "../sw.h"
#include "../crypto.h"
#include "../ui/ui.h"

int handler_sign_tx_hash(buffer_t *cdata) {
    PRINTF("handler_sign_tx_hash invoked\n");
    if (!app_mode_hash_signing_enabled()) {
        return io_send_sw(SW_TX_HASH_MODE_NOT_ENABLED);
    }
    explicit_bzero(&G_context, sizeof(G_context));
    G_context.req_type = CONFIRM_TRANSACTION_HASH;
    G_context.state = STATE_NONE;

    cx_ecfp_private_key_t private_key = {0};
    cx_ecfp_public_key_t public_key = {0};

    // TODO: check bip32 path valid
    if (!buffer_read_u8(cdata, &G_context.bip32_path_len) ||
        !buffer_read_bip32_path(cdata, G_context.bip32_path, (size_t) G_context.bip32_path_len)) {
        return io_send_sw(SW_WRONG_DATA_LENGTH);
    }

    // derive private key according to BIP32 path
    crypto_derive_private_key(&private_key, G_context.bip32_path, G_context.bip32_path_len);
    // generate corresponding public key
    crypto_init_public_key(&private_key, &public_key, G_context.raw_public_key);
    // reset private key
    explicit_bzero(&private_key, sizeof(private_key));

    if (cdata->offset + 32 != cdata->size) {
        return io_send_sw(SW_WRONG_DATA_LENGTH);
    }

    memcpy(G_context.hash, cdata->ptr + cdata->offset, 32);

    return ui_tx_blind_signing();
};

#pragma once

#include "io.h"

#include "common/macros.h"

/**
 * Helper to send APDU response with public key and chain code.
 *
 * response = G_context.pk_info.public_key (PUBKEY_LEN)
 *
 * @return zero or positive integer if success, -1 otherwise.
 *
 */
int send_response_pubkey(void);

/**
 * Helper to send APDU response with signature and v (parity of
 * y-coordinate of R).
 *
 * response = G_context.tx_info.signature (64)
 *
 * @return zero or positive integer if success, -1 otherwise.
 *
 */
int send_response_sig(void);

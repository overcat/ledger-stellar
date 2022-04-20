#include "utils.h"
#include "common/base58.h"
#include "common/base32.h"
#include "common/format.h"
#include <string.h>
#include <time.h>
#include <stdio.h>

#define MUXED_ACCOUNT_MED_25519_SIZE 43

uint16_t crc16(const uint8_t *input_str, int num_bytes) {
    int crc;
    crc = 0;
    while (--num_bytes >= 0) {
        crc = crc ^ (int) *input_str++ << 8;
        int i = 8;
        do {
            if (crc & 0x8000)
                crc = crc << 1 ^ 0x1021;
            else
                crc = crc << 1;
        } while (--i);
    }
    return crc;
}

bool encode_key(const uint8_t *in, uint8_t version_byte, char *out, uint8_t out_len) {
    if (out_len < 56 + 1) {
        return false;
    }
    uint8_t buffer[35];
    buffer[0] = version_byte;
    for (uint8_t i = 0; i < 32; i++) {
        buffer[i + 1] = in[i];
    }
    uint16_t crc = crc16(buffer, 33);  // checksum
    buffer[33] = crc;
    buffer[34] = crc >> 8;
    if (base32_encode(buffer, 35, (uint8_t *) out, 56) == -1) {
        return false;
    }
    out[56] = '\0';
    return true;
}

bool encode_ed25519_public_key(const uint8_t raw_public_key[static RAW_ED25519_PUBLIC_KEY_SIZE],
                               char *out,
                               size_t out_len) {
    return encode_key(raw_public_key, VERSION_BYTE_ED25519_PUBLIC_KEY, out, out_len);
}

bool encode_hash_x_key(const uint8_t raw_hash_x[static RAW_HASH_X_KEY_SIZE],
                       char *out,
                       size_t out_len) {
    return encode_key(raw_hash_x, VERSION_BYTE_HASH_X, out, out_len);
}

bool encode_pre_auth_x_key(const uint8_t raw_pre_auth_tx[static RAW_PRE_AUTH_TX_KEY_SIZE],
                           char *out,
                           size_t out_len) {
    return encode_key(raw_pre_auth_tx, VERSION_BYTE_PRE_AUTH_TX_KEY, out, out_len);
}

bool encode_muxed_account(const MuxedAccount *raw_muxed_account, char *out, size_t out_len) {
    if (raw_muxed_account->type == KEY_TYPE_ED25519) {
        return encode_ed25519_public_key(raw_muxed_account->ed25519, out, out_len);
    } else {
        if (out_len < ENCODED_MUXED_ACCOUNT_KEY_LENGTH) {
            return false;
        }
        uint8_t buffer[MUXED_ACCOUNT_MED_25519_SIZE];
        buffer[0] = VERSION_BYTE_MUXED_ACCOUNT;
        memcpy(buffer + 1, raw_muxed_account->med25519.ed25519, RAW_ED25519_PUBLIC_KEY_SIZE);
        for (int i = 0; i < 8; i++) {
            buffer[33 + i] = raw_muxed_account->med25519.id >> 8 * (7 - i);
        }
        uint16_t crc = crc16(buffer, MUXED_ACCOUNT_MED_25519_SIZE - 2);  // checksum
        buffer[41] = crc;
        buffer[42] = crc >> 8;
        if (base32_encode(buffer,
                          MUXED_ACCOUNT_MED_25519_SIZE,
                          (uint8_t *) out,
                          ENCODED_MUXED_ACCOUNT_KEY_LENGTH) == -1) {
            return false;
        }
        out[69] = '\0';
        return true;
    }
}

bool print_binary(const uint8_t *in, size_t in_len, char *out, size_t out_len) {
    return format_hex(in, in_len, out, out_len);
}

bool print_claimable_balance_id(const ClaimableBalanceID *claimable_balance_id,
                                char *out,
                                size_t out_len) {
    if (out_len < 36 * 2 + 1) {
        return false;
    }
    uint8_t data[36];
    memcpy(data, &claimable_balance_id->type, 4);
    memcpy(data + 4, claimable_balance_id->v0, 32);
    return print_binary(data, 36, out, out_len);
}

bool print_time(uint64_t seconds, char *out, size_t out_len) {
    if (seconds > 253402300799) {
        // valid range 1970-01-01 00:00:00 - 9999-12-31 23:59:59
        return false;
    }
    char time_str[20] = {0};  // 1970-01-01 00:00:00

    if (out_len < sizeof(time_str)) {
        return false;
    }
    struct tm tm;
    if (!gmtime_r((time_t *) &seconds, &tm)) {
        return false;
    };

    if (snprintf(time_str,
                 sizeof(time_str),
                 "%04d-%02d-%02d %02d:%02d:%02d",
                 tm.tm_year + 1900,
                 tm.tm_mon + 1,
                 tm.tm_mday,
                 tm.tm_hour,
                 tm.tm_min,
                 tm.tm_sec) < 0) {
        return false;
    };
    strcpy(out, time_str);
    return true;
}

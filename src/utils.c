#include "utils.h"
#include "common/base58.h"
#include "common/base32.h"

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
    // ENCODED_ED25519_PUBLIC_KEY_LENGTH
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

bool encode_ed25519_public_key(const uint8_t raw_public_key[static RAW_ED25519_PUBLIC_KEY_LENGTH],
                               char *out,
                               size_t out_len) {
    return encode_key(raw_public_key, 6 << 3, out, out_len);
};
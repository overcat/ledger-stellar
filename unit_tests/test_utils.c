#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <cmocka.h>

#include "common/base58.h"
#include "utils.h"
#include "types.h"

static void test_encode_ed25519_public_key(void **state) {
    (void) state;
    uint8_t raw_public_key[RAW_ED25519_PUBLIC_KEY_LENGTH] = {
        0xe9, 0x33, 0x88, 0xbb, 0xfd, 0x2f, 0xbd, 0x11, 0x80, 0x6d, 0xd0,
        0xbd, 0x59, 0xce, 0xa9, 0x7,  0x9e, 0x7c, 0xc7, 0xc,  0xe7, 0xb1,
        0xe1, 0x54, 0xf1, 0x14, 0xcd, 0xfe, 0x4e, 0x46, 0x6e, 0xcd};
    char *encoded_key = "GDUTHCF37UX32EMANXIL2WOOVEDZ47GHBTT3DYKU6EKM37SOIZXM2FN7";
    char out[ENCODED_ED25519_PUBLIC_KEY_LENGTH];
    size_t out_len = sizeof(out);
    assert_true(encode_ed25519_public_key(raw_public_key, out, out_len));
    assert_string_equal(out, encoded_key);
}

int main() {
    const struct CMUnitTest tests[] = {cmocka_unit_test(test_encode_ed25519_public_key)};
    return cmocka_run_group_tests(tests, NULL, NULL);
}
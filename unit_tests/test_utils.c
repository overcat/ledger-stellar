#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include "common/base58.h"

static void test_encode_ed25519_public_key(void **state) {
    (void) state;

    assert_false(true);
}

int main() {
    const struct CMUnitTest tests[] = {cmocka_unit_test(test_encode_ed25519_public_key)};
    return cmocka_run_group_tests(tests, NULL, NULL);
}
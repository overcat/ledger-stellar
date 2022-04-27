#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <cmocka.h>

#include "transaction/transaction_parser.h"
#include "transaction/transaction_formatter.h"

static const char *testcases[] = {
    "../testcases/feeBumpTxFeeSourceEqualSigner.raw",
    "../testcases/feeBumpTxFeeSourceMuxedAccountEqualSigner.raw",
    "../testcases/feeBumpTxInnerSourceEqualSigner.raw",
    "../testcases/feeBumpTxSimple.raw",
    "../testcases/feeBumpTxSimpleMuxedFeeSource.raw",
    "../testcases/txAccountMergeMuxedDestination.raw",
    "../testcases/txAccountMerge.raw",
    "../testcases/txAllowTrustAuthorized.raw",
    "../testcases/txAllowTrustAuthorizedToMaintainLiabilities.raw",
    "../testcases/txAllowTrustUnauthorized.raw",
    "../testcases/txBeginSponsoringFutureReserves.raw",
    "../testcases/txBumpSequence.raw",
    "../testcases/txChangeOffer.raw",
    "../testcases/txChangeTrust.raw",
    "../testcases/txChangeTrustLiquidityPoolAsset.raw",
    "../testcases/txClaimClaimableBalance.raw",
    "../testcases/txClawbackClaimableBalance.raw",
    "../testcases/txClawbackMuxedFrom.raw",
    "../testcases/txClawback.raw",
    "../testcases/txCreateAccount.raw",
    "../testcases/txCreateClaimableBalancePredicateAnd.raw",
    "../testcases/txCreateClaimableBalancePredicateBeforeAbs.raw",
    "../testcases/txCreateClaimableBalancePredicateBeforeRel.raw",
    "../testcases/txCreateClaimableBalancePredicateMultiClaimant.raw",
    "../testcases/txCreateClaimableBalancePredicateNot.raw",
    "../testcases/txCreateClaimableBalancePredicateOr.raw",
    "../testcases/txCreateClaimableBalancePredicateUnconditional.raw",
    "../testcases/txCreateOffer2.raw",
    "../testcases/txCreateOffer.raw",
    "../testcases/txCustomAsset12.raw",
    "../testcases/txCustomAsset4.raw",
    "../testcases/txEndSponsoringFutureReserves.raw",
    "../testcases/txInflation.raw",
    "../testcases/txLiquidityPoolDeposit.raw",
    "../testcases/txLiquidityPoolWithdraw.raw",
    "../testcases/txManageBuyOffer.raw",
    "../testcases/txMemoHash.raw",
    "../testcases/txMemoId.raw",
    "../testcases/txMemoText.raw",
    "../testcases/txMultiOp.raw",
    "../testcases/txMultiOpWithSource.raw",
    "../testcases/txOpSourceMuxedDestination.raw",
    "../testcases/txOpSource.raw",
    "../testcases/txPassiveOffer.raw",
    "../testcases/txPathPaymentStrictReceiveMuxedDestination.raw",
    "../testcases/txPathPaymentStrictReceiveEmptyPath.raw",
    "../testcases/txPathPaymentStrictReceive.raw",
    "../testcases/txPathPaymentStrictSendMuxedDestination.raw",
    "../testcases/txPathPaymentStrictSendEmptyPath.raw",
    "../testcases/txPathPaymentStrictSend.raw",
    "../testcases/txPaymentMuxedDestination.raw",
    "../testcases/txRemoveData.raw",
    "../testcases/txRevokeSponsorshipLiquidityPool.raw",
    "../testcases/txRemoveOffer.raw",
    "../testcases/txRemoveTrust.raw",
    "../testcases/txRemoveTrustLiquidityPoolAsset.raw",
    "../testcases/txRevokeSponsorshipAccount.raw",
    "../testcases/txRevokeSponsorshipClaimableBalance.raw",
    "../testcases/txRevokeSponsorshipData.raw",
    "../testcases/txRevokeSponsorshipOffer.raw",
    "../testcases/txRevokeSponsorshipSignerEd25519PublicKey.raw",
    "../testcases/txRevokeSponsorshipSignerHashX.raw",
    "../testcases/txRevokeSponsorshipSignerPreAuth.raw",
    "../testcases/txRevokeSponsorshipTrustLine.raw",
    "../testcases/txRevokeSponsorshipTrustLineLiquidityPoolID.raw",
    "../testcases/txSetAllOptions.raw",
    "../testcases/txSetData.raw",
    "../testcases/txSetOptionsEmptyBody.raw",
    "../testcases/txSetOptionsNoSigner.raw",
    "../testcases/txSetOptionsRemoveHomeDomain.raw",
    "../testcases/txSetSomeOptions.raw",
    "../testcases/txSetTrustLineFlags.raw",
    "../testcases/txSourceEqualOpSourceEqualSigner.raw",
    "../testcases/txSourceEqualSigner.raw",
    "../testcases/txSourceMuxedAccountEqualSigner.raw",
    "../testcases/txSimpleMuxedSource.raw",
    "../testcases/txSimple.raw",
    "../testcases/txCondTimeBounds.raw",
    "../testcases/txCondTimeBoundsMaxIsZero.raw",
    "../testcases/txCondTimeBoundsMinIsZero.raw",
    "../testcases/txCondExtraSigners.raw",
    "../testcases/txCondFull.raw",
    "../testcases/txCondLedgerBounds.raw",
    "../testcases/txCondLedgerBoundsMaxIsZero.raw",
    "../testcases/txCondLedgerBoundsMinIsZero.raw",
    "../testcases/txCondWithoutLedgerBounds.raw",
    "../testcases/txCondMinSeqAge.raw",
    "../testcases/txCondMinSeqLedgerGap.raw",
    "../testcases/txCondMinSeqNum.raw",
};

tx_ctx_t tx_info;
global_ctx_t G_context;

static void load_transaction_data(const char *filename, tx_ctx_t *txCtx) {
    FILE *f = fopen(filename, "rb");
    assert_non_null(f);

    txCtx->rawLength = fread(txCtx->raw, 1, RAW_TX_MAX_SIZE, f);
    assert_int_not_equal(txCtx->rawLength, 0);
    fclose(f);
}

static void get_result_filename(const char *filename, char *path, size_t size) {
    strncpy(path, filename, size);

    char *ext = strstr(path, ".raw");
    assert_non_null(ext);
    memcpy(ext, ".txt", 4);
}

static void check_transaction_results(const char *filename) {
    char path[1024];
    char line[4096];
    uint8_t opCount = G_context.tx_info.txDetails.opCount;
    G_ui_current_data_index = 0;
    get_result_filename(filename, path, sizeof(path));

    FILE *fp = fopen(path, "r");
    assert_non_null(fp);

    set_state_data(true);

    while ((opCount != 0 && G_ui_current_data_index < opCount) ||
           formatter_stack[formatter_index] != NULL) {
        assert_non_null(fgets(line, sizeof(line), fp));

        char *expected_title = line;
        char *expected_value = strstr(line, "; ");
        assert_non_null(expected_value);

        *expected_value = '\x00';
        assert_string_equal(expected_title, G_ui_detail_caption);

        expected_value += 2;
        char *p = strchr(expected_value, '\n');
        if (p != NULL) {
            *p = '\x00';
        }
        assert_string_equal(expected_title, G_ui_detail_caption);
        assert_string_equal(expected_value, G_ui_detail_value);

        formatter_index++;

        if (formatter_stack[formatter_index] != NULL) {
            set_state_data(true);
        }
    }
    assert_int_equal(fgets(line, sizeof(line), fp), 0);
    assert_int_equal(feof(fp), 1);
    fclose(fp);
}

static void test_tx(const char *filename) {
    memset(&G_context.tx_info, 0, sizeof(G_context.tx_info));

    load_transaction_data(filename, &G_context.tx_info);

    // GDJYDBIA3WHL4IGI4PHQBBOLPXTR5A6U5SAPYMIPIGYXB37GSOAIP2GC
    uint8_t publicKey[] = {0xd3, 0x81, 0x85, 0x0,  0xdd, 0x8e, 0xbe, 0x20, 0xc8, 0xe3, 0xcf,
                           0x0,  0x85, 0xcb, 0x7d, 0xe7, 0x1e, 0x83, 0xd4, 0xec, 0x80, 0xfc,
                           0x31, 0xf,  0x41, 0xb1, 0x70, 0xef, 0xe6, 0x93, 0x80, 0x87};
    // TODO: check status
    // G_context.state = STATE_APPROVE_TX;
    assert_true(
        parse_tx_xdr(G_context.tx_info.raw, G_context.tx_info.rawLength, &G_context.tx_info));
    memcpy(G_context.raw_public_key, publicKey, sizeof(publicKey));

    // G_context.state = STATE_APPROVE_TX;

    check_transaction_results(filename);
}

void test_transactions(void **state) {
    (void) state;

    for (int i = 0; i < sizeof(testcases) / sizeof(testcases[0]); i++) {
        test_tx(testcases[i]);
    }
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_transactions),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
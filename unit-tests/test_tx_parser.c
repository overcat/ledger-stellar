#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <cmocka.h>

#include "tx_parser.h"

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

static void parse_tx(const char *filename) {
    FILE *f = fopen(filename, "rb");
    assert_non_null(f);
    tx_ctx_t tx_info;
    memset(&tx_info, 0, sizeof(tx_ctx_t));
    tx_info.rawLength = fread(tx_info.raw, 1, MAX_RAW_TX, f);
    if (!parse_tx_xdr(tx_info.raw, tx_info.rawLength, &tx_info)) {
        fail_msg("parse %s failed!", filename);
    }
}

void test_parse() {
    for (int i = 0; i < sizeof(testcases) / sizeof(testcases[0]); i++) {
        parse_tx(testcases[i]);
    }
}

int main() {
    const struct CMUnitTest tests[] = {cmocka_unit_test(test_parse)};
    return cmocka_run_group_tests(tests, NULL, NULL);
}
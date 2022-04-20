#include <stdbool.h>  // bool
#include <string.h>   // memset
#include "os.h"
#include "tx.h"
#include "../globals.h"
#include "action/validate.h"
#include "../common/format.h"
#include "../stellar_utils.h"
#include "parse.h"

#define MEMCLEAR(dest)                       \
    do {                                     \
        explicit_bzero(&dest, sizeof(dest)); \
    } while (0)
// #define PIC(code) code

static action_validate_cb g_validate_callback;
typedef void (*format_function_t)(tx_ctx_t *txCtx);

/* 16 formatters in a row ought to be enough for everybody*/
#define MAX_FORMATTERS_PER_OPERATION 16
void set_state_data(bool forward);
/* the current formatter */
format_function_t formatter_stack[MAX_FORMATTERS_PER_OPERATION];
int8_t formatter_index;
uint8_t current_data_index;  // network = 1, detail = 2, op + 1

/* the current details printed by the formatter */
char opCaption[OPERATION_CAPTION_MAX_SIZE];
char detailCaption[DETAIL_CAPTION_MAX_SIZE];
char detailValue[DETAIL_VALUE_MAX_SIZE];
#define INSIDE_BORDERS 0
#define OUT_OF_BORDERS 1
uint8_t num_data;                // operation 总数
volatile uint8_t current_state;  // 确定是不是在动态数据内

void display_next_state(bool is_upper_border);

static void push_to_formatter_stack(format_function_t formatter) {
    if (formatter_index + 1 >= MAX_FORMATTERS_PER_OPERATION) {
        THROW(0x6124);
    }
    formatter_stack[formatter_index + 1] = formatter;
}

static void format_next_step(tx_ctx_t *txCtx) {
    (void) txCtx;
    formatter_stack[formatter_index] = NULL;
    set_state_data(true);
}

static void format_network(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Network");
    strlcpy(detailValue, (char *) PIC(NETWORK_NAMES[txCtx->network]), DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_next_step);
}

static void format_fee_bump_transaction_fee(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Fee");
    Asset asset = {.type = ASSET_TYPE_NATIVE};
    print_amount(txCtx->feeBumpTxDetails.fee,
                 &asset,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_next_step);
}

static void format_fee_bump_transaction_source(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Fee Source");
    if (txCtx->envelopeType == ENVELOPE_TYPE_TX_FEE_BUMP &&
        txCtx->feeBumpTxDetails.feeSource.type == KEY_TYPE_ED25519 &&
        memcmp(txCtx->feeBumpTxDetails.feeSource.ed25519,
               G_context.raw_public_key,
               ED25519_PUBLIC_KEY_LEN) == 0) {
        print_muxed_account(&txCtx->feeBumpTxDetails.feeSource, detailValue, 6, 6);
    } else {
        print_muxed_account(&txCtx->feeBumpTxDetails.feeSource, detailValue, 0, 0);
    }
    push_to_formatter_stack(&format_fee_bump_transaction_fee);
}

static void format_fee_bump_transaction_details(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Fee Bump");
    strcpy(detailValue, "Transaction Details");
    push_to_formatter_stack(&format_fee_bump_transaction_source);
}

static void format_transaction_source(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Tx Source");
    if (txCtx->envelopeType == ENVELOPE_TYPE_TX &&
        txCtx->txDetails.sourceAccount.type == KEY_TYPE_ED25519 &&
        memcmp(txCtx->txDetails.sourceAccount.ed25519,
               G_context.raw_public_key,
               ED25519_PUBLIC_KEY_LEN) == 0) {
        print_muxed_account(&txCtx->txDetails.sourceAccount, detailValue, 6, 6);
    } else {
        print_muxed_account(&txCtx->txDetails.sourceAccount, detailValue, 0, 0);
    }
    push_to_formatter_stack(format_next_step);
}

static void format_min_seq_ledger_gap(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Min Seq Ledger Gap");
    print_uint(txCtx->txDetails.cond.minSeqLedgerGap, detailValue, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_transaction_source);
}

static void format_min_seq_ledger_gap_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.cond.minSeqLedgerGap == 0) {
        format_transaction_source(txCtx);
    } else {
        format_min_seq_ledger_gap(txCtx);
    }
}

static void format_min_seq_age(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Min Seq Age");
    print_uint(txCtx->txDetails.cond.minSeqAge, detailValue, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_min_seq_ledger_gap_prepare);
}

static void format_min_seq_age_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.cond.minSeqAge == 0) {
        format_min_seq_ledger_gap_prepare(txCtx);
    } else {
        format_min_seq_age(txCtx);
    }
}

static void format_min_seq_num(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Min Seq Num");
    print_uint(txCtx->txDetails.cond.minSeqNum, detailValue, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_min_seq_age_prepare);
}

static void format_min_seq_num_prepare(tx_ctx_t *txCtx) {
    if (!txCtx->txDetails.cond.hasMinSeqNum || txCtx->txDetails.cond.minSeqNum == 0) {
        format_min_seq_age_prepare(txCtx);
    } else {
        format_min_seq_num(txCtx);
    }
}

static void format_ledger_bounds_max_ledger(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Max Ledger Bounds");
    if (txCtx->txDetails.cond.ledgerBounds.maxLedger == 0) {
        strcpy(detailValue, "[no restriction]");
    } else {
        print_uint(txCtx->txDetails.cond.ledgerBounds.maxLedger,
                   detailValue,
                   DETAIL_VALUE_MAX_SIZE);
    }
    push_to_formatter_stack(&format_min_seq_num_prepare);
}

static void format_ledger_bounds_min_ledger(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Min Ledger Bounds");
    if (txCtx->txDetails.cond.ledgerBounds.minLedger == 0) {
        strcpy(detailValue, "[no restriction]");
    } else {
        print_uint(txCtx->txDetails.cond.ledgerBounds.minLedger,
                   detailValue,
                   DETAIL_VALUE_MAX_SIZE);
    }
    push_to_formatter_stack(&format_ledger_bounds_max_ledger);
}

static void format_ledger_bounds(tx_ctx_t *txCtx) {
    if (!txCtx->txDetails.cond.hasLedgerBounds ||
        (txCtx->txDetails.cond.ledgerBounds.minLedger == 0 &&
         txCtx->txDetails.cond.ledgerBounds.maxLedger == 0)) {
        format_min_seq_num_prepare(txCtx);
    } else {
        format_ledger_bounds_min_ledger(txCtx);
    }
}

static void format_time_bounds_max_time(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Time Bounds To");
    if (txCtx->txDetails.cond.timeBounds.maxTime == 0) {
        strcpy(detailValue, "[no restriction]");
    } else {
        if (!print_time_(txCtx->txDetails.cond.timeBounds.maxTime,
                        detailValue,
                        DETAIL_VALUE_MAX_SIZE)) {
            THROW(0x6126);
        };
    }
    push_to_formatter_stack(&format_ledger_bounds);
}

static void format_time_bounds_min_time(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Time Bounds From");
    if (txCtx->txDetails.cond.timeBounds.minTime == 0) {
        strcpy(detailValue, "[no restriction]");
    } else {
        if (!print_time_(txCtx->txDetails.cond.timeBounds.minTime,
                        detailValue,
                        DETAIL_VALUE_MAX_SIZE)) {
            THROW(0x6126);
        };
    }
    push_to_formatter_stack(&format_time_bounds_max_time);
}

static void format_time_bounds(tx_ctx_t *txCtx) {
    // TODO: should we add a page to remind the user that this is UTC time?
    if (!txCtx->txDetails.cond.hasTimeBounds || (txCtx->txDetails.cond.timeBounds.minTime == 0 &&
                                                 txCtx->txDetails.cond.timeBounds.maxTime == 0)) {
        format_ledger_bounds(txCtx);
    } else {
        format_time_bounds_min_time(txCtx);
    }
}

static void format_sequence(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Sequence Num");
    print_uint(txCtx->txDetails.sequenceNumber, detailValue, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_time_bounds);
}

static void format_fee(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Fee");
    Asset asset = {.type = ASSET_TYPE_NATIVE};
    print_amount(txCtx->txDetails.fee, &asset, txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_sequence);
}

static void format_memo(tx_ctx_t *txCtx) {
    Memo *memo = &txCtx->txDetails.memo;
    switch (memo->type) {
        case MEMO_ID: {
            strcpy(detailCaption, "Memo ID");
            print_uint(memo->id, detailValue, DETAIL_VALUE_MAX_SIZE);
            break;
        }
        case MEMO_TEXT: {
            strcpy(detailCaption, "Memo Text");
            strlcpy(detailValue, memo->text, MEMO_TEXT_MAX_SIZE + 1);
            break;
        }
        case MEMO_HASH: {
            strcpy(detailCaption, "Memo Hash");
            print_binary__summary(memo->hash, detailValue, HASH_SIZE);
            break;
        }
        case MEMO_RETURN: {
            strcpy(detailCaption, "Memo Return");
            print_binary__summary(memo->hash, detailValue, HASH_SIZE);
            break;
        }
        default: {
            strcpy(detailCaption, "Memo");
            strcpy(detailValue, "[none]");
        }
    }
    push_to_formatter_stack(&format_fee);
}

static void format_transaction_details(tx_ctx_t *txCtx) {
    switch (txCtx->envelopeType) {
        case ENVELOPE_TYPE_TX_FEE_BUMP:
            strcpy(detailCaption, "InnerTx");
            break;
        case ENVELOPE_TYPE_TX:
            strcpy(detailCaption, "Transaction");
            break;
    }
    strcpy(detailValue, "Details");
    push_to_formatter_stack(&format_memo);
}

static void format_operation_source(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Op Source");
    print_muxed_account(&txCtx->txDetails.opDetails.sourceAccount, detailValue, 0, 0);

    if (txCtx->txDetails.opIdx == txCtx->txDetails.opCount) {
        // last operation
        push_to_formatter_stack(NULL);
    } else {
        // more operations
        push_to_formatter_stack(&format_next_step);
    }
}

static void format_operation_source_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.sourceAccountPresent) {
        // If the source exists, when the user clicks the next button,
        // it will jump to the page showing the source
        push_to_formatter_stack(&format_operation_source);
    } else {
        // If not, jump to the signing page or show the next operation.
        if (txCtx->txDetails.opIdx == txCtx->txDetails.opCount) {
            // last operation
            push_to_formatter_stack(NULL);
        } else {
            // more operations
            push_to_formatter_stack(&format_next_step);
        }
    }
}

static void format_bump_sequence_bump_to(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Bump To");
    print_int(txCtx->txDetails.opDetails.bumpSequenceOp.bumpTo, detailValue, DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_bump_sequence(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Bump Sequence");
    push_to_formatter_stack(&format_bump_sequence_bump_to);
}

static void format_inflation(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Inflation");
    format_operation_source_prepare(txCtx);
}

static void format_account_merge_destination(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Destination");
    print_muxed_account(&txCtx->txDetails.opDetails.destination, detailValue, 0, 0);
    format_operation_source_prepare(txCtx);
}

static void format_account_merge_detail(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Merge Account");
    if (txCtx->txDetails.opDetails.sourceAccountPresent) {
        print_muxed_account(&txCtx->txDetails.opDetails.sourceAccount, detailValue, 0, 0);
    } else {
        print_muxed_account(&txCtx->txDetails.sourceAccount, detailValue, 0, 0);
    }
    push_to_formatter_stack(&format_account_merge_destination);
}

static void format_account_merge(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Account Merge");
    push_to_formatter_stack(&format_account_merge_detail);
}

static void format_manage_data_value(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Data Value");
    char tmp[89];
    base64_encode(txCtx->txDetails.opDetails.manageDataOp.dataValue,
                  txCtx->txDetails.opDetails.manageDataOp.dataValueSize,
                  tmp);
    print_summary(tmp, detailValue, 12, 12);
    format_operation_source_prepare(txCtx);
}

static void format_manage_data_detail(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.manageDataOp.dataValueSize) {
        strcpy(detailCaption, "Set Data");
        push_to_formatter_stack(&format_manage_data_value);
    } else {
        strcpy(detailCaption, "Remove Data");
        format_operation_source_prepare(txCtx);
    }
    char tmp[65];
    memcpy(tmp,
           txCtx->txDetails.opDetails.manageDataOp.dataName,
           txCtx->txDetails.opDetails.manageDataOp.dataNameSize);
    tmp[txCtx->txDetails.opDetails.manageDataOp.dataNameSize] = '\0';
    print_summary(tmp, detailValue, 12, 12);
}

static void format_manage_data(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Manage Data");
    push_to_formatter_stack(&format_manage_data_detail);
}

static void format_allow_trust_authorize(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Authorize Flag");
    if (txCtx->txDetails.opDetails.allowTrustOp.authorize == AUTHORIZED_FLAG) {
        strlcpy(detailValue, "AUTHORIZED_FLAG", DETAIL_VALUE_MAX_SIZE);
    } else if (txCtx->txDetails.opDetails.allowTrustOp.authorize ==
               AUTHORIZED_TO_MAINTAIN_LIABILITIES_FLAG) {
        strlcpy(detailValue, "AUTHORIZED_TO_MAINTAIN_LIABILITIES_FLAG", DETAIL_VALUE_MAX_SIZE);
    } else {
        strlcpy(detailValue, "UNAUTHORIZED_FLAG", DETAIL_VALUE_MAX_SIZE);
    }
    format_operation_source_prepare(txCtx);
}

static void format_allow_trust_asset_code(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Asset Code");
    strlcpy(detailValue, txCtx->txDetails.opDetails.allowTrustOp.assetCode, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_allow_trust_authorize);
}

static void format_allow_trust_trustor(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Trustor");
    print_public_key(txCtx->txDetails.opDetails.allowTrustOp.trustor, detailValue, 0, 0);
    push_to_formatter_stack(&format_allow_trust_asset_code);
}

static void format_allow_trust(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Allow Trust");
    push_to_formatter_stack(&format_allow_trust_trustor);
}

static void format_set_option_signer_weight(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Weight");
    print_uint(txCtx->txDetails.opDetails.setOptionsOp.signer.weight,
               detailValue,
               DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_set_option_signer_detail(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Signer Key");
    SignerKey *key = &txCtx->txDetails.opDetails.setOptionsOp.signer.key;

    switch (key->type) {
        case SIGNER_KEY_TYPE_ED25519: {
            print_public_key(key->data, detailValue, 0, 0);
            break;
        }
        case SIGNER_KEY_TYPE_HASH_X: {
            char tmp[57];
            encode_hash_x_key_(key->data, tmp);
            print_summary(tmp, detailValue, 12, 12);
            break;
        }

        case SIGNER_KEY_TYPE_PRE_AUTH_TX: {
            char tmp[57];
            encode_pre_auth_key(key->data, tmp);
            print_summary(tmp, detailValue, 12, 12);
            break;
        }
        default:
            break;
    }
    push_to_formatter_stack(&format_set_option_signer_weight);
}

static void format_set_option_signer(tx_ctx_t *txCtx) {
    signer_t *signer = &txCtx->txDetails.opDetails.setOptionsOp.signer;
    if (signer->weight) {
        strcpy(detailCaption, "Add Signer");
    } else {
        strcpy(detailCaption, "Remove Signer");
    }
    switch (signer->key.type) {
        case SIGNER_KEY_TYPE_ED25519: {
            strcpy(detailValue, "Type Public Key");
            break;
        }
        case SIGNER_KEY_TYPE_HASH_X: {
            strcpy(detailValue, "Type Hash(x)");
            break;
        }
        case SIGNER_KEY_TYPE_PRE_AUTH_TX: {
            strcpy(detailValue, "Type Pre-Auth");
            break;
        }
        default:
            break;
    }
    push_to_formatter_stack(&format_set_option_signer_detail);
}

static void format_set_option_signer_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.setOptionsOp.signerPresent) {
        push_to_formatter_stack(&format_set_option_signer);
    } else {
        format_operation_source_prepare(txCtx);
    }
}

static void format_set_option_home_domain(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Home Domain");
    if (txCtx->txDetails.opDetails.setOptionsOp.homeDomainSize) {
        memcpy(detailValue,
               txCtx->txDetails.opDetails.setOptionsOp.homeDomain,
               txCtx->txDetails.opDetails.setOptionsOp.homeDomainSize);
        detailValue[txCtx->txDetails.opDetails.setOptionsOp.homeDomainSize] = '\0';
    } else {
        strcpy(detailValue, "[remove home domain from account]");
    }
    format_set_option_signer_prepare(txCtx);
}

static void format_set_option_home_domain_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.setOptionsOp.homeDomainPresent) {
        push_to_formatter_stack(&format_set_option_home_domain);
    } else {
        format_set_option_signer_prepare(txCtx);
    }
}

static void format_set_option_high_threshold(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "High Threshold");
    print_uint(txCtx->txDetails.opDetails.setOptionsOp.highThreshold,
               detailValue,
               DETAIL_VALUE_MAX_SIZE);
    format_set_option_home_domain_prepare(txCtx);
}

static void format_set_option_high_threshold_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.setOptionsOp.highThresholdPresent) {
        push_to_formatter_stack(&format_set_option_high_threshold);
    } else {
        format_set_option_home_domain_prepare(txCtx);
    }
}

static void format_set_option_medium_threshold(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Medium Threshold");
    print_uint(txCtx->txDetails.opDetails.setOptionsOp.mediumThreshold,
               detailValue,
               DETAIL_VALUE_MAX_SIZE);
    format_set_option_high_threshold_prepare(txCtx);
}

static void format_set_option_medium_threshold_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.setOptionsOp.mediumThresholdPresent) {
        push_to_formatter_stack(&format_set_option_medium_threshold);
    } else {
        format_set_option_high_threshold_prepare(txCtx);
    }
}

static void format_set_option_low_threshold(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Low Threshold");
    print_uint(txCtx->txDetails.opDetails.setOptionsOp.lowThreshold,
               detailValue,
               DETAIL_VALUE_MAX_SIZE);
    format_set_option_medium_threshold_prepare(txCtx);
}

static void format_set_option_low_threshold_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.setOptionsOp.lowThresholdPresent) {
        push_to_formatter_stack(&format_set_option_low_threshold);
    } else {
        format_set_option_medium_threshold_prepare(txCtx);
    }
}

static void format_set_option_master_weight(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Master Weight");
    print_uint(txCtx->txDetails.opDetails.setOptionsOp.masterWeight,
               detailValue,
               DETAIL_VALUE_MAX_SIZE);
    format_set_option_low_threshold_prepare(txCtx);
}

static void format_set_option_master_weight_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.setOptionsOp.masterWeightPresent) {
        push_to_formatter_stack(&format_set_option_master_weight);
    } else {
        format_set_option_low_threshold_prepare(txCtx);
    }
}

static void format_set_option_set_flags(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Set Flags");
    print_flags(txCtx->txDetails.opDetails.setOptionsOp.setFlags,
                detailValue,
                DETAIL_VALUE_MAX_SIZE);
    format_set_option_master_weight_prepare(txCtx);
}

static void format_set_option_set_flags_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.setOptionsOp.setFlags) {
        push_to_formatter_stack(&format_set_option_set_flags);
    } else {
        format_set_option_master_weight_prepare(txCtx);
    }
}

static void format_set_option_clear_flags(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Clear Flags");
    print_flags(txCtx->txDetails.opDetails.setOptionsOp.clearFlags,
                detailValue,
                DETAIL_VALUE_MAX_SIZE);
    format_set_option_set_flags_prepare(txCtx);
}

static void format_set_option_clear_flags_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.setOptionsOp.clearFlags) {
        push_to_formatter_stack(&format_set_option_clear_flags);
    } else {
        format_set_option_set_flags_prepare(txCtx);
    }
}

static void format_set_option_inflation_destination(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Inflation Dest");
    print_public_key(txCtx->txDetails.opDetails.setOptionsOp.inflationDestination,
                     detailValue,
                     0,
                     0);
    format_set_option_clear_flags_prepare(txCtx);
}

static void format_set_option_inflation_destination_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.setOptionsOp.inflationDestinationPresent) {
        push_to_formatter_stack(format_set_option_inflation_destination);
    } else {
        format_set_option_clear_flags_prepare(txCtx);
    }
}

static void format_set_options_empty_body(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "SET OPTIONS");
    strcpy(detailValue, "BODY IS EMPTY");
    format_operation_source_prepare(txCtx);
}

static bool is_empty_set_options_body(tx_ctx_t *txCtx) {
    return !(txCtx->txDetails.opDetails.setOptionsOp.inflationDestinationPresent ||
             txCtx->txDetails.opDetails.setOptionsOp.clearFlags ||
             txCtx->txDetails.opDetails.setOptionsOp.setFlags ||
             txCtx->txDetails.opDetails.setOptionsOp.masterWeightPresent ||
             txCtx->txDetails.opDetails.setOptionsOp.lowThresholdPresent ||
             txCtx->txDetails.opDetails.setOptionsOp.mediumThresholdPresent ||
             txCtx->txDetails.opDetails.setOptionsOp.highThresholdPresent ||
             txCtx->txDetails.opDetails.setOptionsOp.homeDomainPresent ||
             txCtx->txDetails.opDetails.setOptionsOp.signerPresent);
}

static void format_set_options(tx_ctx_t *txCtx) {
    // this operation is a special one among all operations, because all its fields are optional.
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Set Options");
    if (is_empty_set_options_body(txCtx)) {
        push_to_formatter_stack(format_set_options_empty_body);
    } else {
        format_set_option_inflation_destination_prepare(txCtx);
    }
}

static void format_change_trust_limit(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Trust Limit");
    if (txCtx->txDetails.opDetails.changeTrustOp.limit == INT64_MAX) {
        strcpy(detailValue, "[maximum]");
    } else {
        print_amount(txCtx->txDetails.opDetails.changeTrustOp.limit,
                     NULL,
                     txCtx->network,
                     detailValue,
                     DETAIL_VALUE_MAX_SIZE);
    }
    format_operation_source_prepare(txCtx);
}

static void format_change_trust_detail_liquidity_pool_fee(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Pool Fee Rate");
    uint64_t fee =
        ((uint64_t)
             txCtx->txDetails.opDetails.changeTrustOp.line.liquidityPool.constantProduct.fee *
         10000000) /
        100;
    print_amount(fee, NULL, txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    strlcat(detailValue, "%", DETAIL_VALUE_MAX_SIZE);
    if (txCtx->txDetails.opDetails.changeTrustOp.limit) {
        push_to_formatter_stack(&format_change_trust_limit);
    } else {
        format_operation_source_prepare(txCtx);
    }
}

static void format_change_trust_detail_liquidity_pool_asset_b(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Asset B");
    if (txCtx->txDetails.opDetails.changeTrustOp.line.liquidityPool.constantProduct.assetB.type ==
        ASSET_TYPE_NATIVE) {
        print_native_asset_code(txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    } else {
        print_asset_t(
            &txCtx->txDetails.opDetails.changeTrustOp.line.liquidityPool.constantProduct.assetB,
            txCtx->network,
            detailValue,
            DETAIL_VALUE_MAX_SIZE);
    }
    push_to_formatter_stack(&format_change_trust_detail_liquidity_pool_fee);
}

static void format_change_trust_detail_liquidity_pool_asset_a(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Asset A");
    if (txCtx->txDetails.opDetails.changeTrustOp.line.liquidityPool.constantProduct.assetA.type ==
        ASSET_TYPE_NATIVE) {
        print_native_asset_code(txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    } else {
        print_asset_t(
            &txCtx->txDetails.opDetails.changeTrustOp.line.liquidityPool.constantProduct.assetA,
            txCtx->network,
            detailValue,
            DETAIL_VALUE_MAX_SIZE);
    }
    push_to_formatter_stack(&format_change_trust_detail_liquidity_pool_asset_b);
}

static void format_change_trust_detail_liquidity_pool_asset(tx_ctx_t *txCtx) {
    (void) txCtx;

    strcpy(detailCaption, "Liquidity Pool");
    strcpy(detailValue, "Asset");
    push_to_formatter_stack(&format_change_trust_detail_liquidity_pool_asset_a);
}

static void format_change_trust_detail(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.changeTrustOp.limit) {
        strcpy(detailCaption, "Change Trust");
    } else {
        strcpy(detailCaption, "Remove Trust");
    }
    uint8_t asset_type = txCtx->txDetails.opDetails.changeTrustOp.line.type;
    switch (asset_type) {
        case ASSET_TYPE_CREDIT_ALPHANUM4:
        case ASSET_TYPE_CREDIT_ALPHANUM12:
            print_asset_t((Asset *) &txCtx->txDetails.opDetails.changeTrustOp.line,
                          txCtx->network,
                          detailValue,
                          DETAIL_VALUE_MAX_SIZE);
            if (txCtx->txDetails.opDetails.changeTrustOp.limit) {
                push_to_formatter_stack(&format_change_trust_limit);
            } else {
                format_operation_source_prepare(txCtx);
            }
            break;
        case ASSET_TYPE_POOL_SHARE:
            push_to_formatter_stack(&format_change_trust_detail_liquidity_pool_asset);
            break;
        default:
            return;
    }
}

static void format_change_trust(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Change Trust");
    push_to_formatter_stack(&format_change_trust_detail);
}

static void format_manage_sell_offer_sell(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Sell");
    print_amount(txCtx->txDetails.opDetails.manageSellOfferOp.amount,
                 &txCtx->txDetails.opDetails.manageSellOfferOp.selling,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_manage_sell_offer_price(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Price");
    uint64_t price = ((uint64_t) txCtx->txDetails.opDetails.manageSellOfferOp.price.n * 10000000) /
                     txCtx->txDetails.opDetails.manageSellOfferOp.price.d;
    print_amount(price,
                 &txCtx->txDetails.opDetails.manageSellOfferOp.buying,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_manage_sell_offer_sell);
}

static void format_manage_sell_offer_buy(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Buy");
    if (txCtx->txDetails.opDetails.manageSellOfferOp.buying.type == ASSET_TYPE_NATIVE) {
        print_native_asset_code(txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    } else {
        print_asset_t(&txCtx->txDetails.opDetails.manageSellOfferOp.buying,
                      txCtx->network,
                      detailValue,
                      DETAIL_VALUE_MAX_SIZE);
    }
    push_to_formatter_stack(&format_manage_sell_offer_price);
}

static void format_manage_sell_offer_detail(tx_ctx_t *txCtx) {
    if (!txCtx->txDetails.opDetails.manageSellOfferOp.amount) {
        strcpy(detailCaption, "Remove Offer");
        print_uint(txCtx->txDetails.opDetails.manageSellOfferOp.offerID,
                   detailValue,
                   DETAIL_VALUE_MAX_SIZE);
        format_operation_source_prepare(txCtx);
    } else {
        if (txCtx->txDetails.opDetails.manageSellOfferOp.offerID) {
            strcpy(detailCaption, "Change Offer");
            print_uint(txCtx->txDetails.opDetails.manageSellOfferOp.offerID,
                       detailValue,
                       DETAIL_VALUE_MAX_SIZE);
        } else {
            strcpy(detailCaption, "Create Offer");
            strcpy(detailValue, "Type Active");
        }
        push_to_formatter_stack(&format_manage_sell_offer_buy);
    }
}

static void format_manage_sell_offer(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Manage Sell Offer");
    push_to_formatter_stack(&format_manage_sell_offer_detail);
}

static void format_manage_buy_offer_buy(tx_ctx_t *txCtx) {
    ManageBuyOfferOp *op = &txCtx->txDetails.opDetails.manageBuyOfferOp;

    strcpy(detailCaption, "Buy");
    print_amount(op->buyAmount, &op->buying, txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_manage_buy_offer_price(tx_ctx_t *txCtx) {
    ManageBuyOfferOp *op = &txCtx->txDetails.opDetails.manageBuyOfferOp;

    strcpy(detailCaption, "Price");
    uint64_t price = ((uint64_t) op->price.n * 10000000) / op->price.d;
    print_amount(price, &op->selling, txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_manage_buy_offer_buy);
}

static void format_manage_buy_offer_sell(tx_ctx_t *txCtx) {
    ManageBuyOfferOp *op = &txCtx->txDetails.opDetails.manageBuyOfferOp;

    strcpy(detailCaption, "Sell");
    if (op->selling.type == ASSET_TYPE_NATIVE) {
        print_native_asset_code(txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    } else {
        print_asset_t(&op->selling, txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    }
    push_to_formatter_stack(&format_manage_buy_offer_price);
}

static void format_manage_buy_offer_detail(tx_ctx_t *txCtx) {
    ManageBuyOfferOp *op = &txCtx->txDetails.opDetails.manageBuyOfferOp;

    if (op->buyAmount == 0) {
        strcpy(detailCaption, "Remove Offer");
        print_uint(op->offerID, detailValue, DETAIL_VALUE_MAX_SIZE);
        format_operation_source_prepare(txCtx);  // TODO
    } else {
        if (op->offerID) {
            strcpy(detailCaption, "Change Offer");
            print_uint(op->offerID, detailValue, DETAIL_VALUE_MAX_SIZE);
        } else {
            strcpy(detailCaption, "Create Offer");
            strcpy(detailValue, "Type Active");
        }
        push_to_formatter_stack(&format_manage_buy_offer_sell);
    }
}

static void format_manage_buy_offer(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Manage Buy Offer");
    push_to_formatter_stack(&format_manage_buy_offer_detail);
}

static void format_create_passive_sell_offer_sell(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Sell");
    print_amount(txCtx->txDetails.opDetails.createPassiveSellOfferOp.amount,
                 &txCtx->txDetails.opDetails.createPassiveSellOfferOp.selling,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_create_passive_sell_offer_price(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Price");

    CreatePassiveSellOfferOp *op = &txCtx->txDetails.opDetails.createPassiveSellOfferOp;
    uint64_t price = ((uint64_t) op->price.n * 10000000) / op->price.d;
    print_amount(price, &op->buying, txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_create_passive_sell_offer_sell);
}

static void format_create_passive_sell_offer_buy(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Buy");
    if (txCtx->txDetails.opDetails.createPassiveSellOfferOp.buying.type == ASSET_TYPE_NATIVE) {
        print_native_asset_code(txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    } else {
        print_asset_t(&txCtx->txDetails.opDetails.createPassiveSellOfferOp.buying,
                      txCtx->network,
                      detailValue,
                      DETAIL_VALUE_MAX_SIZE);
    }
    push_to_formatter_stack(&format_create_passive_sell_offer_price);
}

static void format_create_passive_sell_offer(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Create Passive Sell Offer");
    push_to_formatter_stack(&format_create_passive_sell_offer_buy);
}

static void format_path_payment_strict_receive_path_via(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Via");
    uint8_t i;
    for (i = 0; i < txCtx->txDetails.opDetails.pathPaymentStrictReceiveOp.pathLen; i++) {
        char asset_name[12 + 1];
        Asset *asset = &txCtx->txDetails.opDetails.pathPaymentStrictReceiveOp.path[i];
        if (strlen(detailValue) != 0) {
            strlcat(detailValue, ", ", DETAIL_VALUE_MAX_SIZE);
        }
        print_asset_name(asset, txCtx->network, asset_name, sizeof(asset_name));
        strlcat(detailValue, asset_name, DETAIL_VALUE_MAX_SIZE);
    }
    format_operation_source_prepare(txCtx);
}

static void format_path_payment_strict_receive_receive(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Receive");
    print_amount(txCtx->txDetails.opDetails.pathPaymentStrictReceiveOp.destAmount,
                 &txCtx->txDetails.opDetails.pathPaymentStrictReceiveOp.destAsset,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    if (txCtx->txDetails.opDetails.pathPaymentStrictReceiveOp.pathLen) {
        push_to_formatter_stack(&format_path_payment_strict_receive_path_via);
    } else {
        format_operation_source_prepare(txCtx);
    }
}

static void format_path_payment_strict_receive_destination(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Destination");
    print_muxed_account(&txCtx->txDetails.opDetails.pathPaymentStrictReceiveOp.destination,
                        detailValue,
                        0,
                        0);
    push_to_formatter_stack(&format_path_payment_strict_receive_receive);
}

static void format_path_payment_strict_receive_send(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Send Max");
    print_amount(txCtx->txDetails.opDetails.pathPaymentStrictReceiveOp.sendMax,
                 &txCtx->txDetails.opDetails.pathPaymentStrictReceiveOp.sendAsset,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_path_payment_strict_receive_destination);
}

static void format_path_payment_strict_receive(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Path Payment Strict Receive");
    push_to_formatter_stack(&format_path_payment_strict_receive_send);
}

static void format_path_payment_strict_send_path_via(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Via");
    uint8_t i;
    for (i = 0; i < txCtx->txDetails.opDetails.pathPaymentStrictSendOp.pathLen; i++) {
        char asset_name[12 + 1];
        Asset *asset = &txCtx->txDetails.opDetails.pathPaymentStrictSendOp.path[i];
        if (strlen(detailValue) != 0) {
            strlcat(detailValue, ", ", DETAIL_VALUE_MAX_SIZE);
        }
        print_asset_name(asset, txCtx->network, asset_name, sizeof(asset_name));
        strlcat(detailValue, asset_name, DETAIL_VALUE_MAX_SIZE);
    }
    format_operation_source_prepare(txCtx);
}

static void format_path_payment_strict_send_receive(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Receive Min");
    print_amount(txCtx->txDetails.opDetails.pathPaymentStrictSendOp.destMin,
                 &txCtx->txDetails.opDetails.pathPaymentStrictSendOp.destAsset,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    if (txCtx->txDetails.opDetails.pathPaymentStrictSendOp.pathLen) {
        push_to_formatter_stack(&format_path_payment_strict_send_path_via);
    } else {
        format_operation_source_prepare(txCtx);
    }
}

static void format_path_payment_strict_send_destination(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Destination");
    print_muxed_account(&txCtx->txDetails.opDetails.pathPaymentStrictSendOp.destination,
                        detailValue,
                        0,
                        0);
    push_to_formatter_stack(&format_path_payment_strict_send_receive);
}

static void format_path_payment_strict_send(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Send");
    print_amount(txCtx->txDetails.opDetails.pathPaymentStrictSendOp.sendAmount,
                 &txCtx->txDetails.opDetails.pathPaymentStrictSendOp.sendAsset,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_path_payment_strict_send_destination);
}

static void format_payment_destination(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Destination");
    print_muxed_account(&txCtx->txDetails.opDetails.payment.destination, detailValue, 0, 0);
    format_operation_source_prepare(txCtx);
}

static void format_payment_send(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Send");
    print_amount(txCtx->txDetails.opDetails.payment.amount,
                 &txCtx->txDetails.opDetails.payment.asset,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_payment_destination);
}

static void format_payment(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Payment");
    push_to_formatter_stack(&format_payment_send);
}

static void format_create_account_amount(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Starting Balance");
    Asset asset = {.type = ASSET_TYPE_NATIVE};
    print_amount(txCtx->txDetails.opDetails.createAccount.startingBalance,
                 &asset,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_create_account_destination(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Destination");
    print_public_key(txCtx->txDetails.opDetails.createAccount.destination, detailValue, 0, 0);
    push_to_formatter_stack(&format_create_account_amount);
}

static void format_create_account(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Create Account");
    push_to_formatter_stack(&format_create_account_destination);
}

void format_create_claimable_balance_warning(tx_ctx_t *txCtx) {
    (void) txCtx;
    // TODO: The claimant can be very complicated. I haven't figured out how to
    // display it for the time being, so let's display an WARNING here first.
    strcpy(detailCaption, "WARNING");
    strcpy(detailValue, "Currently does not support displaying claimant details");
    format_operation_source_prepare(txCtx);
}

static void format_create_claimable_balance_balance(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Balance");
    print_amount(txCtx->txDetails.opDetails.createClaimableBalanceOp.amount,
                 &txCtx->txDetails.opDetails.createClaimableBalanceOp.asset,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_create_claimable_balance_warning);
}

static void format_create_claimable_balance(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Create Claimable Balance");
    push_to_formatter_stack(&format_create_claimable_balance_balance);
}

static void format_claim_claimable_balance_balance_id(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Balance ID");
    print_claimable_balance_id_(&txCtx->txDetails.opDetails.claimClaimableBalanceOp.balanceID,
                               detailValue);
    format_operation_source_prepare(txCtx);
}

static void format_claim_claimable_balance(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Claim Claimable Balance");
    push_to_formatter_stack(&format_claim_claimable_balance_balance_id);
}

static void format_claim_claimable_balance_sponsored_id(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Sponsored ID");
    print_public_key(txCtx->txDetails.opDetails.beginSponsoringFutureReservesOp.sponsoredID,
                     detailValue,
                     0,
                     0);
    format_operation_source_prepare(txCtx);
}

static void format_begin_sponsoring_future_reserves(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Begin Sponsoring Future Reserves");
    push_to_formatter_stack(&format_claim_claimable_balance_sponsored_id);
}

static void format_end_sponsoring_future_reserves(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "End Sponsoring Future Reserves");
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_account(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Account ID");
    print_public_key(txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.account.accountID,
                     detailValue,
                     0,
                     0);
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_trust_line_asset(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.trustLine.asset.type ==
        ASSET_TYPE_POOL_SHARE) {
        strcpy(detailCaption, "Liquidity Pool ID");
        print_binary_(txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.trustLine.asset
                         .liquidityPoolID,
                     detailValue,
                     LIQUIDITY_POOL_ID_SIZE);
    } else {
        strcpy(detailCaption, "Asset");
        print_asset_t(
            (Asset *) &txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.trustLine.asset,
            txCtx->network,
            detailValue,
            DETAIL_VALUE_MAX_SIZE);
    }
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_trust_line_account(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Account ID");
    print_public_key(txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.trustLine.accountID,
                     detailValue,
                     0,
                     0);
    push_to_formatter_stack(&format_revoke_sponsorship_trust_line_asset);
}
static void format_revoke_sponsorship_offer_offer_id(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Offer ID");
    print_uint(txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.offer.offerID,
               detailValue,
               DETAIL_VALUE_MAX_SIZE);

    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_offer_seller_id(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Seller ID");
    print_public_key(txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.offer.sellerID,
                     detailValue,
                     0,
                     0);
    push_to_formatter_stack(&format_revoke_sponsorship_offer_offer_id);
}

static void format_revoke_sponsorship_data_data_name(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Data Name");

    _Static_assert(DATA_NAME_MAX_SIZE + 1 < DETAIL_VALUE_MAX_SIZE,
                   "DATA_NAME_MAX_SIZE must be smaller than DETAIL_VALUE_MAX_SIZE");

    memcpy(detailValue,
           txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.data.dataName,
           txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.data.dataNameSize);
    detailValue[txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.data.dataNameSize] = '\0';
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_data_account(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Account ID");
    print_public_key(txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.data.accountID,
                     detailValue,
                     0,
                     0);
    push_to_formatter_stack(&format_revoke_sponsorship_data_data_name);
}

static void format_revoke_sponsorship_claimable_balance(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Balance ID");
    print_claimable_balance_id_(
        &txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.claimableBalance.balanceId,
        detailValue);
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_liquidity_pool(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Liquidity Pool ID");
    print_binary_(
        txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.liquidityPool.liquidityPoolID,
        detailValue,
        LIQUIDITY_POOL_ID_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_claimable_signer_signer_key_detail(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Signer Key");
    SignerKey *key = &txCtx->txDetails.opDetails.revokeSponsorshipOp.signer.signerKey;

    switch (key->type) {
        case SIGNER_KEY_TYPE_ED25519: {
            print_public_key(key->data, detailValue, 0, 0);
            break;
        }
        case SIGNER_KEY_TYPE_HASH_X: {
            char tmp[57];
            encode_hash_x_key_(key->data, tmp);
            print_summary(tmp, detailValue, 12, 12);
            break;
        }

        case SIGNER_KEY_TYPE_PRE_AUTH_TX: {
            char tmp[57];
            encode_pre_auth_key(key->data, tmp);
            print_summary(tmp, detailValue, 12, 12);
            break;
        }
        default:
            break;
    }
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_claimable_signer_signer_key_type(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Signer Key Type");
    switch (txCtx->txDetails.opDetails.revokeSponsorshipOp.signer.signerKey.type) {
        case SIGNER_KEY_TYPE_ED25519: {
            strcpy(detailValue, "Public Key");
            break;
        }
        case SIGNER_KEY_TYPE_HASH_X: {
            strcpy(detailValue, "Hash(x)");
            break;
        }
        case SIGNER_KEY_TYPE_PRE_AUTH_TX: {
            strcpy(detailValue, "Pre-Auth");
            break;
        }
        default:
            break;
    }

    push_to_formatter_stack(&format_revoke_sponsorship_claimable_signer_signer_key_detail);
}

static void format_revoke_sponsorship_claimable_signer_account(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Account ID");
    print_public_key(txCtx->txDetails.opDetails.revokeSponsorshipOp.signer.accountID,
                     detailValue,
                     0,
                     0);
    push_to_formatter_stack(&format_revoke_sponsorship_claimable_signer_signer_key_type);
}

static void format_revoke_sponsorship(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Operation Type");
    if (txCtx->txDetails.opDetails.revokeSponsorshipOp.type == REVOKE_SPONSORSHIP_SIGNER) {
        strcpy(detailValue, "Revoke Sponsorship (SIGNER_KEY)");
        push_to_formatter_stack(&format_revoke_sponsorship_claimable_signer_account);
    } else {
        switch (txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.type) {
            case ACCOUNT:
                strcpy(detailValue, "Revoke Sponsorship (ACCOUNT)");
                push_to_formatter_stack(&format_revoke_sponsorship_account);
                break;
            case OFFER:
                strcpy(detailValue, "Revoke Sponsorship (OFFER)");
                push_to_formatter_stack(&format_revoke_sponsorship_offer_seller_id);
                break;
            case TRUSTLINE:
                strcpy(detailValue, "Revoke Sponsorship (TRUSTLINE)");
                push_to_formatter_stack(&format_revoke_sponsorship_trust_line_account);
                break;
            case DATA:
                strcpy(detailValue, "Revoke Sponsorship (DATA)");
                push_to_formatter_stack(&format_revoke_sponsorship_data_account);
                break;
            case CLAIMABLE_BALANCE:
                strcpy(detailValue, "Revoke Sponsorship (CLAIMABLE_BALANCE)");
                push_to_formatter_stack(&format_revoke_sponsorship_claimable_balance);
                break;
            case LIQUIDITY_POOL:
                strcpy(detailValue, "Revoke Sponsorship (LIQUIDITY_POOL)");
                push_to_formatter_stack(&format_revoke_sponsorship_liquidity_pool);
                break;
        }
    }
}

static void format_clawback_from(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "From");
    print_muxed_account(&txCtx->txDetails.opDetails.clawbackOp.from, detailValue, 0, 0);
    format_operation_source_prepare(txCtx);
}

static void format_clawback_amount(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Clawback Balance");
    print_amount(txCtx->txDetails.opDetails.clawbackOp.amount,
                 &txCtx->txDetails.opDetails.clawbackOp.asset,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_clawback_from);
}

static void format_clawback(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Clawback");
    push_to_formatter_stack(&format_clawback_amount);
}

static void format_clawback_claimable_balance_balance_id(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Balance ID");
    print_claimable_balance_id_(&txCtx->txDetails.opDetails.clawbackClaimableBalanceOp.balanceID,
                               detailValue);
    format_operation_source_prepare(txCtx);
}

static void format_clawback_claimable_balance(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Clawback Claimable Balance");
    push_to_formatter_stack(&format_clawback_claimable_balance_balance_id);
}

static void format_set_trust_line_set_flags(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Set Flags");
    if (txCtx->txDetails.opDetails.setTrustLineFlagsOp.setFlags) {
        print_trust_line_flags(txCtx->txDetails.opDetails.setTrustLineFlagsOp.setFlags,
                               detailValue,
                               DETAIL_VALUE_MAX_SIZE);
    } else {
        strcpy(detailValue, "[none]");
    }
    format_operation_source_prepare(txCtx);
}

static void format_set_trust_line_clear_flags(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Clear Flags");
    if (txCtx->txDetails.opDetails.setTrustLineFlagsOp.clearFlags) {
        print_trust_line_flags(txCtx->txDetails.opDetails.setTrustLineFlagsOp.clearFlags,
                               detailValue,
                               DETAIL_VALUE_MAX_SIZE);
    } else {
        strcpy(detailValue, "[none]");
    }
    push_to_formatter_stack(&format_set_trust_line_set_flags);
}

static void format_set_trust_line_asset(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Asset");
    print_asset_t(&txCtx->txDetails.opDetails.setTrustLineFlagsOp.asset,
                  txCtx->network,
                  detailValue,
                  DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_set_trust_line_clear_flags);
}

static void format_set_trust_line_trustor(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Trustor");
    print_public_key(txCtx->txDetails.opDetails.setTrustLineFlagsOp.trustor, detailValue, 0, 0);
    push_to_formatter_stack(&format_set_trust_line_asset);
}

static void format_set_trust_line_flags(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Set Trust Line Flags");
    push_to_formatter_stack(&format_set_trust_line_trustor);
}

static void format_liquidity_pool_deposit_max_price(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Max Price");
    uint64_t price =
        ((uint64_t) txCtx->txDetails.opDetails.liquidityPoolDepositOp.maxPrice.n * 10000000) /
        txCtx->txDetails.opDetails.liquidityPoolDepositOp.maxPrice.d;
    print_amount(price, NULL, txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_liquidity_pool_deposit_min_price(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Min Price");
    uint64_t price =
        ((uint64_t) txCtx->txDetails.opDetails.liquidityPoolDepositOp.minPrice.n * 10000000) /
        txCtx->txDetails.opDetails.liquidityPoolDepositOp.minPrice.d;
    print_amount(price, NULL, txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_liquidity_pool_deposit_max_price);
}

static void format_liquidity_pool_deposit_max_amount_b(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Max Amount B");
    print_amount(txCtx->txDetails.opDetails.liquidityPoolDepositOp.maxAmountB,
                 NULL,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_liquidity_pool_deposit_min_price);
}

static void format_liquidity_pool_deposit_max_amount_a(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Max Amount A");
    print_amount(txCtx->txDetails.opDetails.liquidityPoolDepositOp.maxAmountA,
                 NULL,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_liquidity_pool_deposit_max_amount_b);
}

static void format_liquidity_pool_deposit_liquidity_pool_id(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Liquidity Pool ID");
    print_binary_(txCtx->txDetails.opDetails.liquidityPoolDepositOp.liquidityPoolID,
                 detailValue,
                 LIQUIDITY_POOL_ID_SIZE);
    push_to_formatter_stack(&format_liquidity_pool_deposit_max_amount_a);
}

static void format_liquidity_pool_deposit(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Liquidity Pool Deposit");
    push_to_formatter_stack(&format_liquidity_pool_deposit_liquidity_pool_id);
}

static void format_liquidity_pool_withdraw_min_amount_b(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Min Amount B");
    print_amount(txCtx->txDetails.opDetails.liquidityPoolWithdrawOp.minAmountB,
                 NULL,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_liquidity_pool_withdraw_min_amount_a(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Min Amount A");
    print_amount(txCtx->txDetails.opDetails.liquidityPoolWithdrawOp.minAmountA,
                 NULL,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_liquidity_pool_withdraw_min_amount_b);
}

static void format_liquidity_pool_withdraw_amount(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Amount");
    print_amount(txCtx->txDetails.opDetails.liquidityPoolWithdrawOp.amount,
                 NULL,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_liquidity_pool_withdraw_min_amount_a);
}

static void format_liquidity_pool_withdraw_liquidity_pool_id(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Liquidity Pool ID");
    print_binary_(txCtx->txDetails.opDetails.liquidityPoolWithdrawOp.liquidityPoolID,
                 detailValue,
                 LIQUIDITY_POOL_ID_SIZE);
    push_to_formatter_stack(&format_liquidity_pool_withdraw_amount);
}

static void format_liquidity_pool_withdraw(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Liquidity Pool Withdraw");
    push_to_formatter_stack(&format_liquidity_pool_withdraw_liquidity_pool_id);
}

static const format_function_t formatters[] = {&format_create_account,
                                               &format_payment,
                                               &format_path_payment_strict_receive,
                                               &format_manage_sell_offer,
                                               &format_create_passive_sell_offer,
                                               &format_set_options,
                                               &format_change_trust,
                                               &format_allow_trust,
                                               &format_account_merge,
                                               &format_inflation,
                                               &format_manage_data,
                                               &format_bump_sequence,
                                               &format_manage_buy_offer,
                                               &format_path_payment_strict_send,
                                               &format_create_claimable_balance,
                                               &format_claim_claimable_balance,
                                               &format_begin_sponsoring_future_reserves,
                                               &format_end_sponsoring_future_reserves,
                                               &format_revoke_sponsorship,
                                               &format_clawback,
                                               &format_clawback_claimable_balance,
                                               &format_set_trust_line_flags,
                                               &format_liquidity_pool_deposit,
                                               &format_liquidity_pool_withdraw};

void format_confirm_operation(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opCount > 1) {
        size_t len;
        strcpy(opCaption, "Operation ");
        len = strlen(opCaption);
        print_uint(txCtx->txDetails.opIdx, opCaption + len, OPERATION_CAPTION_MAX_SIZE - len);
        strlcat(opCaption, " of ", sizeof(opCaption));
        len = strlen(opCaption);
        print_uint(txCtx->txDetails.opCount, opCaption + len, OPERATION_CAPTION_MAX_SIZE - len);
        push_to_formatter_stack(
            ((format_function_t) PIC(formatters[txCtx->txDetails.opDetails.type])));
    } else {
        ((format_function_t) PIC(formatters[txCtx->txDetails.opDetails.type]))(txCtx);
    }
}

static void format_confirm_hash_detail(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Hash");
    print_binary__summary(G_context.hash, detailValue, 32);
    push_to_formatter_stack(NULL);
}

void format_confirm_hash_warning(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "WARNING");
    strcpy(detailValue, "No details available");
    push_to_formatter_stack(&format_confirm_hash_detail);
}

uint8_t current_data_index;

format_function_t get_formatter(tx_ctx_t *txCtx, bool forward) {
    if (!forward) {
        if (current_data_index ==
            0) {  // if we're already at the beginning of the buffer, return NULL
            return NULL;
        }
        // rewind to tx beginning if we're requesting a previous operation
        txCtx->offset = 0;
        txCtx->txDetails.opIdx = 0;
    }

    uint8_t data_count_before_ops;
    if (txCtx->envelopeType == ENVELOPE_TYPE_TX_FEE_BUMP) {
        data_count_before_ops = 3;
        switch (current_data_index) {
            case 1:
                return &format_network;
            case 2:
                return &format_fee_bump_transaction_details;
            case 3:
                return &format_transaction_details;
            default:
                break;
        }
    } else if (txCtx->envelopeType == ENVELOPE_TYPE_TX) {
        data_count_before_ops = 2;
        switch (current_data_index) {
            case 1:
                return &format_network;
            case 2:
                return &format_transaction_details;
            default:
                break;
        }
    } else {
        THROW(0x6125);
    }

    while (current_data_index - data_count_before_ops > txCtx->txDetails.opIdx) {
        if (!parse_tx_xdr(txCtx->raw, txCtx->rawLength, txCtx)) {
            return NULL;
        }
    }
    return &format_confirm_operation;
}

void ui_approve_tx_next_screen(tx_ctx_t *txCtx) {
    if (!formatter_stack[formatter_index]) {
        MEMCLEAR(formatter_stack);
        formatter_index = 0;
        current_data_index++;
        formatter_stack[0] = get_formatter(txCtx, true);
    }
}

void ui_approve_tx_prev_screen(tx_ctx_t *txCtx) {
    if (formatter_index == -1) {
        MEMCLEAR(formatter_stack);
        formatter_index = 0;
        current_data_index--;
        formatter_stack[0] = get_formatter(txCtx, false);
    }
}

void set_state_data(bool forward) {
    PRINTF("set_state_data invoked, forward = %d\n", forward);
    if (forward) {
        ui_approve_tx_next_screen(&G_context.tx_info);
    } else {
        ui_approve_tx_prev_screen(&G_context.tx_info);
    }

    // Apply last formatter to fill the screen's buffer
    if (formatter_stack[formatter_index]) {
        MEMCLEAR(detailCaption);
        MEMCLEAR(detailValue);
        MEMCLEAR(opCaption);
        formatter_stack[formatter_index](&G_context.tx_info);

        if (opCaption[0] != '\0') {
            strlcpy(detailCaption, opCaption, sizeof(detailCaption));
            detailValue[0] = ' ';
        }
    }
}
// clang-format off
UX_STEP_NOCB(
    ux_confirm_tx_init_flow_step,
    pnn,
    {
      &C_icon_eye,
      "Review",
      "Transaction",
    });

UX_STEP_INIT(
    ux_init_upper_border,
    NULL,
    NULL,
    {
        display_next_state(true);
    });
UX_STEP_NOCB(
    ux_variable_display,
    bnnn_paging,
    {
      .title = detailCaption,
      .text = detailValue,
    });
UX_STEP_INIT(
    ux_init_lower_border,
    NULL,
    NULL,
    {
        display_next_state(false);
    });

UX_STEP_CB(
    ux_confirm_tx_finalize_step,
    pnn,
    g_validate_callback(true),
    {
      &C_icon_validate_14,
      "Finalize",
      "Transaction",
    });

UX_STEP_CB(
    ux_reject_tx_flow_step,
    pb,
    g_validate_callback(false),
    {
      &C_icon_crossmark,
      "Cancel",
    });

UX_FLOW(ux_confirm_flow,
  &ux_confirm_tx_init_flow_step,

  &ux_init_upper_border,
  &ux_variable_display,
  &ux_init_lower_border,

  &ux_confirm_tx_finalize_step,
  &ux_reject_tx_flow_step
);


void display_next_state(bool is_upper_border) {
    PRINTF(
        "display_next_state invoked. is_upper_border = %d, current_state = %d, formatter_index = "
        "%d, current_data_index = %d\n",
        is_upper_border,
        current_state,
        formatter_index,
        current_data_index);
    if (is_upper_border) {  // -> from first screen
        if (current_state == OUT_OF_BORDERS) {
            current_state = INSIDE_BORDERS;
            set_state_data(true);
            ux_flow_next();
        } else {
            formatter_index -= 1;
            if (current_data_index > 0) {  // <- from middle, more screens available
                set_state_data(false);
                if (formatter_stack[formatter_index] != NULL) {
                    ux_flow_next();
                } else {
                    current_state = OUT_OF_BORDERS;
                    current_data_index = 0;
                    ux_flow_prev();
                }
            } else {  // <- from middle, no more screens available
                current_state = OUT_OF_BORDERS;
                current_data_index = 0;
                ux_flow_prev();
            }
        }
    } else  // walking over the second border
    {
        if (current_state == OUT_OF_BORDERS) {  // <- from last screen
            current_state = INSIDE_BORDERS;
            set_state_data(false);
            ux_flow_prev();
        } else {
            if ((num_data != 0 && current_data_index < num_data - 1) ||
                formatter_stack[formatter_index + 1] !=
                    NULL) {  // -> from middle, more screens available
                formatter_index += 1;
                set_state_data(true);
                /*dirty hack to have coherent behavior on bnnn_paging when there are multiple
                 * screens*/
                G_ux.flow_stack[G_ux.stack_count - 1].prev_index =
                    G_ux.flow_stack[G_ux.stack_count - 1].index - 2;
                G_ux.flow_stack[G_ux.stack_count - 1].index--;
                ux_flow_relayout();
                /*end of dirty hack*/
            } else {  // -> from middle, no more screens available
                current_state = OUT_OF_BORDERS;
                ux_flow_next();
            }
        }
    }
}


int ui_approve_tx_init(void) {
    G_context.tx_info.offset = 0;
    formatter_index = 0;
    explicit_bzero(formatter_stack, sizeof(formatter_stack));
    num_data = G_context.tx_info.txDetails.opCount;
    current_data_index = 0;
    current_state = OUT_OF_BORDERS;
    g_validate_callback = &ui_action_validate_transaction;
    ux_flow_init(0, ux_confirm_flow, NULL);
    return 0;
}
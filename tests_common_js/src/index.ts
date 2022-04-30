import {
  Operation,
  TransactionBuilder,
  Networks,
  BASE_FEE,
  Account,
  Memo,
  Keypair,
  Asset,
} from "stellar-base";

// mnemonic: 'other base behind follow wet put glad muscle unlock sell income october'
// index 0: GDUTHCF37UX32EMANXIL2WOOVEDZ47GHBTT3DYKU6EKM37SOIZXM2FN7 / SAIYWGGWU2WMXYDSK33UBQBMBDKU4TTJVY3ZIFF24H2KQDR7RQW5KAEK
// index 1: GDRMNAIPTNIJWJSL6JOF76CJORN47TDVMWERTXO2G2WKOMXGNHUFL5QX / SAE52G23WPAS7MIR2OFGILLICLXXR4K6HSXZHMKD6C33JCAVVILIWYAA
// index 2: GCJBZJSKICFGD3FJMN5RBQIIXYUNVWOI7YAHQZQKK4UAWFGW6TRBRVX3 / SCGYXI6ZHWGD5EPFCFVH37EUHA5BIFNJQJGPMXDKHD4DYA3N2MXMA3NI
// index 3: GCPWZDBYNXVQYOKUWAL34ED3GDKD6UITURJP734A4DPB5EPDSXHAM3KX / SB6YADAUJ6ZCG4X7UAKLBJEXGZZJP3S2JUBSD5ZMQNXUGWPNBECNBE6O
// index 4: GATLHLTLWJVCZ52WDZOVTXFE5YXGEQ6SGEAFEL5J52WIYSGPY7PW7BMV / SBWIYHM4LEWSVHIOJXNP66XDUNME373L25EIDEMFWXNZD56PGXEUWSYG
// index 5: GCJ644IGDW7YFNKHTWSCM37FRMFBQ2EDMZLQM4AUCRBFCW562XXC5OW3 / SA3LRNOYCV4NVVYWLX4P3CXQA3ONKBCQRZDSOVENQ2TCNZRFBEO55FXW
// index 6: GCV6BUTD2REAS3MYMXIFPAMPX24FII3HNHLLESPYLOZDNZAJ4ULXP6KU / SDWURSMB36GGY7DBV2D6QMLG5WAZGFQIZNOSCJZDHC7XEMSFNFX2PEWV
// index 7: GALWXOA7RDHCPT7EXBIVCEPQIDNS5RRD6LJORBIA2HU22ORQ6XH267VE / SAKNBXQWB47HK5IN3VI5VM6UVJTCFRIILUQMLTXVA4KNM36RGME6CSJH
// index 8: GBWFAOQTZVL76F27XDJA2YDH2WKYHHMJAOKHF3B2HFHBMNBNBGMCNKQE / SC3N32WGXFSTQQZ6YNCBLUS6QAD6YUBITJROPQ2UWLMC6HOWZ2N3I5F7
// index 9: GD3OAWFV6M5T2DWVWRQONITXSMWJA2DQ3524H7BQYCNVHJZJFWSN65IA / SAJIU6F4S6ML76JWPBE56XWUIFT77VVPMB6IFTKA6L6EIJ2CFWBFYDWO
const kp0 = Keypair.fromSecret(
  "SAIYWGGWU2WMXYDSK33UBQBMBDKU4TTJVY3ZIFF24H2KQDR7RQW5KAEK"
);
const kp1 = Keypair.fromSecret(
  "SAE52G23WPAS7MIR2OFGILLICLXXR4K6HSXZHMKD6C33JCAVVILIWYAA"
);

function getCommonTransactionBuilder() {
  let account = new Account(kp0.publicKey(), "103720918407102567");
  return new TransactionBuilder(account, {
    fee: BASE_FEE,
    networkPassphrase: Networks.PUBLIC,
    memo: Memo.text("hello world"),
    timebounds: {
      minTime: 0,
      maxTime: 1670818332, // 2022-04-27T13:24:12+00:00
    },
  });
}

// Operation Testcases
export function opCreateAccount() {
  return getCommonTransactionBuilder().addOperation(
    Operation.createAccount({
      destination: kp1.publicKey(),
      startingBalance: "100.0",
    })
  ).build();
}

export function opPaymentAssetNative() {
  return getCommonTransactionBuilder().addOperation(
    Operation.payment({
      destination: kp1.publicKey(),
      asset: new Asset("XLM"),
      amount: "100.0",
    })
  ).build();
}

export function opPaymentAssetAlphanum4() {
  return getCommonTransactionBuilder().addOperation(
    Operation.payment({
      destination: kp1.publicKey(),
      asset: new Asset("BTC", "GATEMHCCKCY67ZUCKTROYN24ZYT5GK4EQZ65JJLDHKHRUZI3EUEKMTCH"),
      amount: "100.0",
    })
  ).build();
}

export function opPaymentAssetAlphanum12() {
  return getCommonTransactionBuilder().addOperation(
    Operation.payment({
      destination: kp1.publicKey(),
      asset: new Asset("BANANANANANA", "GCDGPFKW2LUJS2ESKAS42HGOKC6VWOKEJ44TQ3ZXZAMD4ZM5FVHJHPJS"),
      amount: "100.0",
    })
  ).build();
}
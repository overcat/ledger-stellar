import Zemu, { DEFAULT_START_OPTIONS } from "@zondax/zemu";
import { APP_SEED, models } from "./common";
import { opCreateAccount, opPaymentAssetNative } from 'tests-common'
import { Keypair } from 'stellar-base'
import Str from '@ledgerhq/hw-app-str'

beforeAll(async () => {
  await Zemu.checkAndPullImage();
});

jest.setTimeout(1000 * 60 * 3);

const defaultOptions = {
  ...DEFAULT_START_OPTIONS,
  logging: true,
  custom: `-s "${APP_SEED}"`,
  X11: false,
  startText: "is ready",
};

test.each(models)("can start and stop container ($name)", async (m) => {
  let sim = new Zemu(m.path);
  try {
    await sim.start({ ...defaultOptions, model: m.name });
  } finally {
    await sim.close();
  }
});

test.each(models)("main menu ($name)", async (m) => {
  let sim = new Zemu(m.path);
  try {
    await sim.start({ ...defaultOptions, model: m.name });
    await sim.navigateAndCompareSnapshots(
      ".",
      `${m.prefix.toLowerCase()}-mainmenu`,
      [4, -4]
    );
  } finally {
    await sim.close();
  }
});

test.each(models)("app version ($name)", async (m) => {
  let sim = new Zemu(m.path);
  try {
    await sim.start({ ...defaultOptions, model: m.name });
    let transport = await sim.getTransport();
    let str = new Str(transport);
    let result = await str.getAppConfiguration();
    expect(result.version).toBe('4.0.0');
  } finally {
    await sim.close();
  }
});


describe('get public key', () => {
  test.each(models)("get public key without confirmation ($name)", async (m) => {
    let sim = new Zemu(m.path);
    try {
      await sim.start({ ...defaultOptions, model: m.name });
      let transport = await sim.getTransport();
      let str = new Str(transport);
      let result = await str.getPublicKey("44'/148'/0'", false, false);
      let kp = Keypair.fromSecret("SAIYWGGWU2WMXYDSK33UBQBMBDKU4TTJVY3ZIFF24H2KQDR7RQW5KAEK")

      expect(result).toStrictEqual({
        publicKey: kp.publicKey(),
        raw: kp.rawPublicKey()
      })
    } finally {
      await sim.close();
    }
  });

  test.each(models)("get public key with confirmation ($name)", async (m) => {
    let sim = new Zemu(m.path);
    try {
      await sim.start({ ...defaultOptions, model: m.name });
      let transport = await sim.getTransport();
      let str = new Str(transport);
      let result = str.getPublicKey("44'/148'/0'", false, true);
      let kp = Keypair.fromSecret("SAIYWGGWU2WMXYDSK33UBQBMBDKU4TTJVY3ZIFF24H2KQDR7RQW5KAEK")

      await sim.waitScreenChange()
      await sim.navigateAndCompareUntilText(".", `${m.prefix.toLowerCase()}-public-key`, 'Approve')

      expect(result).resolves.toStrictEqual({
        publicKey: kp.publicKey(),
        raw: kp.rawPublicKey()
      })
    } finally {
      await sim.close();
    }
  });
})

describe('operations', () => {
  describe.each([
    {
      txFunction: opCreateAccount,
      caseName: 'create account',
      filePath: 'op-create-account'
    },
    {
      txFunction: opPaymentAssetNative,
      caseName: 'payment with asset native',
      filePath: 'op-payment-asset-native'
    }
  ])('$caseName', (c) => {
    test.each(models)("$name", async (m) => {
      let tx = c.txFunction();
      let sim = new Zemu(m.path);
      try {
        await sim.start({ ...defaultOptions, model: m.name });
        let transport = await sim.getTransport();
        let str = new Str(transport);

        // display sequence
        await sim.clickRight()
        await sim.clickBoth()
        await sim.clickRight()
        await sim.clickBoth()

        let result = str.signTransaction("44'/148'/0'", tx.signatureBase())
        await sim.waitScreenChange()
        await sim.navigateAndCompareUntilText(".", `${m.prefix.toLowerCase()}-${c.filePath}`, 'Finalize')

        let kp = Keypair.fromSecret("SAIYWGGWU2WMXYDSK33UBQBMBDKU4TTJVY3ZIFF24H2KQDR7RQW5KAEK")
        tx.sign(kp)
        expect((await result).signature).toStrictEqual(tx.signatures[0].signature());
      } finally {
        await sim.close();
      }
    });
  })
})
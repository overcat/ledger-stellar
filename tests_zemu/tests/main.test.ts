import Zemu, { DEFAULT_START_OPTIONS } from "@zondax/zemu";
import { APP_SEED, models } from "./common";
import * as testCasesFunction from 'tests-common'
import { Keypair } from 'stellar-base'
import Str from '@ledgerhq/hw-app-str'

beforeAll(async () => {
  await Zemu.checkAndPullImage();
});

jest.setTimeout(1000 * 60 * 60);

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
  // TODO: refuse
})

describe('hash signing', () => {
  test.each(models)("hash signing mode is not enabled ($name)", async (m) => {
    let sim = new Zemu(m.path);
    try {
      await sim.start({ ...defaultOptions, model: m.name });
      let transport = await sim.getTransport();
      let str = new Str(transport);
      const hash = Buffer.from("3389e9f0f1a65f19736cacf544c2e825313e8447f569233bb8db39aa607c8889", "hex")
      expect(() => str.signHash("44'/148'/0'", hash)).rejects.toThrow(new Error("Hash signing not allowed. Have you enabled it in the app settings?"));
    } finally {
      await sim.close();
    }
  });

  test.each(models)("approve ($name)", async (m) => {
    let sim = new Zemu(m.path);
    try {
      await sim.start({ ...defaultOptions, model: m.name });
      let transport = await sim.getTransport();
      let str = new Str(transport);

      // enable hash signing
      await sim.clickRight()
      await sim.clickBoth()
      await sim.clickBoth()

      const hash = Buffer.from("3389e9f0f1a65f19736cacf544c2e825313e8447f569233bb8db39aa607c8889", "hex")
      const result = str.signHash("44'/148'/0'", hash)
      await sim.waitScreenChange()
      await sim.navigateAndCompareUntilText(".", `${m.prefix.toLowerCase()}-hash-signing-approve`, 'Approve')
      let kp = Keypair.fromSecret("SAIYWGGWU2WMXYDSK33UBQBMBDKU4TTJVY3ZIFF24H2KQDR7RQW5KAEK")
      expect((await result).signature).toStrictEqual(kp.sign(hash));
    } finally {
      await sim.close();
    }
  });

  test.each(models)("reject ($name)", async (m) => {
    let sim = new Zemu(m.path);
    try {
      await sim.start({ ...defaultOptions, model: m.name });
      let transport = await sim.getTransport();
      let str = new Str(transport);

      // enable hash signing
      await sim.clickRight()
      await sim.clickBoth()
      await sim.clickBoth()

      const hash = Buffer.from("3389e9f0f1a65f19736cacf544c2e825313e8447f569233bb8db39aa607c8889", "hex")
      expect(() => str.signHash("44'/148'/0'", hash)).rejects.toThrow(new Error("Transaction approval request was rejected"));

      await sim.waitScreenChange()
      await sim.navigateAndCompareUntilText(".", `${m.prefix.toLowerCase()}-hash-signing-reject`, 'Reject')
    } finally {
      await sim.close();
    }
  });
})

describe('transactions', () => {
  describe.each(getTestCases())('$caseName', (c) => {
    test.each(models)("device ($name)", async (m) => {
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
        await sim.waitScreenChange(1000 * 60 * 60)
        await sim.navigateAndCompareUntilText(".", `${m.prefix.toLowerCase()}-${c.filePath}`, 'Finalize', 1000 * 60 * 60)

        let kp = Keypair.fromSecret("SAIYWGGWU2WMXYDSK33UBQBMBDKU4TTJVY3ZIFF24H2KQDR7RQW5KAEK")
        tx.sign(kp)
        expect((await result).signature).toStrictEqual(tx.signatures[0].signature());
      } finally {
        await sim.close();
      }
    });
  })
})

function camelToFilePath(str: string) {
  return str.replace(/([A-Z])/g, '-$1').toLowerCase();
}

function getTestCases() {
  const casesFunction = Object.keys(testCasesFunction);
  const cases = []
  for (const rawCase of casesFunction) {
    cases.push({
      caseName: rawCase,
      filePath: camelToFilePath(rawCase),
      txFunction: (testCasesFunction as any)[rawCase]  // dirty hack
    });
  }
  return cases;
}

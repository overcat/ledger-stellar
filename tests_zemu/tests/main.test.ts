import Zemu, { DEFAULT_START_OPTIONS } from "@zondax/zemu";
import { APP_SEED, models } from "./common";

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

test.each(models)(`can start and stop container`, async function (m) {
  const sim = new Zemu(m.path);
  try {
    await sim.start({ ...defaultOptions, model: m.name });
  } finally {
    await sim.close();
  }
});

test.each(models)("main menu", async function (m) {
  const sim = new Zemu(m.path);
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

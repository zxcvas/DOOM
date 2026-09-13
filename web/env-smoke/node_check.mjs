// Headless verification that the WASM module built from real Doom sources
// loads and runs correctly under Node. Exits non-zero on any failure.
import createDoomSmoke from "../dist/doom_smoke.js";

const M = await createDoomSmoke();

const selftest = M.cwrap("selftest", "number", []);
const renderFrame = M.cwrap("render_frame", null, ["number"]);
const getFb = M.cwrap("get_framebuffer", "number", []);
const fbW = M.cwrap("framebuffer_width", "number", []);
const fbH = M.cwrap("framebuffer_height", "number", []);

const ok = selftest();
console.log(`selftest (real Doom fixed-point/trig/RNG): ${ok ? "PASS" : "FAIL"}`);
if (!ok) process.exit(1);

const w = fbW();
const h = fbH();
console.log(`framebuffer: ${w}x${h} (${w * h} indexed bytes)`);
if (w !== 320 || h !== 200) {
  console.error("unexpected framebuffer dimensions");
  process.exit(1);
}

// Render a frame and confirm the indexed buffer is populated and changes
// between animation ticks (proves FixedMul/finesine run per-frame).
renderFrame(0);
const ptr = getFb();
const frame0 = Uint8Array.from(M.HEAPU8.subarray(ptr, ptr + w * h));
renderFrame(50);
const frame50 = Uint8Array.from(M.HEAPU8.subarray(ptr, ptr + w * h));

let distinct = new Set(frame0).size;
let changed = 0;
for (let i = 0; i < frame0.length; i++) if (frame0[i] !== frame50[i]) changed++;

console.log(`distinct palette indices in frame 0: ${distinct}`);
console.log(`pixels changed between tic 0 and tic 50: ${changed}/${w * h}`);

if (distinct < 8 || changed === 0) {
  console.error("framebuffer looks static/empty — render path not exercised");
  process.exit(1);
}

console.log("RESULT: environment smoke test PASSED");

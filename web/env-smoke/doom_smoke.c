// Environment smoke test for the Linux Doom browser/WASM port.
//
// This is NOT the game and NOT a renderer. It exists only to prove that the
// Cloud Agent environment can compile the repository's real, portable C
// (fixed-point math, trig LUTs, RNG) to WebAssembly with Emscripten and run
// it correctly in a browser via the documented indexed-framebuffer model
// (8-bit palette indices on the CPU, RGB expansion in the compositor/JS).
//
// It deliberately reuses the unmodified engine sources m_fixed.c, tables.c and
// m_random.c so a green result means the toolchain handles the actual codebase,
// not a hand-written stand-in. The software renderer (r_*.c) is untouched.

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <emscripten.h>

#include "m_fixed.h"
#include "tables.h"

// finesine[] lives in tables.c; finecosine is a PI/2 phase-shifted view that
// the engine defines in r_main.c. We provide it here so the smoke test does
// not have to pull in the full renderer translation unit.
fixed_t* finecosine = &finesine[FINEANGLES / 4];

// m_random.c exposes these.
extern int P_Random(void);
extern void M_ClearRandom(void);

// m_fixed.c references I_Error for its divide guard. Provide a minimal version
// so we do not have to link the platform layer.
void I_Error(char* error, ...)
{
    va_list ap;
    va_start(ap, error);
    vfprintf(stderr, error, ap);
    va_end(ap);
    fputc('\n', stderr);
    abort();
}

#define SMOKE_W 320
#define SMOKE_H 200

static unsigned char framebuffer[SMOKE_W * SMOKE_H];
static unsigned char palette[256 * 3];

EMSCRIPTEN_KEEPALIVE
void init_palette(void)
{
    // A built-in fire palette. The real port will replace this with the WAD's
    // PLAYPAL lump; without a user-supplied IWAD we cannot ship a real palette.
    for (int i = 0; i < 256; i++) {
        int r = i < 85 ? i * 3 : 255;
        int g = i < 85 ? 0 : (i < 170 ? (i - 85) * 3 : 255);
        int b = i < 170 ? 0 : (i - 170) * 3;
        palette[i * 3 + 0] = (unsigned char)r;
        palette[i * 3 + 1] = (unsigned char)g;
        palette[i * 3 + 2] = (unsigned char)b;
    }
}

// Verify the real engine math returns the exact values the native build does.
// Returns 1 on success, 0 on any mismatch.
EMSCRIPTEN_KEEPALIVE
int selftest(void)
{
    int ok = 1;

    if (FixedMul(2 * FRACUNIT, 3 * FRACUNIT) != 6 * FRACUNIT) ok = 0;
    if (FixedDiv(6 * FRACUNIT, 3 * FRACUNIT) != 2 * FRACUNIT) ok = 0;

    // Values captured from the native gcc build of tables.c.
    if (finesine[0] != 25) ok = 0;
    if (finesine[2048] != 65535) ok = 0;
    if (finecosine[0] != 65535) ok = 0;

    // Deterministic RNG: first draw after a clear is rndtable[1] == 8.
    M_ClearRandom();
    if (P_Random() != 8) ok = 0;

    return ok;
}

EMSCRIPTEN_KEEPALIVE
unsigned char* get_framebuffer(void) { return framebuffer; }

EMSCRIPTEN_KEEPALIVE
int framebuffer_width(void) { return SMOKE_W; }

EMSCRIPTEN_KEEPALIVE
int framebuffer_height(void) { return SMOKE_H; }

EMSCRIPTEN_KEEPALIVE
unsigned char* get_palette(void) { return palette; }

// Fill the indexed framebuffer with a rotozoom driven by the engine's own
// finesine/finecosine LUTs and FixedMul. This exercises real Doom fixed-point
// arithmetic every frame and writes palette indices exactly like the engine's
// software column drawer would.
EMSCRIPTEN_KEEPALIVE
void render_frame(int tic)
{
    unsigned ang = ((unsigned)tic * 8u) & (FINEANGLES - 1);
    fixed_t c = finecosine[ang];
    fixed_t s = finesine[ang];

    for (int y = 0; y < SMOKE_H; y++) {
        for (int x = 0; x < SMOKE_W; x++) {
            fixed_t fx = ((x - SMOKE_W / 2) * FRACUNIT) / 96;
            fixed_t fy = ((y - SMOKE_H / 2) * FRACUNIT) / 96;

            fixed_t u = FixedMul(fx, c) - FixedMul(fy, s);
            fixed_t v = FixedMul(fx, s) + FixedMul(fy, c);

            int iu = u >> 12;
            int iv = v >> 12;
            int idx = (iu * iu + iv * iv) + tic;

            framebuffer[y * SMOKE_W + x] = (unsigned char)(idx & 0xff);
        }
    }
}

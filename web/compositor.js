// Phase 1 present path: 2D-canvas palette expand + integer upscale + 4:3.
//
// Contract (see docs/linuxdoom-browser-port.md sections 5, 9): the software
// renderer's screens[0] (320x200 palette indices) and the current PLAYPAL are
// read-only inputs. We expand indices -> RGB on the CPU here for Phase 1; the
// WebGL LUT compositor is Phase 2. We never bilinear-filter the index buffer.
//
// The buffer is authored for 320x200 shown on a 4:3 display (rectangular
// pixels), so the default present stretches to 4:3.

(function () {
  const DoomCompositor = {
    canvas: null,
    ctx: null,
    off: null,      // offscreen 320x200 canvas
    offCtx: null,
    image: null,    // ImageData 320x200
    w: 320,
    h: 200,
    scale: 3,
    aspect: "43",

    init(canvas, w, h) {
      this.canvas = canvas;
      this.w = w;
      this.h = h;
      this.ctx = canvas.getContext("2d", { alpha: false });
      this.ctx.imageSmoothingEnabled = false;

      this.off = document.createElement("canvas");
      this.off.width = w;
      this.off.height = h;
      this.offCtx = this.off.getContext("2d", { alpha: false });
      this.image = this.offCtx.createImageData(w, h);
      // Force alpha to opaque up front.
      const d = this.image.data;
      for (let i = 3; i < d.length; i += 4) d[i] = 255;

      this.resize();
    },

    setScale(scale) { this.scale = scale; this.resize(); },
    setAspect(aspect) { this.aspect = aspect; this.resize(); },

    resize() {
      let cw, ch;
      if (this.scale === 0) {
        // Fit the stage while preserving the chosen aspect.
        const stage = this.canvas.parentElement.getBoundingClientRect();
        const targetH = this.aspect === "43" ? this.h * 1.2 : this.h;
        const s = Math.max(
          1,
          Math.min(stage.width / this.w, (stage.height - 2) / targetH)
        );
        cw = Math.floor(this.w * s);
        ch = Math.floor(targetH * s);
      } else {
        cw = this.w * this.scale;
        ch = (this.aspect === "43" ? this.h * 1.2 : this.h) * this.scale;
        ch = Math.round(ch);
      }
      this.canvas.width = cw;
      this.canvas.height = ch;
      this.ctx.imageSmoothingEnabled = false;
    },

    // fb: Uint8Array of w*h palette indices; pal: Uint8Array of 256*3 RGB.
    present(fb, pal) {
      const d = this.image.data;
      const n = this.w * this.h;
      for (let i = 0, j = 0; i < n; i++, j += 4) {
        const c = fb[i] * 3;
        d[j] = pal[c];
        d[j + 1] = pal[c + 1];
        d[j + 2] = pal[c + 2];
        // alpha stays 255
      }
      this.offCtx.putImageData(this.image, 0, 0);
      this.ctx.imageSmoothingEnabled = false;
      this.ctx.drawImage(
        this.off,
        0, 0, this.w, this.h,
        0, 0, this.canvas.width, this.canvas.height
      );
    },
  };

  window.DoomCompositor = DoomCompositor;
})();

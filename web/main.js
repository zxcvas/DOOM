// Glue: boot the WASM module, load a user IWAD into MEMFS, run D_DoomMain, then
// drive D_DoomFrame from requestAnimationFrame (no blocking loop). See
// docs/linuxdoom-browser-port.md sections 8 and 11.

(function () {
  const $ = (id) => document.getElementById(id);
  const statusEl = $("status");
  const startBtn = $("startBtn");
  const wadInput = $("wadInput");
  const fileLabel = $("fileLabel");
  const iwadType = $("iwadType");
  const scaleSel = $("scaleSel");
  const aspectSel = $("aspectSel");
  const stage = $("stage");
  const dropHint = $("dropHint");
  const canvas = $("screen");

  let Module = null;
  let selectedFile = null;
  let running = false;

  function log(msg) {
    statusEl.textContent += (statusEl.textContent ? "\n" : "") + msg;
    statusEl.scrollTop = statusEl.scrollHeight;
  }
  function setStatus(msg) {
    statusEl.textContent = msg;
  }

  // Map an arbitrary IWAD filename to a name D_IdentifyVersion recognizes.
  function canonicalIwadName(filename) {
    const f = filename.toLowerCase();
    if (f.includes("doom2f")) return "doom2f.wad";
    if (f.includes("doom2") || f.includes("freedoom2")) return "doom2.wad";
    if (f.includes("tnt")) return "tnt.wad";
    if (f.includes("plutonia")) return "plutonia.wad";
    // Freedoom phase 1 is a full multi-episode replacement -> registered.
    if (f.includes("freedoom1")) return "doom.wad";
    if (f.includes("doom1")) return "doom1.wad"; // shareware
    if (f.includes("doomu")) return "doomu.wad"; // Ultimate
    return "doom.wad";
  }

  function targetName(filename) {
    const sel = iwadType.value;
    return sel === "auto" ? canonicalIwadName(filename) : sel;
  }

  // --- Compositor wiring -------------------------------------------------
  function applyDisplay() {
    DoomCompositor.setScale(parseInt(scaleSel.value, 10));
    DoomCompositor.setAspect(aspectSel.value);
  }
  scaleSel.addEventListener("change", applyDisplay);
  aspectSel.addEventListener("change", applyDisplay);
  window.addEventListener("resize", () => {
    if (parseInt(scaleSel.value, 10) === 0) DoomCompositor.resize();
  });

  // --- Frame loop --------------------------------------------------------
  function present() {
    const fbPtr = Module._Doom_FrameBuffer();
    const palPtr = Module._Doom_Palette();
    const w = Module._Doom_Width();
    const h = Module._Doom_Height();
    const heap = Module.HEAPU8;
    const fb = heap.subarray(fbPtr, fbPtr + w * h);
    const pal = heap.subarray(palPtr, palPtr + 256 * 3);
    DoomCompositor.present(fb, pal);
  }

  function frame() {
    if (!running) return;
    try {
      Module._D_DoomFrame();
    } catch (e) {
      running = false;
      log("Frame loop stopped: " + e);
      return;
    }
    present();
    requestAnimationFrame(frame);
  }

  function startGame() {
    if (!Module || !selectedFile || running) return;
    const name = targetName(selectedFile.name);
    const reader = new FileReader();
    reader.onload = () => {
      const bytes = new Uint8Array(reader.result);
      try {
        Module.FS.writeFile("/" + name, bytes);
      } catch (e) {
        log("Failed to write WAD to FS: " + e);
        return;
      }
      log(`Loaded ${selectedFile.name} -> /${name} (${bytes.length} bytes)`);
      log("Starting D_DoomMain…");

      DoomInput.attach((down, key) => Module._Doom_PostKey(down, key));

      try {
        Module.callMain([]);
      } catch (e) {
        // ExitStatus from a clean quit is fine; anything else we report.
        if (!(e && e.name === "ExitStatus")) log("D_DoomMain error: " + e);
      }

      dropHint.style.display = "none";
      startBtn.disabled = true;
      wadInput.disabled = true;
      running = true;
      canvas.focus();
      requestAnimationFrame(frame);
      log("Running. Click the canvas and use the keyboard.");
    };
    reader.onerror = () => log("Could not read file: " + reader.error);
    reader.readAsArrayBuffer(selectedFile);
  }

  function pickFile(file) {
    selectedFile = file;
    fileLabel.textContent = file.name;
    startBtn.disabled = !Module;
    log(`Selected ${file.name}. Interpretation: ${targetName(file.name)}.`);
  }

  wadInput.addEventListener("change", (e) => {
    if (e.target.files && e.target.files[0]) pickFile(e.target.files[0]);
  });
  startBtn.addEventListener("click", startGame);

  // Drag & drop onto the stage.
  ["dragenter", "dragover"].forEach((ev) =>
    stage.addEventListener(ev, (e) => {
      e.preventDefault();
      stage.classList.add("dragover");
    })
  );
  ["dragleave", "drop"].forEach((ev) =>
    stage.addEventListener(ev, (e) => {
      e.preventDefault();
      stage.classList.remove("dragover");
    })
  );
  stage.addEventListener("drop", (e) => {
    if (e.dataTransfer.files && e.dataTransfer.files[0]) {
      pickFile(e.dataTransfer.files[0]);
    }
  });

  // Optionally auto-load an IWAD from a URL (e.g. one you self-host):
  //   index.html?wad=path/or/url/to/your.wad
  // Handy for self-hosting a WAD you own; never point this at a WAD you may
  // not legally distribute.
  function autoLoadFromUrl(url) {
    log("Fetching WAD from " + url + " …");
    fetch(url)
      .then((r) => {
        if (!r.ok) throw new Error("HTTP " + r.status);
        return r.arrayBuffer();
      })
      .then((buf) => {
        const name = url.split("/").pop().split("?")[0] || "doom.wad";
        selectedFile = new File([buf], name);
        fileLabel.textContent = name;
        log(`Fetched ${name} (${buf.byteLength} bytes).`);
        startGame();
      })
      .catch((e) => log("WAD fetch failed: " + e));
  }

  // --- Boot the module ---------------------------------------------------
  DoomCompositor.init(canvas, 320, 200);
  applyDisplay();

  setStatus("Loading WebAssembly…");
  createDoomModule({
    noInitialRun: true,
    preRun: [
      function () {
        // The engine's IWAD search uses $DOOMWADDIR (default ".") and needs
        // $HOME for its config file. Point both at the MEMFS root.
        this.ENV.HOME = "/";
        this.ENV.DOOMWADDIR = "/";
      },
    ],
    print: (t) => log(t),
    printErr: (t) => log(t),
    onDoomError: (msg) => log("I_Error: " + msg),
    onDoomQuit: () => {
      running = false;
      log("Game quit.");
    },
  })
    .then((mod) => {
      Module = mod;
      setStatus("WebAssembly ready. Choose an IWAD to begin.");
      if (selectedFile) startBtn.disabled = false;
      const wadUrl = new URLSearchParams(location.search).get("wad");
      if (wadUrl) autoLoadFromUrl(wadUrl);
    })
    .catch((e) => log("Module load failed: " + e));
})();

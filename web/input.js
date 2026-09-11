// DOM keyboard -> Doom key codes (doomdef.h) -> D_PostEvent via Doom_PostKey.
//
// Only the platform seam is touched: we translate browser events and push them
// into the engine's event ring. Gameplay bindings (fire=Ctrl, use=Space, etc.)
// are Doom's own defaults.

(function () {
  // Doom key constants (doomdef.h).
  const K = {
    RIGHTARROW: 0xae,
    LEFTARROW: 0xac,
    UPARROW: 0xad,
    DOWNARROW: 0xaf,
    ESCAPE: 27,
    ENTER: 13,
    TAB: 9,
    BACKSPACE: 127,
    PAUSE: 0xff,
    EQUALS: 0x3d,
    MINUS: 0x2d,
    RSHIFT: 0x80 + 0x36,
    RCTRL: 0x80 + 0x1d,
    RALT: 0x80 + 0x38,
    F1: 0x80 + 0x3b,
    F2: 0x80 + 0x3c,
    F3: 0x80 + 0x3d,
    F4: 0x80 + 0x3e,
    F5: 0x80 + 0x3f,
    F6: 0x80 + 0x40,
    F7: 0x80 + 0x41,
    F8: 0x80 + 0x42,
    F9: 0x80 + 0x43,
    F10: 0x80 + 0x44,
    F11: 0x80 + 0x57,
    F12: 0x80 + 0x58,
  };

  const byCode = {
    ArrowRight: K.RIGHTARROW,
    ArrowLeft: K.LEFTARROW,
    ArrowUp: K.UPARROW,
    ArrowDown: K.DOWNARROW,
    Escape: K.ESCAPE,
    Enter: K.ENTER,
    NumpadEnter: K.ENTER,
    Tab: K.TAB,
    Backspace: K.BACKSPACE,
    Delete: K.BACKSPACE,
    Pause: K.PAUSE,
    Equal: K.EQUALS,
    Minus: K.MINUS,
    ShiftLeft: K.RSHIFT,
    ShiftRight: K.RSHIFT,
    ControlLeft: K.RCTRL,
    ControlRight: K.RCTRL,
    AltLeft: K.RALT,
    AltRight: K.RALT,
    Space: 32,
    F1: K.F1, F2: K.F2, F3: K.F3, F4: K.F4, F5: K.F5, F6: K.F6,
    F7: K.F7, F8: K.F8, F9: K.F9, F10: K.F10, F11: K.F11, F12: K.F12,
  };

  // Keys we consume so the browser doesn't scroll / change focus / open menus.
  const swallow = new Set([
    "ArrowRight", "ArrowLeft", "ArrowUp", "ArrowDown", "Space", "Tab",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
    "Enter", "NumpadEnter",
  ]);

  function toDoomKey(e) {
    if (byCode.hasOwnProperty(e.code)) return byCode[e.code];

    // Letters -> lowercase ASCII.
    if (e.code.startsWith("Key")) {
      return e.code.charCodeAt(3) + 32; // 'A' + 32 -> 'a'
    }
    // Top-row and numpad digits -> ASCII digit.
    if (e.code.startsWith("Digit")) return e.code.charCodeAt(5);
    if (e.code.startsWith("Numpad") && e.code.length === 7) {
      const d = e.code.charCodeAt(6);
      if (d >= 48 && d <= 57) return d;
    }
    // Fall back to a single printable char from e.key.
    if (e.key && e.key.length === 1) {
      let c = e.key.charCodeAt(0);
      if (c >= 65 && c <= 90) c += 32;
      return c;
    }
    return 0;
  }

  const DoomInput = {
    send: null,
    attached: false,

    attach(sendKey) {
      if (this.attached) return;
      this.attached = true;
      this.send = sendKey;

      window.addEventListener("keydown", (e) => {
        if (e.repeat) { if (swallow.has(e.code)) e.preventDefault(); return; }
        const k = toDoomKey(e);
        if (k) this.send(1, k);
        if (swallow.has(e.code)) e.preventDefault();
      });

      window.addEventListener("keyup", (e) => {
        const k = toDoomKey(e);
        if (k) this.send(0, k);
        if (swallow.has(e.code)) e.preventDefault();
      });
    },
  };

  window.DoomInput = DoomInput;
})();

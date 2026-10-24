// Shared helpers: safe element builder, clipboard copy with visible feedback, relative and absolute dates.
(function () {
  "use strict";
  var live = document.getElementById("sr-live");

  App.el = function (tag, cls, text) {
    var e = document.createElement(tag);
    if (cls) e.className = cls;
    if (text !== undefined) e.textContent = text;
    return e;
  };

  function locale() { return App.lang() === "es" ? "es-ES" : "en-GB"; }

  App.abs = function (iso) {
    var d = new Date(iso);
    return isNaN(d) ? "" : d.toLocaleDateString(locale(), { year: "numeric", month: "long", day: "numeric" });
  };

  // "3 days ago" / "hace 3 días" (falls back to the absolute date if Intl.RelativeTimeFormat is missing).
  App.rel = function (iso) {
    var d = new Date(iso);
    if (isNaN(d)) return "";
    try {
      var rtf = new Intl.RelativeTimeFormat(locale(), { numeric: "auto" });
      var s = Math.round((d.getTime() - Date.now()) / 1000), a = Math.abs(s);
      var units = [["year", 31536000], ["month", 2592000], ["day", 86400], ["hour", 3600], ["minute", 60]];
      for (var i = 0; i < units.length; i++) {
        if (a >= units[i][1]) return rtf.format(Math.trunc(s / units[i][1]), units[i][0]);
      }
      return rtf.format(0, "second");
    } catch (e) {
      return App.abs(iso);
    }
  };

  function announce(msg) {
    if (!live) return;
    live.textContent = "";
    setTimeout(function () { live.textContent = msg; }, 30);
  }

  // Rebuilds the buttons of `container` with fn() and gives the focus back to the button that had it (the keyboard keeps its place)
  App.keepFocus = function (container, fn) {
    var a = document.activeElement, idx = -1;
    if (a && container.contains(a)) idx = Array.prototype.indexOf.call(container.children, a);
    fn();
    if (idx >= 0 && container.children[idx]) container.children[idx].focus({ preventScroll: true });
  };

  function legacyCopy(text) {
    var ta = document.createElement("textarea");
    ta.value = text; ta.setAttribute("readonly", ""); ta.className = "sr";
    var host = (document.activeElement && document.activeElement.closest && document.activeElement.closest("dialog")) || document.body; // (a modal dialog makes the rest of the page inert)
    var prev = document.activeElement;
    host.appendChild(ta);
    ta.focus({ preventScroll: true });
    ta.select();
    var ok = false;
    try { ok = document.execCommand("copy"); } catch (e) {}
    ta.remove();
    if (prev && prev.focus) prev.focus({ preventScroll: true });
    return ok;
  }

  // Copies text; btn (optional) shows "Copied!" for a moment and goes back to the i18n label `key`.
  App.copy = function (text, btn, key) {
    function done(ok) {
      var msg = App.t(ok ? "copied" : "copy_failed");
      announce(msg);
      if (!btn) return;
      btn.textContent = msg;
      btn.classList.toggle("copied", ok);
      clearTimeout(btn._tm);
      btn._tm = setTimeout(function () { btn.textContent = App.t(key); btn.classList.remove("copied"); }, 1800);
    }
    if (navigator.clipboard && window.isSecureContext) {
      navigator.clipboard.writeText(text).then(function () { done(true); }, function () { done(legacyCopy(text)); });
    } else done(legacyCopy(text));
  };
})();

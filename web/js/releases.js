// Releases section: loads every release from GitHub, lets you download a version or open its QR (for FBI).
(function () {
  "use strict";
  var API = "https://api.github.com/repos/IkerPlays/Iker-s-Tube/releases?per_page=100&page=";
  var CACHE_KEY = "ikt-releases-v1", CACHE_MS = 10 * 60 * 1000, PAGE = 12;
  var t = App.t;
  var all = [], filter = "all", query = "", shown = PAGE, latestTag = "", failed = false, loaded = false;
  var elList = document.getElementById("rel-list"), elStatus = document.getElementById("rel-status");
  var elFilter = document.getElementById("rel-filter"), elSearch = document.getElementById("rel-search");
  var elMore = document.getElementById("rel-more"), elLatest = document.getElementById("latest");

  function el(tag, cls, text) {
    var e = document.createElement(tag);
    if (cls) e.className = cls;
    if (text !== undefined) e.textContent = text;
    return e;
  }
  function size(b) { return b >= 1048576 ? (b / 1048576).toFixed(1) + " MB" : Math.max(1, Math.round(b / 1024)) + " KB"; }
  function day(iso) {
    var d = new Date(iso);
    return isNaN(d) ? "" : d.toLocaleDateString(App.lang() === "es" ? "es-ES" : "en-GB", { year: "numeric", month: "short", day: "numeric" });
  }

  // Safe mini markdown (headings, bullets, **bold**, `code`) built with DOM nodes, never innerHTML.
  function inline(parent, text) {
    text.split(/(\*\*[^*]+\*\*|`[^`]+`)/).forEach(function (p) {
      if (!p) return;
      if (p.slice(0, 2) === "**" && p.slice(-2) === "**" && p.length > 4) parent.appendChild(el("strong", "", p.slice(2, -2)));
      else if (p[0] === "`" && p.slice(-1) === "`" && p.length > 2) parent.appendChild(el("code", "", p.slice(1, -1)));
      else parent.appendChild(document.createTextNode(p));
    });
  }
  function notes(body) {
    var box = el("div", "notes"), ul = null;
    String(body || "").split(/\r?\n/).forEach(function (line) {
      line = line.replace(/\s+$/, "");
      var m;
      if (/^---+$/.test(line) || !line) { ul = null; return; }
      if ((m = /^#{1,2}\s+(.*)$/.exec(line))) { ul = null; return; } // the title repeats the release name
      if ((m = /^#{3,6}\s+(.*)$/.exec(line))) { ul = null; box.appendChild(el("h5", "", m[1])); return; }
      if ((m = /^[-*]\s+(.*)$/.exec(line))) {
        if (!ul) { ul = el("ul"); box.appendChild(ul); }
        var li = el("li"); inline(li, m[1]); ul.appendChild(li); return;
      }
      ul = null;
      var p = el("p"); inline(p, line); box.appendChild(p);
    });
    return box;
  }

  function asset(r, name) {
    for (var i = 0; i < r.assets.length; i++) if (r.assets[i].name === name) return r.assets[i];
    return null;
  }

  function card(r) {
    var li = el("li", "rel" + (r.pre ? " dev" : ""));
    var head = el("div", "rel-head");
    var title = el("h3", "", (r.name || r.tag).replace(/^Iker's Tube\s*/, "") || r.tag);
    head.appendChild(title);
    if (r.tag === latestTag) head.appendChild(el("span", "badge latest", t("rel_latest")));
    head.appendChild(el("span", "badge " + (r.pre ? "dev" : "stable"), r.pre ? t("rel_dev_badge") : t("rel_stable_badge")));
    li.appendChild(head);
    var dl = r.assets.reduce(function (n, a) { return n + (a.dl || 0); }, 0);
    li.appendChild(el("p", "small", [day(r.date), r.tag, dl + " " + t("rel_downloads")].filter(Boolean).join(" · ")));

    var row = el("div", "row");
    var cia = asset(r, "IkersTube.cia"), dsx = asset(r, "IkersTube.3dsx");
    if (cia) { var a = el("a", "btn small-btn", ".cia (" + size(cia.size) + ")"); a.href = cia.url; row.appendChild(a); }
    if (dsx) { var b = el("a", "btn small-btn ghost", ".3dsx (" + size(dsx.size) + ")"); b.href = dsx.url; row.appendChild(b); }
    if (cia || dsx) {
      var q = el("button", "btn small-btn ghost", t("rel_qr")); q.type = "button";
      q.addEventListener("click", function () { openQr(r, cia, dsx); });
      row.appendChild(q);
    } else row.appendChild(el("span", "small", t("rel_no_files")));
    var g = el("a", "btn small-btn ghost", t("rel_page")); g.href = r.url; row.appendChild(g);
    li.appendChild(row);

    if (r.body) {
      var det = el("details", "rel-notes"), sum = el("summary", "", t("rel_notes"));
      det.appendChild(sum);
      var filled = false;
      det.addEventListener("toggle", function () { if (det.open && !filled) { filled = true; det.appendChild(notes(r.body)); } });
      li.appendChild(det);
    }
    return li;
  }

  function visible() {
    var q = query.trim().toLowerCase();
    return all.filter(function (r) {
      if (filter === "stable" && r.pre) return false;
      if (filter === "dev" && !r.pre) return false;
      return !q || (r.tag + " " + (r.name || "")).toLowerCase().indexOf(q) >= 0;
    });
  }

  function render() {
    elFilter.textContent = "";
    [["all", "rel_all"], ["stable", "rel_stable"], ["dev", "rel_dev"]].forEach(function (f) {
      var b = el("button", "chip", t(f[1])); b.type = "button";
      b.setAttribute("aria-pressed", String(filter === f[0]));
      b.addEventListener("click", function () { filter = f[0]; shown = PAGE; render(); });
      elFilter.appendChild(b);
    });
    elList.textContent = "";
    if (failed) { elStatus.textContent = t("rel_error"); elMore.hidden = true; return; }
    if (!loaded) { elStatus.textContent = t("rel_loading"); elMore.hidden = true; return; }
    var list = visible();
    elStatus.textContent = list.length ? list.length + " " + t("rel_count") : t("rel_none");
    list.slice(0, shown).forEach(function (r) { elList.appendChild(card(r)); });
    elMore.hidden = list.length <= shown;
  }

  function setLatest() {
    if (!elLatest) return;
    if (failed) { elLatest.textContent = t("latest_fail"); return; }
    if (!loaded) return;
    elLatest.textContent = latestTag ? t("latest") + latestTag.replace(/^v/, "") : t("latest_fail");
  }

  // ----- QR dialog -----
  var dlg = document.getElementById("qr-dialog"), cv = document.getElementById("qr-canvas");
  var qrTitle = document.getElementById("qr-title"), qrUrl = document.getElementById("qr-url");
  function drawQr(url) {
    var qr = qrcode(0, "M");
    qr.addData(url);
    qr.make();
    var n = qr.getModuleCount(), cell = Math.floor(cv.width / (n + 8)), off = Math.floor((cv.width - cell * n) / 2);
    var ctx = cv.getContext("2d");
    ctx.fillStyle = "#fff"; ctx.fillRect(0, 0, cv.width, cv.height);
    ctx.fillStyle = "#000";
    for (var r = 0; r < n; r++) for (var c = 0; c < n; c++) if (qr.isDark(r, c)) ctx.fillRect(off + c * cell, off + r * cell, cell, cell);
  }
  function openQr(r, cia, dsx) {
    var a = cia || dsx;
    qrTitle.textContent = t("qr_for") + (r.name || r.tag) + " (" + (cia ? ".cia" : ".3dsx") + ")";
    qrUrl.textContent = a.url;
    drawQr(a.url);
    if (dlg.showModal) dlg.showModal(); else dlg.setAttribute("open", "");
  }
  document.getElementById("qr-save").addEventListener("click", function () {
    cv.toBlob(function (blob) {
      if (!blob) return;
      var u = URL.createObjectURL(blob), a = document.createElement("a");
      a.href = u; a.download = "IkersTube-qr.png"; document.body.appendChild(a); a.click(); a.remove();
      setTimeout(function () { URL.revokeObjectURL(u); }, 1000);
    });
  });
  dlg.addEventListener("click", function (e) { if (e.target === dlg) dlg.close(); });

  // ----- data -----
  function slim(x) {
    return {
      tag: x.tag_name, name: x.name || x.tag_name, body: x.body || "", pre: !!x.prerelease, draft: !!x.draft,
      date: x.published_at || x.created_at, url: x.html_url,
      assets: (x.assets || []).map(function (a) { return { name: a.name, size: a.size, url: a.browser_download_url, dl: a.download_count }; })
    };
  }
  function ready(list) {
    all = list.filter(function (r) { return !r.draft; });
    var stable = all.filter(function (r) { return !r.pre; }).sort(function (a, b) { return new Date(b.date) - new Date(a.date); });
    latestTag = stable.length ? stable[0].tag : "";
    loaded = true; failed = false;
    render(); setLatest();
  }
  function fetchPage(p, acc) {
    return fetch(API + p, { headers: { Accept: "application/vnd.github+json" } }).then(function (res) {
      if (!res.ok) throw new Error("http " + res.status);
      return res.json();
    }).then(function (arr) {
      acc = acc.concat(arr.map(slim));
      return arr.length === 100 && p < 5 ? fetchPage(p + 1, acc) : acc;
    });
  }
  function load() {
    try {
      var c = JSON.parse(sessionStorage.getItem(CACHE_KEY) || "null");
      if (c && Date.now() - c.at < CACHE_MS && c.data && c.data.length) { ready(c.data); return; }
    } catch (e) {}
    fetchPage(1, []).then(function (list) {
      try { sessionStorage.setItem(CACHE_KEY, JSON.stringify({ at: Date.now(), data: list })); } catch (e) {}
      ready(list);
    }).catch(function () { failed = true; render(); setLatest(); });
  }

  elSearch.addEventListener("input", function () { query = elSearch.value; shown = PAGE; render(); });
  elMore.addEventListener("click", function () { shown += PAGE; render(); });
  App.onLang(function () { render(); setLatest(); });
  render();
  load();
})();

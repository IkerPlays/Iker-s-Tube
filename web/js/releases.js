// Releases section: loads every release from GitHub, lets you download a version, copy links, check hashes or open its QR (for FBI).
(function () {
  "use strict";
  var API = "https://api.github.com/repos/IkerPlays/Iker-s-Tube/releases?per_page=100&page=";
  var CACHE_KEY = "ikt-releases-v2", CACHE_MS = 10 * 60 * 1000, PAGE = 12;
  var t = App.t, el = App.el;
  var all = [], filter = "all", query = "", shown = PAGE, latestTag = "", latestRel = null;
  var failed = false, loaded = false, cachedAt = 0, newestFirst = true, hlTag = "";
  var elList = document.getElementById("rel-list"), elStatus = document.getElementById("rel-status");
  var elFilter = document.getElementById("rel-filter"), elSearch = document.getElementById("rel-search");
  var elMore = document.getElementById("rel-more"), elLatest = document.getElementById("latest");
  var elSort = document.getElementById("rel-sort"), elStats = document.getElementById("rel-stats");
  var elAlert = document.getElementById("rel-alert"), elHero = document.getElementById("hero-card");

  function size(b) { return b >= 1048576 ? (b / 1048576).toFixed(1) + " MB" : Math.max(1, Math.round(b / 1024)) + " KB"; }
  function num(n) { return n.toLocaleString(App.lang() === "es" ? "es-ES" : "en-GB"); }

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
  // First n bullet points of the notes (or, if there are none, the first plain lines).
  function bullets(body, n) {
    var lines = String(body || "").split(/\r?\n/), out = [], plain = [], m;
    lines.forEach(function (l) {
      l = l.replace(/\s+$/, "");
      if ((m = /^\s*[-*]\s+(.*)$/.exec(l))) out.push(m[1]);
      else if (l && !/^#|^---/.test(l)) plain.push(l);
    });
    return (out.length ? out : plain).slice(0, n);
  }

  function asset(r, name) {
    for (var i = 0; i < r.assets.length; i++) if (r.assets[i].name === name) return r.assets[i];
    return null;
  }
  function find(tag) {
    for (var i = 0; i < all.length; i++) if (all[i].tag === tag) return all[i];
    return null;
  }
  function pageUrl(tag) { return location.origin + location.pathname + "#" + tag; }

  function copyBtn(label, key, text, cls) {
    var b = el("button", "btn small-btn ghost" + (cls ? " " + cls : ""), label); b.type = "button";
    b.addEventListener("click", function () { App.copy(typeof text === "function" ? text() : text, b, key); });
    return b;
  }

  function card(r) {
    var li = el("li", "rel" + (r.pre ? " dev" : "") + (r.tag === hlTag ? " hl" : ""));
    li.id = r.tag;
    var head = el("div", "rel-head");
    head.appendChild(el("h3", "", (r.name || r.tag).replace(/^Iker's Tube\s*/, "") || r.tag));
    if (r.tag === latestTag) head.appendChild(el("span", "badge latest", t("rel_latest")));
    head.appendChild(el("span", "badge " + (r.pre ? "dev" : "stable"), r.pre ? t("rel_dev_badge") : t("rel_stable_badge")));
    li.appendChild(head);

    var dl = r.assets.reduce(function (n, a) { return n + (a.dl || 0); }, 0);
    var meta = el("p", "small meta");
    if (r.date) {
      var tm = el("time", "", App.rel(r.date)); tm.setAttribute("datetime", r.date); tm.title = App.abs(r.date);
      meta.appendChild(tm);
      meta.appendChild(document.createTextNode(" · "));
    }
    meta.appendChild(document.createTextNode(r.tag + " · " + num(dl) + " " + t("rel_downloads")));
    li.appendChild(meta);

    var cia = asset(r, "IkersTube.cia"), dsx = asset(r, "IkersTube.3dsx");
    var row = el("div", "row");
    if (cia) { var a = el("a", "btn small-btn", ".cia (" + size(cia.size) + ")"); a.href = cia.url; row.appendChild(a); }
    if (dsx) { var b = el("a", "btn small-btn ghost", ".3dsx (" + size(dsx.size) + ")"); b.href = dsx.url; row.appendChild(b); }
    if (!cia && !dsx) row.appendChild(el("span", "small", t("rel_no_files")));
    li.appendChild(row);

    var tools = el("div", "row tools");
    if (cia || dsx) {
      var q = el("button", "btn small-btn ghost", t("rel_qr")); q.type = "button";
      q.addEventListener("click", function () { openQr(r, cia, dsx, q); });
      tools.appendChild(q);
      tools.appendChild(copyBtn(t("rel_copy_link"), "rel_copy_link", (cia || dsx).url));
    }
    tools.appendChild(copyBtn(t("rel_copy_page"), "rel_copy_page", function () { return pageUrl(r.tag); }));
    var g = el("a", "btn small-btn ghost", t("rel_page")); g.href = r.url; tools.appendChild(g);
    li.appendChild(tools);

    var hashes = r.assets.filter(function (x) { return x.sha; });
    if (hashes.length) {
      var hd = el("details", "rel-notes hashes");
      hd.appendChild(el("summary", "", t("rel_sha")));
      hashes.forEach(function (x) {
        var hr = el("div", "hash-row");
        hr.appendChild(el("span", "hash-name", x.name));
        hr.appendChild(el("code", "hash", x.sha));
        hr.appendChild(copyBtn(t("rel_copy"), "rel_copy", x.sha, "mini"));
        hd.appendChild(hr);
      });
      li.appendChild(hd);
    }

    if (r.body) {
      var det = el("details", "rel-notes"), filled = false;
      det.appendChild(el("summary", "", t("rel_notes")));
      var fill = function () {
        if (filled) return;
        filled = true;
        var bar = el("div", "notes-bar");
        bar.appendChild(copyBtn(t("rel_copy_notes"), "rel_copy_notes", r.body.trim(), "mini"));
        det.appendChild(bar);
        det.appendChild(notes(r.body));
      };
      det._fill = fill;
      det.addEventListener("toggle", function () { if (det.open) fill(); });
      if (r.tag === hlTag) { det.open = true; fill(); }
      li.appendChild(det);
    }
    return li;
  }

  function visible() {
    var q = query.trim().toLowerCase();
    var list = all.filter(function (r) {
      if (filter === "stable" && r.pre) return false;
      if (filter === "dev" && !r.pre) return false;
      return !q || (r.tag + " " + (r.name || "")).toLowerCase().indexOf(q) >= 0;
    });
    list.sort(function (a, b) { return ((new Date(b.date) - new Date(a.date)) || 0) * (newestFirst ? 1 : -1); });
    return list;
  }

  function skeleton() {
    for (var i = 0; i < 3; i++) {
      var li = el("li", "rel skeleton"); li.setAttribute("aria-hidden", "true");
      li.appendChild(el("div", "sk sk-title"));
      li.appendChild(el("div", "sk sk-line"));
      var row = el("div", "row");
      row.appendChild(el("div", "sk sk-btn")); row.appendChild(el("div", "sk sk-btn"));
      li.appendChild(row);
      elList.appendChild(li);
    }
  }

  function stat(value, label) {
    var d = el("div", "stat");
    d.appendChild(el("strong", "", value));
    d.appendChild(el("span", "", label));
    return d;
  }
  function renderStats() {
    elStats.textContent = "";
    elStats.hidden = !loaded;
    if (!loaded) return;
    var total = 0, st = 0, dev = 0;
    all.forEach(function (r) {
      if (r.pre) dev++; else st++;
      r.assets.forEach(function (a) { total += a.dl || 0; });
    });
    elStats.appendChild(stat(num(total), t("rel_stat_total")));
    elStats.appendChild(stat(num(st), t("rel_stat_stable")));
    elStats.appendChild(stat(num(dev), t("rel_stat_dev")));
  }

  function renderAlert() {
    elAlert.textContent = "";
    if (!failed && !(loaded && cachedAt)) return;
    var msg = failed ? t("rel_error") : t("rel_cached") + App.rel(new Date(cachedAt).toISOString()) + ".";
    elAlert.appendChild(el("p", "", msg));
    var rb = el("button", "btn small-btn ghost", t("rel_retry")); rb.type = "button";
    rb.addEventListener("click", retry);
    elAlert.appendChild(rb);
  }

  function renderHero() {
    elHero.textContent = "";
    elHero.hidden = failed || (loaded && !latestRel);
    elHero.setAttribute("aria-busy", String(!loaded && !failed));
    if (elHero.hidden) return;
    elHero.appendChild(el("p", "eyebrow", t("hc_title")));
    if (!loaded) {
      elHero.appendChild(el("div", "sk sk-title"));
      elHero.appendChild(el("div", "sk sk-line"));
      elHero.appendChild(el("div", "sk sk-line"));
      return;
    }
    var r = latestRel, cia = asset(r, "IkersTube.cia");
    var top = el("div", "hc-top");
    top.appendChild(el("strong", "hc-ver", r.tag));
    var meta = el("span", "small");
    var tm = el("time", "", App.rel(r.date)); tm.setAttribute("datetime", r.date); tm.title = App.abs(r.date);
    meta.appendChild(tm);
    if (cia) meta.appendChild(document.createTextNode(" · .cia " + size(cia.size)));
    top.appendChild(meta);
    elHero.appendChild(top);
    var items = bullets(r.body, 3);
    if (items.length) {
      var ul = el("ul", "hc-list");
      items.forEach(function (x) { var li = el("li"), sp = el("span", "clamp"); inline(sp, x); li.appendChild(sp); ul.appendChild(li); });
      elHero.appendChild(ul);
    } else elHero.appendChild(el("p", "small", t("hc_nonotes")));
    var more = el("a", "hc-more", t("hc_notes")); more.href = "#" + r.tag;
    more.addEventListener("click", function (e) {
      e.preventDefault();
      if (location.hash === "#" + r.tag) goTo(r.tag); else location.hash = r.tag;
    });
    elHero.appendChild(more);
  }

  function render() {
    elFilter.textContent = "";
    [["all", "rel_all"], ["stable", "rel_stable"], ["dev", "rel_dev"]].forEach(function (f) {
      var b = el("button", "chip", t(f[1])); b.type = "button";
      b.setAttribute("aria-pressed", String(filter === f[0]));
      b.addEventListener("click", function () { filter = f[0]; shown = PAGE; render(); });
      elFilter.appendChild(b);
    });
    elSort.textContent = (newestFirst ? "↓ " : "↑ ") + t(newestFirst ? "rel_sort_new" : "rel_sort_old");
    elList.textContent = "";
    elList.setAttribute("aria-busy", String(!loaded && !failed));
    renderStats(); renderAlert(); renderHero();
    if (failed) { elStatus.textContent = ""; elMore.hidden = true; return; }
    if (!loaded) { elStatus.textContent = t("rel_loading"); elMore.hidden = true; skeleton(); return; }
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

  // ----- deep links: #v1.0.7 scrolls to the release, opens its notes and highlights it -----
  function goTo(tag) {
    var r = find(tag);
    if (!r) return false;
    hlTag = tag;
    if (visible().indexOf(r) < 0) { filter = "all"; query = ""; elSearch.value = ""; }
    var i = visible().indexOf(r);
    if (i >= shown) shown = i + 1;
    render();
    var node = document.getElementById(tag);
    if (node) {
      node.scrollIntoView({ block: "start", behavior: "instant" });
      node.setAttribute("tabindex", "-1");
      node.focus({ preventScroll: true });
    }
    return true;
  }
  function fromHash() {
    var h = location.hash.slice(1);
    if (!h || !loaded) return;
    try { h = decodeURIComponent(h); } catch (e) {}
    goTo(h);
  }
  window.addEventListener("hashchange", fromHash);

  // ----- QR dialog -----
  var dlg = document.getElementById("qr-dialog"), cv = document.getElementById("qr-canvas");
  var qrTitle = document.getElementById("qr-title"), qrUrl = document.getElementById("qr-url");
  var qrCopy = document.getElementById("qr-copy"), opener = null;
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
  function openQr(r, cia, dsx, from) {
    var a = cia || dsx;
    opener = from || document.activeElement;
    qrTitle.textContent = t("qr_for") + (r.name || r.tag) + " (" + (cia ? ".cia" : ".3dsx") + ")";
    qrUrl.textContent = a.url;
    qrCopy.textContent = t("qr_copy");
    drawQr(a.url);
    if (dlg.showModal) dlg.showModal(); else dlg.setAttribute("open", "");
  }
  function restoreFocus() {
    if (opener && opener.isConnected) opener.focus();
    opener = null;
  }
  dlg.addEventListener("close", restoreFocus);
  dlg.addEventListener("keydown", function (e) {
    if (e.key === "Escape" && !dlg.showModal) { dlg.removeAttribute("open"); restoreFocus(); }
  });
  qrCopy.addEventListener("click", function () { App.copy(qrUrl.textContent, qrCopy, "qr_copy"); });
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
      assets: (x.assets || []).map(function (a) {
        var m = /^sha256:([0-9a-f]{64})$/i.exec(a.digest || "");
        return { name: a.name, size: a.size, url: a.browser_download_url, dl: a.download_count || 0, sha: m ? m[1].toLowerCase() : "" };
      })
    };
  }
  function ready(list, at) {
    all = list.filter(function (r) { return !r.draft; });
    var stable = all.filter(function (r) { return !r.pre; }).sort(function (a, b) { return new Date(b.date) - new Date(a.date); });
    latestRel = stable.length ? stable[0] : null;
    latestTag = latestRel ? latestRel.tag : "";
    loaded = true; failed = false; cachedAt = at || 0;
    render(); setLatest(); fromHash();
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
  function readCache() {
    try {
      var c = JSON.parse(localStorage.getItem(CACHE_KEY) || "null");
      if (c && c.data && c.data.length) return c;
    } catch (e) {}
    return null;
  }
  function load(force) {
    var c = readCache();
    if (!force && c && Date.now() - c.at < CACHE_MS) { ready(c.data, 0); return; }
    fetchPage(1, []).then(function (list) {
      try { localStorage.setItem(CACHE_KEY, JSON.stringify({ at: Date.now(), data: list })); } catch (e) {}
      ready(list, 0);
    }).catch(function () {
      // Network or API failure: fall back to the last saved list, even if it has expired.
      if (c) ready(c.data, c.at);
      else { failed = true; loaded = false; render(); setLatest(); }
    });
  }
  function retry() {
    failed = false; loaded = false; cachedAt = 0;
    render(); setLatest();
    load(true);
  }
  window.addEventListener("online", function () { if (failed || cachedAt) retry(); });

  // ----- controls -----
  elSearch.addEventListener("input", function () { query = elSearch.value; shown = PAGE; render(); });
  elMore.addEventListener("click", function () { shown += PAGE; render(); });
  elSort.addEventListener("click", function () { newestFirst = !newestFirst; shown = PAGE; render(); });
  // "/" focuses the search, Esc clears it (both ignored while typing in some other field).
  document.addEventListener("keydown", function (e) {
    if (e.defaultPrevented || e.ctrlKey || e.metaKey || e.altKey || dlg.open) return;
    var tg = e.target, typing = tg && (tg.isContentEditable || /^(INPUT|TEXTAREA|SELECT)$/.test(tg.tagName));
    if (e.key === "/" && !typing) {
      e.preventDefault();
      elSearch.focus(); elSearch.select();
    } else if (e.key === "Escape" && (tg === elSearch || !typing)) {
      if (elSearch.value) { e.preventDefault(); elSearch.value = ""; query = ""; shown = PAGE; render(); }
      else if (tg === elSearch) elSearch.blur();
    }
  });

  // Printing: open the notes of the releases on screen so they end up on paper.
  var autoOpened = [];
  window.addEventListener("beforeprint", function () {
    elList.querySelectorAll("details.rel-notes:not(.hashes):not([open])").forEach(function (d) { d.open = true; if (d._fill) d._fill(); autoOpened.push(d); });
  });
  window.addEventListener("afterprint", function () { autoOpened.forEach(function (d) { d.open = false; }); autoOpened = []; });

  App.onLang(function () { render(); setLatest(); });
  render();
  load();
})();

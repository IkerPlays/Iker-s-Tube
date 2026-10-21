(function () {
  "use strict";
  var ST = { now: "st_now", next: "st_next", done: "st_done" };
  var items = window.ROADMAP || [];
  var board = document.getElementById("board"), filters = document.getElementById("filters");
  var current = "All", openItems = {};
  var el = App.el;
  function tx(o) { return o && (o[App.lang()] || o.en) || ""; }
  function reduced() { return window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches; }

  function renderRoadmap() {
    var areas = ["All"].concat(items.map(function (i) { return i.area; }).filter(function (a, i, arr) { return arr.indexOf(a) === i; }));
    filters.textContent = "";
    areas.forEach(function (a) {
      var b = el("button", "chip", a === "All" ? App.t("all") : a); b.type = "button";
      b.setAttribute("aria-pressed", String(a === current));
      b.addEventListener("click", function () { current = a; renderRoadmap(); });
      filters.appendChild(b);
    });

    var done = items.filter(function (i) { return i.status === "done"; }).length;
    var pct = items.length ? Math.round(done * 100 / items.length) : 0;
    var bar = document.getElementById("road-progress");
    bar.value = pct;
    bar.setAttribute("aria-valuetext", pct + "%");
    document.getElementById("road-progress-text").textContent =
      App.t("road_prog").replace("{n}", done).replace("{total}", items.length).replace("{p}", pct);

    board.textContent = "";
    ["now", "next", "done"].forEach(function (st) {
      var col = el("div", "col");
      col.appendChild(el("h3", "st " + st, App.t(ST[st])));
      var list = items.filter(function (i) { return i.status === st && (current === "All" || i.area === current); });
      if (!list.length) col.appendChild(el("p", "empty", App.t("empty")));
      list.forEach(function (i) {
        var idx = items.indexOf(i);
        var card = el("div", "card");
        card.appendChild(el("span", "tag", i.area));
        card.appendChild(el("strong", "", tx(i.title)));
        card.appendChild(el("p", "", tx(i.text)));
        if (i.detail) {
          var d = el("details", "road-more");
          d.appendChild(el("summary", "", App.t("road_more")));
          d.appendChild(el("p", "", tx(i.detail)));
          if (openItems[idx]) d.open = true;
          d.addEventListener("toggle", function () { openItems[idx] = d.open; });
          card.appendChild(d);
        }
        col.appendChild(card);
      });
      board.appendChild(col);
    });
  }

  // Theme (remembered when storage is available)
  var root = document.documentElement;
  try { var saved = localStorage.getItem("theme"); if (saved) root.setAttribute("data-theme", saved); } catch (e) {}
  document.getElementById("theme").addEventListener("click", function () {
    var dark = root.getAttribute("data-theme") === "dark" ||
      (!root.getAttribute("data-theme") && window.matchMedia("(prefers-color-scheme: dark)").matches);
    var next = dark ? "light" : "dark";
    root.setAttribute("data-theme", next);
    try { localStorage.setItem("theme", next); } catch (e) {}
  });

  document.getElementById("lang").addEventListener("click", function () { App.setLang(App.lang() === "es" ? "en" : "es"); });

  // Install guide tabs (all panels stay visible without JavaScript)
  var tabs = Array.prototype.slice.call(document.querySelectorAll('.tablist [role="tab"]'));
  function selectTab(i, focus) {
    tabs.forEach(function (tb, k) {
      var on = k === i;
      tb.setAttribute("aria-selected", String(on));
      tb.tabIndex = on ? 0 : -1;
      document.getElementById(tb.getAttribute("aria-controls")).hidden = !on;
    });
    if (focus) tabs[i].focus();
  }
  tabs.forEach(function (tb, i) {
    tb.addEventListener("click", function () { selectTab(i, false); });
    tb.addEventListener("keydown", function (e) {
      var n = tabs.length, k = -1;
      if (e.key === "ArrowRight") k = (i + 1) % n;
      else if (e.key === "ArrowLeft") k = (i + n - 1) % n;
      else if (e.key === "Home") k = 0;
      else if (e.key === "End") k = n - 1;
      if (k >= 0) { e.preventDefault(); selectTab(k, true); }
    });
  });
  if (tabs.length) selectTab(0, false);

  // Nav: highlight the section in view
  var navLinks = {}, inView = {};
  var ids = ["download", "features", "releases", "roadmap", "support", "faq"];
  document.querySelectorAll("header nav a").forEach(function (a) { navLinks[a.getAttribute("href").slice(1)] = a; });
  function markNav() {
    var active = "";
    for (var k = 0; k < ids.length; k++) if (inView[ids[k]]) { active = ids[k]; break; }
    Object.keys(navLinks).forEach(function (id) {
      if (id === active) navLinks[id].setAttribute("aria-current", "true"); else navLinks[id].removeAttribute("aria-current");
    });
  }
  if ("IntersectionObserver" in window) {
    var spy = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) { inView[e.target.id] = e.isIntersecting; });
      markNav();
    }, { rootMargin: "-45% 0px -50% 0px" });
    ids.forEach(function (id) { var s = document.getElementById(id); if (s) spy.observe(s); });

    // Reveal on scroll (the CSS only hides things when motion is allowed)
    root.classList.add("js");
    var rv = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) { if (e.isIntersecting) { e.target.classList.add("in"); rv.unobserve(e.target); } });
    }, { rootMargin: "0px 0px -8% 0px" });
    document.querySelectorAll("main section:not(.hero), .grid > article").forEach(function (n) { n.classList.add("reveal"); rv.observe(n); });
  }

  // Back to top
  var toTop = document.getElementById("to-top");
  function onScroll() { toTop.hidden = window.scrollY < 700; }
  window.addEventListener("scroll", onScroll, { passive: true });
  toTop.addEventListener("click", function () {
    window.scrollTo({ top: 0, behavior: reduced() ? "auto" : "smooth" });
    var brand = document.querySelector(".brand");
    if (brand) brand.focus({ preventScroll: true });
  });
  onScroll();

  // Offline banner
  var offline = document.getElementById("offline");
  function net() { offline.hidden = navigator.onLine !== false; }
  window.addEventListener("online", net);
  window.addEventListener("offline", net);
  net();

  // Printing: open the FAQ answers
  window.addEventListener("beforeprint", function () { document.querySelectorAll("#faq details").forEach(function (d) { d.open = true; }); });

  App.onLang(renderRoadmap);
  App.apply();
})();

(function () {
  "use strict";
  var ST = { now: "st_now", next: "st_next", done: "st_done" };
  var items = window.ROADMAP || [];
  var board = document.getElementById("board"), filters = document.getElementById("filters");
  var current = "All";

  function el(tag, cls, text) {
    var e = document.createElement(tag);
    if (cls) e.className = cls;
    if (text !== undefined) e.textContent = text;
    return e;
  }
  function tx(o) { return o && (o[App.lang()] || o.en) || ""; }

  function renderRoadmap() {
    var areas = ["All"].concat(items.map(function (i) { return i.area; }).filter(function (a, i, arr) { return arr.indexOf(a) === i; }));
    filters.textContent = "";
    areas.forEach(function (a) {
      var b = el("button", "chip", a === "All" ? App.t("all") : a); b.type = "button";
      b.setAttribute("aria-pressed", String(a === current));
      b.addEventListener("click", function () { current = a; renderRoadmap(); });
      filters.appendChild(b);
    });
    board.textContent = "";
    ["now", "next", "done"].forEach(function (st) {
      var col = el("div", "col");
      col.appendChild(el("h3", "st " + st, App.t(ST[st])));
      var list = items.filter(function (i) { return i.status === st && (current === "All" || i.area === current); });
      if (!list.length) col.appendChild(el("p", "empty", App.t("empty")));
      list.forEach(function (i) {
        var card = el("div", "card");
        card.appendChild(el("span", "tag", i.area));
        card.appendChild(el("strong", "", tx(i.title)));
        card.appendChild(el("p", "", tx(i.text)));
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
  App.onLang(renderRoadmap);
  App.apply();
})();

(function () {
  "use strict";
  var LABELS = { done: "Done", now: "In progress", next: "Planned" };

  // Roadmap board with filters
  var items = window.ROADMAP || [];
  var board = document.getElementById("board");
  var filters = document.getElementById("filters");
  var areas = ["All"].concat(items.map(function (i) { return i.area; }).filter(function (a, i, arr) { return arr.indexOf(a) === i; }));
  var current = "All";

  function el(tag, cls, text) {
    var e = document.createElement(tag);
    if (cls) e.className = cls;
    if (text) e.textContent = text;
    return e;
  }

  function render() {
    board.textContent = "";
    ["now", "next", "done"].forEach(function (st) {
      var col = el("div", "col");
      col.appendChild(el("h3", "st " + st, LABELS[st]));
      var list = items.filter(function (i) { return i.status === st && (current === "All" || i.area === current); });
      if (!list.length) col.appendChild(el("p", "empty", "Nothing here."));
      list.forEach(function (i) {
        var card = el("div", "card");
        card.appendChild(el("span", "tag", i.area));
        card.appendChild(el("strong", "", i.title));
        card.appendChild(el("p", "", i.text));
        col.appendChild(card);
      });
      board.appendChild(col);
    });
    Array.prototype.forEach.call(filters.children, function (b) {
      b.setAttribute("aria-pressed", String(b.textContent === current));
    });
  }

  areas.forEach(function (a) {
    var b = el("button", "chip", a);
    b.type = "button";
    b.addEventListener("click", function () { current = a; render(); });
    filters.appendChild(b);
  });
  render();

  // Latest stable release (GitHub API, optional: the buttons work without it)
  var info = document.getElementById("latest");
  fetch("https://api.github.com/repos/IkerPlays/Iker-s-Tube/releases/latest")
    .then(function (r) { return r.ok ? r.json() : Promise.reject(); })
    .then(function (d) {
      var when = d.published_at ? " (" + new Date(d.published_at).toLocaleDateString() + ")" : "";
      info.textContent = "Latest stable release: " + String(d.tag_name || "").replace(/^v/, "") + when;
    })
    .catch(function () { info.textContent = "See the Releases page for the latest version."; });

  // Theme toggle (remembered when storage is available)
  var root = document.documentElement;
  try { var saved = localStorage.getItem("theme"); if (saved) root.setAttribute("data-theme", saved); } catch (e) {}
  document.getElementById("theme").addEventListener("click", function () {
    var dark = root.getAttribute("data-theme") === "dark" ||
      (!root.getAttribute("data-theme") && window.matchMedia("(prefers-color-scheme: dark)").matches);
    var next = dark ? "light" : "dark";
    root.setAttribute("data-theme", next);
    try { localStorage.setItem("theme", next); } catch (e) {}
  });
})();

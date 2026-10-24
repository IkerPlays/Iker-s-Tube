// Applies the saved theme before the page is painted (no light flash for dark-theme users)
try { var t = localStorage.getItem("theme"); if (t) document.documentElement.setAttribute("data-theme", t); } catch (e) {}

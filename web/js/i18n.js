// Tiny i18n: data-i18n (text), data-i18n-html (trusted markup from this file only), data-i18n-ph (placeholder).
(function () {
  "use strict";
  var D = {
    en: {
      skip: "Skip to content", nav_download: "Download", nav_features: "Features", nav_releases: "Releases", nav_roadmap: "Roadmap", nav_faq: "FAQ",
      hero_eyebrow: "Homebrew for Nintendo 3DS / 2DS", hero_title: "YouTube on your Nintendo 3DS",
      hero_text: "Search, watch, download and use your account, all on the console.",
      dl_cia: "Download .cia", dl_3dsx: "Download .3dsx", all_versions: "All versions",
      latest_loading: "Latest stable release: loading...", latest: "Latest stable release: ", latest_fail: "See the Releases section for the latest version.",
      dl_title: "Download and install",
      step1: "Scan the QR with <strong>FBI</strong> (<em>Remote Install &gt; Scan QR code</em>), or copy <code>IkersTube.cia</code> to your SD card and install it.",
      step2: "<code>IkersTube.3dsx</code> also runs from the Homebrew Launcher.",
      step3: "For sound you need the DSP firmware dump at <code>sdmc:/3ds/dspfirm.cdc</code>.",
      step4: "The app checks for updates on start (Settings > Update).",
      data_note: "Your data lives in <code>sdmc:/3ds/IkersTube/</code>.",
      qr_caption: "Always points to the latest stable .cia",
      feat_title: "Features",
      f1_t: "Search and watch", f1_x: "Filters, suggestions, Shorts, live streams and chat, subtitles, SponsorBlock.",
      f2_t: "Your account", f2_x: "Sign in for subscriptions, likes, playlists, history and comments.",
      f3_t: "Offline", f3_x: "Save videos or audio, queue, favorites and watch later.",
      f4_t: "Playback tools", f4_x: "Skip intro, go to a time, loop a chapter, speed, sleep timer with fade.",
      f5_t: "Made for the 3DS", f5_x: "Data saver, rounded themes, QR codes for links, bookmarks, backups.",
      f6_t: "Safe by design", f6_x: "Safe mode after a crash, bounded downloads and a crash report you can share.",
      rel_title: "Releases", rel_intro: "Every version, loaded live from GitHub. Download it or scan the QR with FBI.",
      rel_search: "Search a version...", rel_all: "All", rel_stable: "Stable", rel_dev: "Devbuilds", rel_more: "Show more",
      rel_github: "Open the Releases page on GitHub", rel_loading: "Loading releases from GitHub...",
      rel_error: "Could not load the releases (GitHub may be limiting requests). Use the link below instead.",
      rel_none: "No release matches.", rel_latest: "Latest", rel_stable_badge: "Stable", rel_dev_badge: "Devbuild",
      rel_notes: "Notes", rel_hide: "Hide notes", rel_qr: "QR", rel_page: "GitHub", rel_downloads: "downloads", rel_count: "releases",
      rel_no_files: "No files attached",
      qr_hint: "In FBI: Remote Install > Scan QR code.", qr_save: "Save PNG", close: "Close", qr_for: "QR for ",
      road_title: "Roadmap", road_intro: "Plans can change. Items move between columns as work goes on.",
      st_now: "In progress", st_next: "Planned", st_done: "Done", all: "All", empty: "Nothing here.",
      faq_title: "Questions",
      q1: "What do I need?", a1: "A Nintendo 3DS / 2DS family console with custom firmware (homebrew), and FBI or the Homebrew Launcher.",
      q2: "Stable or devbuild?", a2: "Stable releases are installed by the in-app updater. Devbuilds are test builds that the app only offers; they may be unstable. Only the newest 15 devbuilds of each version are kept.",
      q3: "There is no sound", a3: "Dump the DSP firmware to <code>sdmc:/3ds/dspfirm.cdc</code> (for example with DSP1).",
      q4: "The app closes at start", a4: "It enters <strong>safe mode</strong> on the next launch and resets its settings. Please open an issue and attach <code>crash.txt</code> / <code>log.txt</code> from <code>sdmc:/3ds/IkersTube/</code>.",
      q5: "Do I need an account?", a5: "No. You can search and watch without one. Signing in adds your subscriptions, likes, playlists, history and comments.",
      q6: "Is it official?", a6: "No. It is a fan project, not affiliated with YouTube, Google or Nintendo.",
      foot1: "Iker's Tube is based on <a href=\"https://github.com/erievs/FourthTube\">FourthTube</a> / ThirdTube and is licensed under GPLv3. Not affiliated with YouTube, Google or Nintendo.",
      foot2: "Source code on GitHub", foot3: "Report a problem", lang_to: "ES"
    },
    es: {
      skip: "Saltar al contenido", nav_download: "Descargar", nav_features: "Funciones", nav_releases: "Versiones", nav_roadmap: "Roadmap", nav_faq: "Preguntas",
      hero_eyebrow: "Homebrew para Nintendo 3DS / 2DS", hero_title: "YouTube en tu Nintendo 3DS",
      hero_text: "Busca, mira, descarga y usa tu cuenta, todo desde la consola.",
      dl_cia: "Descargar .cia", dl_3dsx: "Descargar .3dsx", all_versions: "Todas las versiones",
      latest_loading: "Última versión estable: cargando...", latest: "Última versión estable: ", latest_fail: "Mira la sección de versiones para la última.",
      dl_title: "Descarga e instalación",
      step1: "Escanea el QR con <strong>FBI</strong> (<em>Remote Install &gt; Scan QR code</em>), o copia <code>IkersTube.cia</code> a la SD e instálalo.",
      step2: "<code>IkersTube.3dsx</code> también funciona desde el Homebrew Launcher.",
      step3: "Para el sonido hace falta el volcado del firmware DSP en <code>sdmc:/3ds/dspfirm.cdc</code>.",
      step4: "La app busca actualizaciones al arrancar (Ajustes > Actualizar).",
      data_note: "Tus datos están en <code>sdmc:/3ds/IkersTube/</code>.",
      qr_caption: "Siempre apunta al último .cia estable",
      feat_title: "Funciones",
      f1_t: "Buscar y ver", f1_x: "Filtros, sugerencias, Shorts, directos y chat, subtítulos, SponsorBlock.",
      f2_t: "Tu cuenta", f2_x: "Inicia sesión para suscripciones, me gusta, listas, historial y comentarios.",
      f3_t: "Sin conexión", f3_x: "Guarda vídeos o audio, cola, favoritos y ver más tarde.",
      f4_t: "Herramientas", f4_x: "Saltar intro, ir a un minuto, repetir un capítulo, velocidad, temporizador con fundido.",
      f5_t: "Hecha para la 3DS", f5_x: "Ahorro de datos, temas redondeados, QR de enlaces, marcadores, copias.",
      f6_t: "Segura", f6_x: "Modo seguro tras un cierre, descargas limitadas e informe de fallo para compartir.",
      rel_title: "Versiones", rel_intro: "Todas las versiones, cargadas en directo desde GitHub. Descárgala o escanea el QR con FBI.",
      rel_search: "Buscar una versión...", rel_all: "Todas", rel_stable: "Estables", rel_dev: "Devbuilds", rel_more: "Ver más",
      rel_github: "Abrir la página de Releases en GitHub", rel_loading: "Cargando versiones desde GitHub...",
      rel_error: "No se pudieron cargar las versiones (GitHub puede estar limitando peticiones). Usa el enlace de abajo.",
      rel_none: "Ninguna versión coincide.", rel_latest: "Última", rel_stable_badge: "Estable", rel_dev_badge: "Devbuild",
      rel_notes: "Notas", rel_hide: "Ocultar notas", rel_qr: "QR", rel_page: "GitHub", rel_downloads: "descargas", rel_count: "versiones",
      rel_no_files: "Sin archivos adjuntos",
      qr_hint: "En FBI: Remote Install > Scan QR code.", qr_save: "Guardar PNG", close: "Cerrar", qr_for: "QR de ",
      road_title: "Roadmap", road_intro: "Los planes pueden cambiar. Las tareas se mueven de columna según avanza el trabajo.",
      st_now: "En curso", st_next: "Planeado", st_done: "Hecho", all: "Todo", empty: "Nada por aquí.",
      faq_title: "Preguntas",
      q1: "¿Qué necesito?", a1: "Una consola de la familia Nintendo 3DS / 2DS con firmware personalizado (homebrew), y FBI o el Homebrew Launcher.",
      q2: "¿Estable o devbuild?", a2: "Las versiones estables las instala el actualizador de la app. Las devbuilds son pruebas que la app solo ofrece; pueden ser inestables. Solo se guardan las 15 devbuilds más recientes de cada versión.",
      q3: "No hay sonido", a3: "Vuelca el firmware DSP a <code>sdmc:/3ds/dspfirm.cdc</code> (por ejemplo con DSP1).",
      q4: "La app se cierra al arrancar", a4: "Entra en <strong>modo seguro</strong> en el siguiente arranque y reinicia sus ajustes. Abre una incidencia y adjunta <code>crash.txt</code> / <code>log.txt</code> de <code>sdmc:/3ds/IkersTube/</code>.",
      q5: "¿Necesito una cuenta?", a5: "No. Puedes buscar y ver sin ella. Al iniciar sesión se añaden tus suscripciones, me gusta, listas, historial y comentarios.",
      q6: "¿Es oficial?", a6: "No. Es un proyecto de fans, sin relación con YouTube, Google ni Nintendo.",
      foot1: "Iker's Tube está basada en <a href=\"https://github.com/erievs/FourthTube\">FourthTube</a> / ThirdTube y tiene licencia GPLv3. Sin relación con YouTube, Google ni Nintendo.",
      foot2: "Código en GitHub", foot3: "Informar de un problema", lang_to: "EN"
    }
  };

  var lang = "en";
  try { lang = localStorage.getItem("lang") || ""; } catch (e) {}
  if (lang !== "en" && lang !== "es") lang = (navigator.language || "en").toLowerCase().indexOf("es") === 0 ? "es" : "en";
  var listeners = [];

  function t(key) { return (D[lang] && D[lang][key]) || D.en[key] || key; }

  function apply() {
    document.documentElement.lang = lang;
    document.querySelectorAll("[data-i18n]").forEach(function (e) { e.textContent = t(e.getAttribute("data-i18n")); });
    document.querySelectorAll("[data-i18n-html]").forEach(function (e) { e.innerHTML = t(e.getAttribute("data-i18n-html")); });
    document.querySelectorAll("[data-i18n-ph]").forEach(function (e) { e.setAttribute("placeholder", t(e.getAttribute("data-i18n-ph"))); });
    var b = document.getElementById("lang");
    if (b) b.textContent = t("lang_to");
    listeners.forEach(function (fn) { fn(lang); });
  }

  window.App = {
    t: t,
    lang: function () { return lang; },
    onLang: function (fn) { listeners.push(fn); },
    setLang: function (l) {
      lang = l === "es" ? "es" : "en";
      try { localStorage.setItem("lang", lang); } catch (e) {}
      apply();
    },
    apply: apply
  };
})();

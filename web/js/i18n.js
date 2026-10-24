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
      step3: "For sound you need the DSP firmware dump at <code>sdmc:/3ds/dspfirm.cdc</code>.",
      step4: "The app checks for updates on start (Settings > Update).",
      data_note: "Your data lives in <code>sdmc:/3ds/IkersTube/</code>.",
      qr_caption: "Always points to the latest stable .cia",
      nav_ui: "Look", ui_title: "The new look", ui_intro: "A new design for the whole app: rounded cards, switches, a new bottom bar and light and dark themes. These are renders of the real interface code with sample data, not photos of a console.",
      ui_a1: "Home screen, dark theme", ui_a2: "Home screen, light theme", ui_a3: "Settings", ui_a4: "Video page", ui_a5: "Video menu", ui_a6: "Settings, light theme",
      ui_c1: "Home, dark", ui_c2: "Home, light", ui_c3: "Settings", ui_c4: "Video page", ui_c5: "Long press menu", ui_c6: "Settings, light",
      feat_title: "Features",
      f1_t: "Search and watch", f1_x: "Filters, suggestions, Shorts, live streams and chat, subtitles, SponsorBlock.",
      f2_t: "Your account", f2_x: "Sign in for subscriptions, likes, playlists, history and comments.",
      f3_t: "Offline", f3_x: "Save videos or audio, queue, favorites and watch later.",
      f4_t: "Playback tools", f4_x: "Skip intro, go to a time, loop a chapter, speed, sleep timer with fade.",
      f5_t: "Made for the 3DS", f5_x: "Data saver, rounded themes, QR codes for links, bookmarks, backups.",
      f6_t: "Safe by design", f6_x: "Safe mode after a crash, bounded downloads and a crash report you can share.",
      rel_title: "Releases", rel_intro: "Every version, loaded live from GitHub. Download it or scan the QR with FBI.",
      rel_all: "All", rel_stable: "Stable", rel_dev: "Devbuilds", rel_more: "Show more",
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
      q7: "The update does not show up", a7: "It checks GitHub at start. In Settings > Update you can check by hand. You can also scan the QR on this site with FBI (Remote Install > Scan QR code).",
      q8: "Which video qualities are there?", a8: "144p, 240p, 360p and 480p, plus an audio-only mode. The New 3DS family plays up to 480p; the old 3DS family only the low ones. Pick one in the Playback tab of the player.",
      q9: "Audio language tracks", a9: "Videos with dubbed audio list their tracks in the player tool \"Audio language\". If YouTube does not answer, tap it again.",
      q10: "Where are my data and backups?", a10: "Everything lives in sdmc:/3ds/IkersTube/. Settings > Data can export a backup to the SD card and restore it.",
      q11: "Can I use it offline?", a11: "Yes: saved videos and audio play from the SD card without internet.",
      faq_search: "Search the questions", faq_none: "No question matches.",
      inst_title: "Install in 3 steps", inst1: "Open <strong>FBI</strong> on your console and choose <em>Remote Install &gt; Scan QR code</em>.", inst2: "Scan the QR of the latest stable release (above, or in Releases).", inst3: "Open <strong>Iker's Tube</strong> from the Home menu. Later updates are offered inside the app.",
      foot4: "Releases feed", nav_label: "Main", rel_filter_label: "Filter releases", rel_search_label: "Search a version", road_filter_label: "Filter roadmap", qr_label: "QR code",
      q6: "Is it official?", a6: "No. It is a fan project, not affiliated with YouTube, Google or Nintendo.",
      foot1: "Iker's Tube is based on <a href=\"https://github.com/erievs/FourthTube\">FourthTube</a> / ThirdTube and is licensed under GPLv3. Not affiliated with YouTube, Google or Nintendo.",
      foot2: "Source code on GitHub", foot3: "Report a problem", lang_to: "ES",
      nav_support: "Support", lang_label: "Language", theme_label: "Theme", to_top: "Back to top",
      meta_title: "Iker's Tube - YouTube for Nintendo 3DS",
      meta_desc: "Iker's Tube is a YouTube client for the Nintendo 3DS / 2DS family. Download every version, scan a QR to install and follow the roadmap.",
      og_desc: "A homebrew YouTube client for the 3DS / 2DS family. Download, scan a QR and install.",
      offline_msg: "You are offline. Saved data is shown when available.",
      hc_title: "Latest stable release", hc_notes: "Full notes", hc_nonotes: "See the full notes for the details.", hc_loading: "Loading the latest release...",
      tabs_label: "Install method", tab_cia: "CIA with FBI", tab_3dsx: "3DSX with Homebrew Launcher", tab_qr: "By QR",
      cia1: "Copy <code>IkersTube.cia</code> to your SD card (for example into <code>sdmc:/cias/</code>).",
      cia2: "Open <strong>FBI</strong>, go to <em>SD &gt; cias</em>, pick the file and choose <em>Install CIA</em>.",
      cia3: "Launch Iker's Tube from the HOME Menu.",
      dsx1: "Copy <code>IkersTube.3dsx</code> to <code>sdmc:/3ds/</code> on your SD card.",
      dsx2: "Open the <strong>Homebrew Launcher</strong> and pick Iker's Tube from the list.",
      dsx3: "Nothing is installed: it runs straight from the Homebrew Launcher.",
      qr1: "On your 3DS open <strong>FBI</strong> and go to <em>Remote Install &gt; Scan QR code</em>.",
      qr2: "Point the console camera at the QR code on this page.",
      qr3: "Confirm the install and wait for it to finish.",
      qr_alt: "QR code for the latest stable IkersTube.cia", good_title: "Good to know",
      copied: "Copied!", copy_failed: "Could not copy", rel_copy_link: "Copy download link", rel_copy_page: "Copy page link",
      rel_copy_notes: "Copy notes", rel_sha: "Checksums (SHA-256)", rel_copy: "Copy",
      rel_sort_new: "Newest first", rel_sort_old: "Oldest first", rel_sort_label: "Sort order",
      rel_search_ph: "Search a version... (press /)",
      rel_stat_total: "Total downloads", rel_stat_stable: "Stable releases", rel_stat_dev: "Devbuilds",
      rel_retry: "Try again",
      rel_cached: "GitHub could not be reached (you may be offline), so this is a saved list from ",
      qr_copy: "Copy link",
      road_progress: "Roadmap progress", road_prog: "{n} of {total} done ({p}%)", road_more: "More detail",
      sup_title: "Support", sup_intro: "Something broke? Here is how to get it fixed quickly.",
      sup1_t: "Report a problem", sup1_x: "Open an issue on GitHub and tell us what you were doing, your console (3DS, 2DS, New 3DS...), its firmware and the app version (stable or devbuild).",
      sup2_t: "Safe mode", sup2_x: "After a crash the app starts in <strong>safe mode</strong> on the next launch and resets its settings so it can open again. That is expected. Report the crash before you carry on.",
      sup3_t: "Files to attach", sup3_x: "Attach <code>crash.txt</code> and <code>log.txt</code> from <code>sdmc:/3ds/IkersTube/</code> on the SD card. Read them first: issues on GitHub are public.",
      sup_new: "Open an issue", sup_all: "See open issues"
    },
    es: {
      skip: "Saltar al contenido", nav_download: "Descargar", nav_features: "Funciones", nav_releases: "Versiones", nav_roadmap: "Roadmap", nav_faq: "Preguntas",
      hero_eyebrow: "Homebrew para Nintendo 3DS / 2DS", hero_title: "YouTube en tu Nintendo 3DS",
      hero_text: "Busca, mira, descarga y usa tu cuenta, todo desde la consola.",
      dl_cia: "Descargar .cia", dl_3dsx: "Descargar .3dsx", all_versions: "Todas las versiones",
      latest_loading: "Última versión estable: cargando...", latest: "Última versión estable: ", latest_fail: "Mira la sección de versiones para la última.",
      dl_title: "Descarga e instalación",
      step3: "Para el sonido hace falta el volcado del firmware DSP en <code>sdmc:/3ds/dspfirm.cdc</code>.",
      step4: "La app busca actualizaciones al arrancar (Ajustes > Actualizar).",
      data_note: "Tus datos están en <code>sdmc:/3ds/IkersTube/</code>.",
      qr_caption: "Siempre apunta al último .cia estable",
      nav_ui: "Aspecto", ui_title: "El nuevo aspecto", ui_intro: "Un diseño nuevo para toda la app: tarjetas redondeadas, interruptores, una barra inferior nueva y temas claro y oscuro. Son imágenes generadas con el código real de la interfaz y datos de ejemplo, no fotos de una consola.",
      ui_a1: "Pantalla de inicio, tema oscuro", ui_a2: "Pantalla de inicio, tema claro", ui_a3: "Ajustes", ui_a4: "Página de un vídeo", ui_a5: "Menú de un vídeo", ui_a6: "Ajustes, tema claro",
      ui_c1: "Inicio, oscuro", ui_c2: "Inicio, claro", ui_c3: "Ajustes", ui_c4: "Página de vídeo", ui_c5: "Menú de pulsación larga", ui_c6: "Ajustes, claro",
      feat_title: "Funciones",
      f1_t: "Buscar y ver", f1_x: "Filtros, sugerencias, Shorts, directos y chat, subtítulos, SponsorBlock.",
      f2_t: "Tu cuenta", f2_x: "Inicia sesión para suscripciones, me gusta, listas, historial y comentarios.",
      f3_t: "Sin conexión", f3_x: "Guarda vídeos o audio, cola, favoritos y ver más tarde.",
      f4_t: "Herramientas", f4_x: "Saltar intro, ir a un minuto, repetir un capítulo, velocidad, temporizador con fundido.",
      f5_t: "Hecha para la 3DS", f5_x: "Ahorro de datos, temas redondeados, QR de enlaces, marcadores, copias.",
      f6_t: "Segura", f6_x: "Modo seguro tras un cierre, descargas limitadas e informe de fallo para compartir.",
      rel_title: "Versiones", rel_intro: "Todas las versiones, cargadas en directo desde GitHub. Descárgala o escanea el QR con FBI.",
      rel_all: "Todas", rel_stable: "Estables", rel_dev: "Devbuilds", rel_more: "Ver más",
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
      q7: "No aparece la actualización", a7: "La app consulta GitHub al arrancar. En Ajustes > Actualizar puedes comprobarlo a mano. También puedes escanear el QR de esta web con FBI (Remote Install > Scan QR code).",
      q8: "¿Qué calidades hay?", a8: "144p, 240p, 360p y 480p, además de un modo solo audio. La familia New 3DS llega a 480p; la antigua solo a las bajas. Elige en la pestaña Reproducción del reproductor.",
      q9: "Pistas de idioma del audio", a9: "Los vídeos con doblaje muestran sus pistas en la herramienta \"Idioma del audio\" del reproductor. Si YouTube no responde, tócala otra vez.",
      q10: "¿Dónde están mis datos y copias?", a10: "Todo está en sdmc:/3ds/IkersTube/. En Ajustes > Datos puedes exportar una copia a la SD y restaurarla.",
      q11: "¿Funciona sin internet?", a11: "Sí: los vídeos y el audio guardados se reproducen desde la SD sin internet.",
      faq_search: "Buscar en las preguntas", faq_none: "Ninguna pregunta coincide.",
      inst_title: "Instala en 3 pasos", inst1: "Abre <strong>FBI</strong> en tu consola y elige <em>Remote Install &gt; Scan QR code</em>.", inst2: "Escanea el QR de la última versión estable (arriba, o en Versiones).", inst3: "Abre <strong>Iker's Tube</strong> desde el menú Home. Las siguientes actualizaciones te las ofrece la propia app.",
      foot4: "Feed de versiones", nav_label: "Principal", rel_filter_label: "Filtrar versiones", rel_search_label: "Buscar una versión", road_filter_label: "Filtrar el roadmap", qr_label: "Código QR",
      q6: "¿Es oficial?", a6: "No. Es un proyecto de fans, sin relación con YouTube, Google ni Nintendo.",
      foot1: "Iker's Tube está basada en <a href=\"https://github.com/erievs/FourthTube\">FourthTube</a> / ThirdTube y tiene licencia GPLv3. Sin relación con YouTube, Google ni Nintendo.",
      foot2: "Código en GitHub", foot3: "Informar de un problema", lang_to: "EN",
      nav_support: "Ayuda", lang_label: "Idioma", theme_label: "Tema", to_top: "Volver arriba",
      meta_title: "Iker's Tube - YouTube para Nintendo 3DS",
      meta_desc: "Iker's Tube es un cliente de YouTube para la familia Nintendo 3DS / 2DS. Descarga cualquier versión, escanea un QR para instalar y sigue el roadmap.",
      og_desc: "Un cliente homebrew de YouTube para la familia 3DS / 2DS. Descarga, escanea un QR e instala.",
      offline_msg: "Estás sin conexión. Se muestran los datos guardados cuando los hay.",
      hc_title: "Última versión estable", hc_notes: "Notas completas", hc_nonotes: "Mira las notas completas para más detalles.", hc_loading: "Cargando la última versión...",
      tabs_label: "Forma de instalar", tab_cia: "CIA con FBI", tab_3dsx: "3DSX con Homebrew Launcher", tab_qr: "Con QR",
      cia1: "Copia <code>IkersTube.cia</code> a la SD (por ejemplo a <code>sdmc:/cias/</code>).",
      cia2: "Abre <strong>FBI</strong>, ve a <em>SD &gt; cias</em>, elige el archivo y pulsa <em>Install CIA</em>.",
      cia3: "Abre Iker's Tube desde el menú HOME.",
      dsx1: "Copia <code>IkersTube.3dsx</code> a <code>sdmc:/3ds/</code> en la SD.",
      dsx2: "Abre el <strong>Homebrew Launcher</strong> y elige Iker's Tube en la lista.",
      dsx3: "No se instala nada: se ejecuta directamente desde el Homebrew Launcher.",
      qr1: "En tu 3DS abre <strong>FBI</strong> y ve a <em>Remote Install &gt; Scan QR code</em>.",
      qr2: "Apunta la cámara de la consola al código QR de esta página.",
      qr3: "Confirma la instalación y espera a que termine.",
      qr_alt: "Código QR del último IkersTube.cia estable", good_title: "Conviene saber",
      copied: "¡Copiado!", copy_failed: "No se pudo copiar", rel_copy_link: "Copiar enlace de descarga", rel_copy_page: "Copiar enlace de la página",
      rel_copy_notes: "Copiar notas", rel_sha: "Sumas de control (SHA-256)", rel_copy: "Copiar",
      rel_sort_new: "Más nuevas primero", rel_sort_old: "Más antiguas primero", rel_sort_label: "Orden",
      rel_search_ph: "Buscar una versión... (pulsa /)",
      rel_stat_total: "Descargas totales", rel_stat_stable: "Versiones estables", rel_stat_dev: "Devbuilds",
      rel_retry: "Reintentar",
      rel_cached: "No se pudo contactar con GitHub (quizá estás sin conexión), así que esta es una lista guardada de ",
      qr_copy: "Copiar enlace",
      road_progress: "Progreso del roadmap", road_prog: "{n} de {total} hechas ({p}%)", road_more: "Más detalle",
      sup_title: "Ayuda", sup_intro: "¿Algo falla? Así se arregla más rápido.",
      sup1_t: "Informar de un problema", sup1_x: "Abre una incidencia en GitHub y cuenta qué hacías, tu consola (3DS, 2DS, New 3DS...), su firmware y la versión de la app (estable o devbuild).",
      sup2_t: "Modo seguro", sup2_x: "Tras un cierre inesperado la app arranca en <strong>modo seguro</strong> la siguiente vez y reinicia sus ajustes para poder abrirse. Es lo esperado. Informa del fallo antes de seguir.",
      sup3_t: "Archivos que adjuntar", sup3_x: "Adjunta <code>crash.txt</code> y <code>log.txt</code> de <code>sdmc:/3ds/IkersTube/</code> en la SD. Léelos antes: las incidencias de GitHub son públicas.",
      sup_new: "Abrir una incidencia", sup_all: "Ver incidencias abiertas"
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
    document.querySelectorAll("[data-i18n-aria]").forEach(function (e) { e.setAttribute("aria-label", t(e.getAttribute("data-i18n-aria"))); });
    document.querySelectorAll("[data-i18n-alt]").forEach(function (e) { e.setAttribute("alt", t(e.getAttribute("data-i18n-alt"))); });
    document.title = t("meta_title");
    [["meta[name=description]", "meta_desc"], ["meta[property='og:title']", "meta_title"], ["meta[property='og:description']", "og_desc"],
      ["meta[name='twitter:title']", "meta_title"], ["meta[name='twitter:description']", "og_desc"]].forEach(function (m) {
      var n = document.querySelector(m[0]);
      if (n) n.setAttribute("content", t(m[1]));
    });
    document.querySelectorAll("[data-i18n-alt]").forEach(function (e) { e.setAttribute("alt", t(e.getAttribute("data-i18n-alt"))); });
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

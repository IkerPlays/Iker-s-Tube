# Web

Static site (HTML, CSS, JS, no build step) for Netlify. English and Spanish.

- Deploy: connect the repository on Netlify, set **Base directory** to `web` and leave the build command empty
  (`netlify.toml` publishes the folder and sets security headers), or drag the `web` folder into Netlify Drop.
- Releases section: loaded live from the GitHub API in the browser (cached 10 minutes per tab), with download buttons and a
  QR for each version. The QR library is `js/vendor/qrcode.js` (MIT, qrcode-generator 1.4.4).
- Roadmap: edit `js/roadmap.js`. Texts of the page: `js/i18n.js`.
- Netlify's CSP only allows scripts and styles from the site itself and requests to `api.github.com`.

## Extras
- Releases: relative dates, sort, copy links, deep links (`#v1.0.7`), SHA-256 (from the API `digest` field, shown when present),
  stats strip, skeletons, retry and fallback to the last saved list (localStorage `ikt-releases-v2`) when GitHub cannot be reached.
- Roadmap items may have an optional `detail` (EN/ES) in `js/roadmap.js`; a progress bar counts the done items.
- `js/util.js`: shared helpers (element builder, clipboard copy, dates). `css/print.css`: print stylesheet.
- Icons and social image in `img/` (apple-touch-icon, icon-192/512, og-image.png 1200x630) and `manifest.webmanifest`. The
  `og:image` tags use root-relative paths; switch them to absolute URLs once the final domain is known.
- JSON-LD (`SoftwareApplication`) is an inline data block; it is not script execution and the strict CSP does not block it.

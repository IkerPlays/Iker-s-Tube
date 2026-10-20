# Web

Static site (HTML, CSS, JS, no build step) for Netlify. English and Spanish.

- Deploy: connect the repository on Netlify, set **Base directory** to `web` and leave the build command empty
  (`netlify.toml` publishes the folder and sets security headers), or drag the `web` folder into Netlify Drop.
- Releases section: loaded live from the GitHub API in the browser (cached 10 minutes per tab), with download buttons and a
  QR for each version. The QR library is `js/vendor/qrcode.js` (MIT, qrcode-generator 1.4.4).
- Roadmap: edit `js/roadmap.js`. Texts of the page: `js/i18n.js`.
- Netlify's CSP only allows scripts and styles from the site itself and requests to `api.github.com`.

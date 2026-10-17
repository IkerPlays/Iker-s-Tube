# Notes for Claude (repository workflow)

- All work goes straight to `main`. Every push to `main` builds a unique devbuild release (`v<version>-dev.N`).
- `RELEASE_NOTES.md` = notes of the CURRENT devbuild only (English, plain text, no emojis). Replace its content with what the push adds.
- `STABLE_RELEASE_NOTES.md` = everything new since the LAST stable release. After each change that adds or changes a feature,
  ADD it to the "What's new since the last stable release" list. When the owner publishes a stable release (Stable release
  workflow), the file is rewritten from scratch for the next stable: keep the intro line and the Installing section, and start
  the list empty. English, plain text, no emojis.
- Compile locally before pushing: `docker run --rm -v $PWD:/src -w /src devkitpro/devkitarm make -j8` (the .cia step needs makerom
  and is only done in CI; the .3dsx must link).
- New user-visible text: use `tr("English", "Spanish")` and add the other languages in `source/data_io/translations.cpp`.

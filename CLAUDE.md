# Notes for Claude (repository workflow)

- All work goes straight to `main`. Every push to `main` builds a unique devbuild release (`v<version>-dev.N`).
- `RELEASE_NOTES.md` = notes of the CURRENT devbuild only (English, plain text, no emojis). Replace its content with what the push adds.
- `STABLE_RELEASE_NOTES.md` = everything new since the LAST stable release. The base of the list is the commit that the latest stable
  tag (vX.Y.Z, not a -dev one) was built from: at the start of every session and before every push, check the latest stable release
  (GitHub releases, `target_commitish`). If the owner published a new one, rewrite the file at once so it only lists what was added
  after that commit (use `git log <commit>..HEAD`), titled "What's new since X.Y.Z". After each change that adds or changes a feature,
  ADD it to the "What's new since the last stable release" list. When the owner publishes a stable release (Stable release
  workflow), the file is rewritten from scratch for the next stable: keep the intro line and the Installing section, and list only
  what comes after that stable. English, plain text, no emojis.
- Compile locally before pushing: `docker run --rm -v $PWD:/src -w /src devkitpro/devkitarm make -j8` (the .cia step needs makerom
  and is only done in CI; the .3dsx must link).
- New user-visible text: use `tr("English", "Spanish")` and add the other languages in `source/data_io/translations.cpp`.

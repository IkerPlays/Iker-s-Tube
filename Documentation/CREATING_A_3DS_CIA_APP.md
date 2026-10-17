# How to create a Nintendo 3DS app (.3dsx and installable .cia)

This guide comes from building Iker's Tube (repo IkerPlays/Iker-s-Music-3DS). Everything here was done in a cloud Linux
container with no console attached, so it works without owning a PC toolchain: build with Docker or GitHub Actions, copy
the file to the SD card, test on the console.

## 0. What you produce

| File | What it is | How the user runs it |
|---|---|---|
| `.3dsx` | Homebrew executable (plus optional SMDH icon) | Copy to `sdmc:/3ds/`, open from the Homebrew Launcher |
| `.cia` | Installable title (appears on the HOME menu) | Install with FBI (or similar) from the SD card |

A `.cia` is built from the same `.elf` as the `.3dsx`, plus a banner (`.bnr`), an icon (`.icn`) and an RSF file with the
permissions. So: compile -> `.elf` -> `.3dsx` (`3dsxtool`) and `.cia` (`makerom`).

## 1. Toolchain

- **devkitPro / devkitARM + libctru** (the 3DS SDK). Use the official Docker image so nothing is installed locally:
  `docker run --rm -v $PWD:/src -w /src devkitpro/devkitarm make -j8`
  The image already has `make`, `arm-none-eabi-gcc`, `3dsxtool`, `smdhtool`, `picasso`, `tex3ds`, libctru, citro2d/citro3d.
  (`dkp-pacman -S 3ds-dev` is the same thing on a normal install; extra portlibs: `3ds-curl`, `3ds-mbedtls`, `3ds-zlib`...)
- **makerom** (builds the .cia): `git clone https://github.com/3DSGuy/Project_CTR.git`, then in `makerom`: `make deps && make`.
- **bannertool** (builds banner and SMDH icon): `git clone https://github.com/carstene1ns/3ds-bannertool.git`, `cmake -S . -B b && make -C b`.
  The `.cia` step needs these two tools; the `.3dsx` does not. Locally in Docker only the `.3dsx` is usually built, the `.cia` is made in CI.

## 2. Project layout (minimum)

```
Makefile              # based on the devkitPro 3ds template, plus the cia step
source/               # .c / .cpp (any depth; the Makefile finds them)
resource/app.rsf      # permissions and title info for makerom
resource/icon.png     # 48x48 PNG
resource/banner.png   # 256x128 PNG
resource/banner.wav   # short jingle, <= 3 s, 16-bit PCM
romfs/                # optional: files embedded in the app (read with "romfs:/")
```

Minimal `source/main.c` to check the toolchain:

```c
#include <3ds.h>
#include <stdio.h>
int main(void) {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    printf("Hello 3DS\nPress START to exit\n");
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();
    }
    gfxExit();
    return 0;
}
```

Start from the template: copy `/opt/devkitpro/examples/3ds/templates/application/` (inside the Docker image) and adapt it.

## 3. Makefile variables that matter

```make
TARGET      := MyApp                 # output name: MyApp.elf / .3dsx
APP_TITLE   := My App                # shown in the Homebrew Launcher and HOME menu
APP_DESCRIPTION := My App
APP_AUTHOR  := Me
PRODUCT_CODE := CTR-ABC              # any text, with FreeProductCode: true in the RSF
UNIQUE_ID   := 0xF1A02               # see "Unique ID" below
APP_VER     := 1                     # integer passed to makerom -ver (bumps the cia version)
ICON        := resource/icon.png
BANNER_IMAGE := resource/banner.png
BANNER_AUDIO := resource/banner.wav
RSF_PATH    := resource/app.rsf
ROMFS       := romfs
```

The extra step that creates the cia (after the normal .3dsx build):

```make
bannertool makebanner -i resource/banner.png -a resource/banner.wav -o build/banner.bnr
bannertool makesmdh -s "$(APP_TITLE)" -l "$(APP_DESCRIPTION)" -p "$(APP_AUTHOR)" -i resource/icon.png -o build/icon.icn
makerom -f cia -o MyApp.cia -target t -exefslogo \
        -elf MyApp.elf -rsf resource/app.rsf -banner build/banner.bnr -icon build/icon.icn \
        -DAPP_TITLE="$(APP_TITLE)" -DAPP_PRODUCT_CODE="$(PRODUCT_CODE)" -DAPP_UNIQUE_ID="$(UNIQUE_ID)" \
        -DAPP_ROMFS="$(ROMFS)" -ver $(APP_VER)
```

(`-target t` = retail-style test build, `-exefslogo` = homebrew logo accepted. In Iker's Tube the Makefile calls tools through
variables `MAKEROM` and `BANNERTOOL`, so CI can pass `MAKEROM=makerom BANNERTOOL=bannertool`.)

## 4. The RSF file (`resource/app.rsf`)

The Iker's Tube one is a good base (copy it from the repo). The important parts:

```yaml
BasicInfo:
  Title: $(APP_TITLE)
  ProductCode: $(APP_PRODUCT_CODE)
  Logo: Homebrew
RomFs:
  RootPath: $(APP_ROMFS)
TitleInfo:
  Category: Application
  UniqueId: $(APP_UNIQUE_ID)
Option:
  UseOnSD: true
  FreeProductCode: true
AccessControlInfo:
  FileSystemAccess: [ ... DirectSdmc, DirectSdmcWrite ... ]   # needed to read/write the SD card
  SystemMode: 64MB
  SystemModeExt: 124MB      # New 3DS extra memory
  CpuSpeed: 804MHz          # New 3DS 804 MHz
  EnableL2Cache: true
  ServiceAccessControl:     # every service the app uses must be listed (max 34)
    - APT:U
    - fs:USER
    - hid:USER
    - gsp::Gpu
    - soc:U       # sockets (network)
    - ssl:C       # https
    - http:C
    - ac:u        # Wi-Fi
    - ndm:u
    - dsp::DSP    # audio
    - cam:u       # camera
    - mic:u
SystemControlInfo:
  StackSize: 0x40000
  Dependency: { ... }       # the system modules (ac, am, hid, http, socket, ssl, ...) with their title IDs
```

If the app calls a service that is not in `ServiceAccessControl` the app crashes on the console only when run as a `.cia`
(the `.3dsx` inherits the Homebrew Launcher permissions, so it can hide the mistake). **Always test the .cia too.**

## 5. Unique ID and Title ID

- `UniqueId` is 20 bits. Homebrew apps usually choose a value in `0xF0000-0xFFFFF` to avoid clashing with real games.
- Title ID = `00040000` + UniqueId as `000XXXXX` + `00`. For `0xF1A02`: **`000400000F1A0200`**.
- Keep the same UniqueId forever: installing a new `.cia` with the same ID updates the existing app (settings and SD data stay).
  Changing it installs a second copy.
- Raise `APP_VER` on each release; FBI will not downgrade.

## 6. Icon, banner, sound

- Icon: 48x48 PNG. Banner: 256x128 PNG. Sound: WAV, up to 3 seconds. The repo has `tools/gen_logo.py` (Pillow) that draws both.
- If the banner step fails, check the image size and that the WAV is PCM 16-bit.

## 7. GitHub Actions: build the .cia on every push

Iker's Tube builds in a `devkitpro/devkitarm:latest` container:

```yaml
name: Build
on: [push, workflow_dispatch]
permissions: { contents: write }
jobs:
  build:
    runs-on: ubuntu-latest
    container: devkitpro/devkitarm:latest
    steps:
      - uses: actions/checkout@v4
      - run: apt-get update && apt-get install -y git make cmake g++ gcc
      - name: Build makerom and bannertool
        run: |
          git clone --depth 1 https://github.com/3DSGuy/Project_CTR.git /tmp/ctr
          make -C /tmp/ctr/makerom deps && make -C /tmp/ctr/makerom -j"$(nproc)"
          cp /tmp/ctr/makerom/bin/makerom /usr/local/bin/
          git clone --depth 1 https://github.com/carstene1ns/3ds-bannertool.git /tmp/bannertool
          cmake -S /tmp/bannertool -B /tmp/bannertool/b && make -C /tmp/bannertool/b -j"$(nproc)"
          cp /tmp/bannertool/b/bannertool /usr/local/bin/
      - run: make MAKEROM=makerom BANNERTOOL=bannertool
      - uses: actions/upload-artifact@v4
        with: { name: MyApp, path: "*.cia\n*.3dsx" }
```

To publish: a second job downloads the artifact and runs `gh release create v<version> MyApp.cia MyApp.3dsx --prerelease`
(devbuilds) or a normal release (stable). Iker's Tube names devbuilds `v<core>-dev.<run number>` so every push has a unique,
never-overwritten release, and the in-app updater compares versions.

## 8. Install and test on the console

1. Copy the `.cia` to the SD card, open **FBI** -> SD -> select it -> Install and delete. The app appears on HOME.
2. For quick tests copy the `.3dsx` to `sdmc:/3ds/` and run it from the Homebrew Launcher.
3. Console needs custom firmware (Luma3DS) for homebrew. Without it nothing here can run.

## 9. Debugging without a debugger

- **Log to the SD card**: write lines to `sdmc:/3ds/MyApp/log.txt` (flush often). Ask the user to send the file.
- **Crash screen**: Luma3DS shows an exception screen (data abort, undefined instruction, ...) with registers and the address
  of the crash (`pc`). Take a photo of it. Rebuild the exact same commit and resolve the address:
  `arm-none-eabi-addr2line -f -C -e MyApp.elf 0x00XXXXXX` (add `-funwind-tables`/`-g` to get usable output).
- **std::terminate / abort**: with `-fno-exceptions` (common on 3DS) any thrown exception aborts the app. Install a
  `std::set_terminate` handler that writes a `crash.txt` before exiting.
- Unmapped-memory crashes right after a service call often mean a freed or never-initialised object (use-after-free).

## 10. Rules and pitfalls learned the hard way

- **Memory is tiny**: ~64 MB on the old 3DS, ~124 MB on the New 3DS with `SystemModeExt`. Stream big files in chunks, never load
  them whole. Check every `malloc`/`new`: failing allocation under `-fno-exceptions` kills the app.
- **Threads**: `threadCreate(func, arg, stack, priority, core, detached)`. The old 3DS has cores 0 and 1 usable by apps; the
  New 3DS also core 2 (needs `CanAccessCore2`). Check the return value (it can fail). Use `LightLock` for short locks and
  never lock the same non-recursive lock twice in one thread (deadlock).
- **Network**: libcurl + mbedtls. TLS certificate checks are usually disabled (`CURLOPT_SSL_VERIFYPEER 0`) because the CA store is
  missing. Init `socInit` with a 0x100000-byte aligned buffer, and retry DNS failures right after Wi-Fi connects.
- **Sleep mode**: closing the lid pauses the app unless you call `aptSetSleepAllowed(false)` (needed for background audio/uploads).
- **Do not call `aptMainLoop()` stuff from other threads**; draw from the main thread only.
- **Test the .cia, not only the .3dsx** (permissions differ, see section 4).
- **Binary HTTP bodies**: set `CURLOPT_POSTFIELDSIZE`, otherwise curl uses `strlen` and cuts at the first zero byte.
- Everything user-visible should be translatable (a `tr("English", "Spanish")` helper plus a table for other languages).

## 11. Useful references

- libctru docs: https://libctru.devkitpro.org/
- devkitPro 3DS examples: `/opt/devkitpro/examples/3ds` (graphics, audio, network, camera, ...)
- citro2d (2D drawing), citro3d (3D), ndsp (audio), httpc / curl (network), `romfs:/` for embedded files.
- Makerom options: `makerom -h`. Title ID details: https://www.3dbrew.org/wiki/Title_list

## 12. Quick checklist for a new app

1. Copy the template and make `Hello 3DS` build with Docker (`.3dsx`).
2. Add icon, banner, sound and `app.rsf`; list the services you use.
3. Pick a UniqueId in `0xF0000-0xFFFFF` and keep it.
4. Add the CI workflow so each push yields `.cia` + `.3dsx`.
5. Install the `.cia` with FBI, test; add file logging and a crash handler early.
6. Bump `APP_VER` and the version string each release; publish a GitHub release.

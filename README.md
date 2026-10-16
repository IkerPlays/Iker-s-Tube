# Iker's Music 3DS

Reproductor de música homebrew para Nintendo 3DS / 2DS (New 2DS XL incluida) con playlists.

## Características
- Formatos: **MP3, OGG Vorbis, WAV (16-bit)**
- Biblioteca automática desde `sdmc:/music` (subcarpetas incluidas)
- Playlists persistentes (`sdmc:/3ds/IkersMusic/playlists/*.m3u`), crear/borrar/añadir/quitar
- Aleatorio, repetir (no / todo / una), volumen, siguiente/anterior, pausa
- Control por botones y pantalla táctil

## Controles
| Botón | Acción |
|---|---|
| ↑ ↓ / Círculo | Mover selección |
| A (o tocar 2 veces) | Reproducir / abrir playlist / añadir a playlist |
| B | Volver |
| X | Pausa / reanudar |
| Y | Canciones: añadir a playlist · Playlists: crear nueva · Dentro de playlist: quitar canción |
| SELECT | (en lista de playlists) borrar playlist |
| L / R | Anterior / siguiente |
| ← → | Volumen |
| ZL / ZR | Pestaña Canciones / Playlists |
| START | Salir |

## Requisitos en la consola
1. Pon tu música en `sdmc:/music/`.
2. Necesitas el firmware del DSP en `sdmc:/3ds/dspfirm.cdc` (se obtiene con la app **DSP1** de homebrew; sin él no hay audio).

## Cómo obtener el .cia (sin instalar nada: GitHub Actions)
1. Sube el repo a GitHub (ya está).
2. Ve a la pestaña **Actions** → workflow **Build 3DSX + CIA** (se ejecuta solo en cada push; o pulsa *Run workflow*).
3. Cuando termine, abre la ejecución y descarga el artefacto **IkersMusic3DS** (zip con `.cia` y `.3dsx`).
4. Pasa el `.cia` a la SD (p. ej. carpeta `cias/`), abre **FBI** en la consola → *SD* → selecciona el `.cia` → *Install*.
   (El `.3dsx` sirve para probar sin instalar: cópialo a `sdmc:/3ds/` y ábrelo con Homebrew Launcher.)

## Compilar en local (opcional)
Instala [devkitPro](https://devkitpro.org/wiki/Getting_Started) y:
```
sudo dkp-pacman -S 3ds-dev 3ds-mpg123 3ds-libvorbisidec 3ds-libogg
python3 tools/gen_assets.py meta
make
```
Para el .cia local necesitas además `makerom` y `bannertool` (mira los pasos del workflow en `.github/workflows/build.yml`).

# Iker's Tube

Cliente de YouTube homebrew para Nintendo 3DS / 2DS (probado como objetivo: New 2DS XL con CFW).
Basado en [FourthTube](https://github.com/erievs/FourthTube) (que viene de ThirdTube), bajo licencia **GPLv3**.

## Novedades respecto a FourthTube
- **Respaldo automático de clientes de reproducción**: si el cliente elegido (Android VR / visionOS / Android)
  no devuelve un vídeo reproducible, la app prueba los otros antes de dar error.
- **Tema oscuro por defecto** y color de acento nuevo.
- **Caché de miniaturas duplicada** (600): menos descargas al volver atrás en las listas.
- Datos separados en `sdmc:/3ds/IkersTube/` (no pisa los de FourthTube) y app con su propio ID.

## Instalar
1. En la pestaña **Actions** de este repo, abre la última ejecución verde y descarga el artefacto **IkersTube**.
2. Descomprime. `IkersTube_dev.cia` se instala con **FBI** (SD → el `.cia` → Install).
   `IkersTube.3dsx` sirve para probar desde el Homebrew Launcher (`sdmc:/3ds/`).

## Aviso
YouTube cambia su API a menudo y puede bloquear clientes. Si un vídeo no carga, prueba otro cliente en
Ajustes. Esta app no ha sido probada por el autor en todas las situaciones; reporta fallos en Issues.

## Compilar
Necesitas devkitPro + devkitARM, `3ds-zlib` y `3ds-mbedtls`. Ejecuta `make`. Los pasos exactos están en
`.github/workflows/build.yml`.

## Créditos
Todo el crédito de la base va para los autores originales: WindowsServer2003 (ThirdTube), Smu1zel, NCP 3.0 (erievs),
5GBurrito, 2b-zipper, ItsFrocat, Dragontwo14, cooolgamer, Dxni y la comunidad de FourthTube;
decodificador de [Video player for 3DS](https://github.com/Core-2-Extreme/Video_player_for_3DS) de Core-2-Extreme.
Dislikes: [Return YouTube Dislike](https://returnyoutubedislike.com).

# Iker's Tube

Cliente de YouTube homebrew para Nintendo 3DS / 2DS (probado como objetivo: New 2DS XL con CFW).
Basado en [FourthTube](https://github.com/erievs/FourthTube) (que viene de ThirdTube), bajo licencia **GPLv3**.

## Novedades respecto a FourthTube
- **Respaldo automático de clientes de reproducción**: si el cliente elegido (Android VR / visionOS / Android)
  no devuelve un vídeo reproducible, la app prueba los otros antes de dar error.
- **Tema oscuro por defecto** y color de acento nuevo.
- **Caché de miniaturas duplicada** (600): menos descargas al volver atrás en las listas.
- Datos separados en `sdmc:/3ds/IkersTube/` (no pisa los de FourthTube) y app con su propio ID.

## Funciones propias
Resumen; la guía completa está en [`Documentation/GUIA_DE_USO.md`](Documentation/GUIA_DE_USO.md).
- **Cuenta:** Me gusta, listas, suscripciones, campana, comentarios, historial sincronizado y listas de YouTube en la Biblioteca.
- **Reproductor:** chat en directo, idioma del audio, herramientas de salto y velocidad, enlaces con QR, SponsorBlock.
- **Offline:** guardar audio o vídeo, cola de descargas, pausa, noche, calidad y limpieza.
- **Studio:** estadísticas, subidas con lista y publicación programada, edición de subidas
  (necesita un cliente de Google Cloud propio: [`Documentation/STUDIO_SETUP.md`](Documentation/STUDIO_SETUP.md)).
- **Comodidad:** menú al mantener pulsado un vídeo o canal, ocultar canales, filtros de búsqueda, incógnito y copias.

## Instalar y actualizar
- Cada build sube el **devbuild** a [Releases](../../releases) con `IkersTube.cia` y `IkersTube.3dsx`.
  Instala el `.cia` con **FBI** (SD → el `.cia` → Install); el `.3dsx` sirve para probar desde el Homebrew Launcher.
- **Autoactualización:** la app busca nuevas versiones al abrirse. Si hay una, aparece un **punto rojo en ☰** y la
  entrada *Actualización disponible*, que lleva a Ajustes → **Actualizar**.
- Si publicas tú un release normal (con `.cia` y `.3dsx`, p. ej. tag `v1.1`), la app lo ofrece cuando su versión es
  mayor que la instalada. La versión de los builds es `VERSION` + número de build (p. ej. `1.0.14`).

## Cómo sacar un release grande (estable)
1. Edita `STABLE_RELEASE_NOTES.md` (inglés, texto plano): son las notas del release.
2. En GitHub: **Actions → Stable release → Run workflow**, desde la rama `main`, y escribe la versión (por ejemplo `1.0.0`).
   Marca *draft* si quieres revisarlo antes de publicarlo.
3. El workflow compila, crea el release `v1.0.0` (no pre-release) con `IkersTube.cia` e `IkersTube.3dsx`, y la app
   lo instala **automáticamente** al abrirse. Falla si esa versión ya existe.

Las devbuilds (`build.yml`) siguen saliendo solas en cada push a `main`, como pre-release, y la app solo las ofrece.
Tras publicar un estable, las devbuilds siguientes pasan solas a la versión siguiente (`1.0.1-dev.N`).

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

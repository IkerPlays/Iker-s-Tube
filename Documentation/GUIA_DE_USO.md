# Guía de uso de Iker's Tube

Esta guía resume las funciones de la app y dónde está cada una. Las que dependen de tu cuenta de YouTube
necesitan haber iniciado sesión en Ajustes; las de Studio, además, el login de Studio (ver `STUDIO_SETUP.md`).

## Pantallas principales (barra de abajo)
- **Inicio:** vídeos recomendados, "Continuar viendo", canales suscritos, vídeos nuevos (directos y estrenos primero)
  y la pestaña Studio (se puede ocultar en Ajustes).
- **Buscar:** búsqueda con filtros (el botón Filtro abre menús que se combinan: ordenar, fecha de subida, tipo, duración y características como HD, subtítulos CC, Creative Commons, en directo, 4K, HDR o 360 grados; mantenerlo pulsado lo quita). El teclado ofrece las sugerencias de YouTube mientras escribes,
  atajos de Explorar, búsquedas recientes y, arriba de los resultados, **Reproducir todo** y **Poner todo en cola**.
- **Biblioteca:** Cola, Favoritos, Listas, Luego (Ver más tarde), Offline y **Me gusta**. La pestaña Me gusta tiene un
  botón que recorre tus Me gusta, el historial de YouTube y cada lista de tu cuenta. Con una de tus listas a la vista salen los botones para renombrarla o borrarla (tocar dos veces).
- **Descargas:** lo guardado en la SD, con progreso, cola de descargas y botones para borrar.
- **Más:** ajustes, historial, incógnito, acerca de y salir.

- **Canales fijados:** mantén pulsado un canal suscrito (lista de canales locales) y elige "Fijar arriba"; sale primero, con una estrella.

## Mantener pulsado
- **Un vídeo (en cualquier lista):** menú con reproducir a continuación, cola, Ver más tarde, Favorito, guardar audio o
  vídeo sin conexión, "Añadir a mi lista de YouTube", "Buscar parecidos", "Ocultar este canal en las listas", marcar
  como visto y "No me interesa". En el historial añade "Quitar del historial".
- **Un canal (búsqueda y listas de suscritos):** suscribirte o quitar la suscripción (también en tu cuenta), la campana
  y ocultar o mostrar sus vídeos en las listas (Ajustes, Datos, enseña cuántos hay y los muestra todos de nuevo).

- **Comunidad de un canal:** mantén pulsada una publicación para darle Me gusta o votar en su encuesta (experimental, necesita tu login).

## Reproductor
- **Shorts:** en Buscar, Explorar, "Shorts" abre directamente el primero y sigue con más. En cualquier vídeo, la cruceta **Abajo** pasa al siguiente de la cola y **Arriba** vuelve al anterior, como deslizar los Shorts.
Herramientas (se desplazan hacia los lados): saltos de 10 s, 30 s y 1 min, A-B, capítulos, velocidad, Luego, marcas,
mini reproductor, parar al acabar, guardar audio/vídeo, Me gusta, No me gusta, Mi lista, campana, No me interesa,
ocultar canal, traducir, **Chat en directo**, **Idioma del audio** (pistas dobladas), **QR del canal**, **QR del vídeo**, **CC** (subtítulos de YouTube: cada pulsación pasa al siguiente idioma y al final los apaga), **<< Cap** (capítulo anterior), **Guardar audio de la
lista**, **Siguiente**, **Reiniciar**, **Silenciar**, **Temporizador** y **Enlaces** de la descripción (cada uno da su QR).

- **Chat en directo:** solo en directos. Botón para cambiar entre chat destacado y todo el chat. Escribir necesita el
  login de Studio. Un ajuste lo abre solo en los directos.
- **Colaboraciones:** toca el autor de un vídeo con varios canales; los ves de tres en tres, con "Suscribirme a todos".
- **Comentarios:** escribir, responder, dar Me gusta y borrar los tuyos. Si algo falla, una pantalla "Por qué falló"
  enseña el motivo para poder mandarlo.

- Tras los resultados salen **Búsquedas relacionadas** (el autocompletado de YouTube).

## Más herramientas del reproductor
- **Subtítulos:** CC (idiomas y traducción), CC tamaño, CC color.
- **Listas:** +Lista (añade el vídeo a una lista local) y Mezclar cola.
- **Velocidad:** Más lento / Más rápido (de 5 en 5 %). **Auto:** cambia entre no, lista y todo.
- **QR en este minuto**, **#Tags** (busca los hashtags del vídeo), **Guardar lista en YouTube**, **Recargar**, **Pantallas off** (SELECT + START las vuelve a encender) y **Estado** (hora y batería).
- La cruceta izquierda/derecha salta 5, 10, 15 o 30 s (Ajustes). Abajo/Arriba: siguiente/anterior vídeo.
- Ajustes nuevos: tiempo restante en la barra, parar tras N vídeos seguidos, recordatorio de descanso y subtítulos automáticos.

## Otras novedades
- **Historial:** "Canales más vistos" y, manteniendo pulsado el título de un día, "Borrar este día".
- **Biblioteca:** recuerda la última pestaña; Favoritos y Luego tienen "Vaciar" (pregunta antes); en Me gusta, "Quitar mi Me gusta".
- **Comentarios:** botón Recargar para ver el que acabas de mandar; frases rápidas en el teclado (edítalas en `comment_phrases.txt`).
- **Studio:** atajo a los vídeos de la cámara de la 3DS (DCIM) y plantillas `upload_description.txt` y `upload_tags.txt`.
- **Menús largos:** las opciones van en páginas ("Más opciones...", "Opciones anteriores...").
- En Buscar sale un **consejo del día**.

## Novedades de la 1.0.7
- **Buscar:** saludo según la hora, consejo del día, **Radio** (mezclas por estado de ánimo que siguen sonando solas: Tranquila, Concentración, Ejercicio, Dormir, Fiesta, Clásica, Música de juegos y Jazz) y "Sorpréndeme".
- **Historial:** "Mi resumen" (vídeos, tiempo, canal top, hora favorita, días seguidos y lo reproducido hoy).
- **Reproductor:** "Siguiente cap." (busca el título con el número de capítulo + 1), "Ahorro" (ahorro de datos), marcadores con QR del momento y el temporizador baja el volumen en los últimos 30 s.
- **Ajustes:** límite diario de reproducción, botones redondeados (se pueden quitar).
- **Modo seguro:** si la app se cierra al arrancar, el siguiente inicio renombra los ajustes a `*.crashed` y arranca sin extras. Si la app no abre: borra o renombra `3ds/IkersTube/settings.txt`.

## Después de la 1.0.7
- **Feed al instante:** la pestaña Feed del Inicio enseña el último feed guardado mientras carga el nuevo (y lo conserva si no hay conexión).
- **Historial:** "Mi resumen" (con este mes, mejor racha, vídeo más repetido y **13 logros**) y un recuerdo de hace una semana o un mes.
- **Reproductor:** herramienta "No me gusta" (votos estimados con returnyoutubedislikeapi.com, solo se envía el id del vídeo),
  "Estado" dice a qué hora termina el vídeo, Info cuenta cuántas veces lo has visto y el modo solo audio tiene un ecualizador animado.
- **Más herramientas:** "Saltar intro" (cada vídeo de un canal empieza 5 a 60 s más tarde, se recuerda por canal), "Controles" (qué hace cada botón)
  y la barra de progreso marca los capítulos y, en verde, lo que salta SponsorBlock.
- **Herramientas del reproductor:** las filas largas ahora se reparten en varias filas (antes los botones que no cabían en la pantalla no se podían tocar) y las menos usadas
  están plegadas tras "Más herramientas". Nuevas: "Ir al minuto" (escribes m:ss) y "Repetir capítulo".
- **Pantalla de solo audio:** ecualizador animado y, con los subtítulos (CC) activados, la letra de la canción.
- **Feed:** los vídeos que no te habían avisado llevan la etiqueta NEW. Al desbloquear un logro sale un aviso, y "Mi resumen" puede enseñar tus números en un QR.
- **Sin conexión:** si abres la app sin Wi-Fi te recuerda dónde están tus vídeos guardados. Si es muy de noche, te sugiere el temporizador de apagado.
- **Canal:** en la descripción verás cuántos vídeos de ese canal has visto.
- **Idioma:** el español y el portugués ya no se pierden al reiniciar.
- **Acerca de:** QR para descargar la última versión estable.

## Descargas y almacenamiento
- Guardar audio o vídeo desde el reproductor o desde cualquier lista; las listas tienen "Guardar audio" en bloque.
- Ajustes: calidad de los vídeos guardados, descargar solo de noche (1:00 a 7:00), borrar tras ver y vaciar la caché de imágenes.
- Descargas: pausar y reanudar la descarga en curso, borrar todas o solo las ya vistas.

## Studio
- Estadísticas, subir vídeos o Shorts (título, descripción, visibilidad, categoría, etiquetas, miniatura, **lista** y
  **publicación programada** con la hora de internet), continuar subidas interrumpidas y crear listas.
- Editar mis subidas: visibilidad, título, **descripción**, miniatura, borrar y estadísticas (con visitas por vídeo).

## Ajustes útiles
- Reproducción: subtítulos automáticos (se abren solos con el último idioma usado), velocidad por defecto, que la reproducción automática salte lo ya visto, quitar de Ver más tarde al
  acabar, chat automático y calidad de lo guardado.
- Datos: exportar listas (también suscripciones en CSV y cada lista local), importar suscripciones desde `import_subscriptions.csv`, olvidar posiciones, estadísticas de uso, informe para enviar, copia de seguridad, vaciar caché y **restablecer ajustes**.
- Incógnito: no se guarda historial, búsquedas ni posiciones.

## Si algo falla
Ajustes, Datos, "Crear un informe para enviar" escribe `sdmc:/3ds/IkersTube/report.txt` (versión, estado de la cuenta,
último error de la cuenta y el final del registro). Mándalo junto con el texto exacto del aviso.

## Otros detalles
- Un enlace de YouTube con tiempo (`&t=95s`, `?t=1m30s`, `&start=95`), por ejemplo escaneado desde un QR, empieza el
  vídeo en ese momento.
- Historial: buscar por título o canal, "Reproducir todo", "Aleatorio" y borrar lo de más de 30 días.
- La lista de canales suscritos locales sale ordenada alfabéticamente.

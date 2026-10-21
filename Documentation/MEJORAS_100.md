# Ronda de 100 mejoras

Estado honesto: todo compila y se ha subido, pero **nada de la app se ha probado en una consola real** (solo compilación y revisión de código).
La web se probó en un navegador con datos simulados de GitHub.

## Web (25)
1. Tarjeta de la última versión estable en la portada (versión, fecha, tamaño, notas).
2. Botón de copiar enlace por versión.
3. Enlace directo por versión (`#v1.0.7`) que abre sus notas y la resalta.
4. SHA-256 de cada archivo (si GitHub lo da) con botón de copiar.
5. Fechas relativas ("hace 3 días").
6. Orden más reciente / más antiguo.
7. Atajo `/` para buscar y `Esc` para limpiar.
8. El menú resalta la sección visible.
9. Botón de volver arriba.
10. Esqueletos de carga en las versiones.
11. Botón de reintentar y lista guardada si GitHub falla.
12. Contadores de descargas y de versiones.
13. Datos estructurados (JSON-LD).
14. Hoja de estilos para imprimir.
15. Guía de instalación en pestañas (FBI, 3DSX, QR).
16. Botón de copiar las notas de una versión.
17. Roadmap con barra de progreso y detalles desplegables.
18. Estilos para alto contraste y colores forzados.
19. Título y descripción según el idioma.
20. Aviso cuando no hay conexión.
21. Sección de soporte (qué archivos adjuntar).
22. Iconos (apple-touch, PWA, manifest).
23. Imagen para compartir (Open Graph / Twitter).
24. Animaciones suaves al desplazarse (respetan "reducir movimiento").
25. Diálogo del QR accesible (foco, Esc, copiar enlace).

## Ajustes y datos (15)
26. Importar copia pide segundo toque y muestra la fecha de la copia.
27. Línea "Última copia" en Ajustes.
28. La copia incluye el último vídeo reproducido y las novedades vistas.
29. La copia guarda `backup_info.txt` (versión, fecha, archivos).
30. Segundo toque al vaciar la caché de imágenes y al olvidar posiciones.
31. Restablecer ajustes conserva el archivo anterior como `settings.txt.reset`.
32. El informe incluye más diagnósticos (tema, calidad, build, listas...).
33. El informe incluye el registro anterior, `crash.txt` y los ajustes.
34. El aviso de almacenamiento muestra el espacio libre de la SD.
35. Botón para limpiar archivos temporales sobrantes.
36. Botón para borrar el historial de búsquedas desde Ajustes.
37. Botón para reiniciar las estadísticas de uso.
38. Importar distingue "no hay carpeta" de "la copia está vacía".
39. Acerca de muestra la consola, el idioma y la carpeta de datos.
40. Botón para recuperar los ajustes apartados por el modo seguro.

## Biblioteca, historial, búsqueda, inicio y canal (22)
41. L y R cambian de pestaña en la Biblioteca.
42. La Biblioteca reabre en Descargas si fue la última.
43. Se recuerdan los órdenes de Favoritos/Más tarde y de Descargas.
44. Contador en Favoritos, Más tarde y Me gusta (con el filtro).
45. Contador en la cola.
46. Confirmación al vaciar la cola.
47. Confirmación (con número) al borrar las descargas ya vistas.
48. "Quitar los ya vistos" dice cuántos quitó (cola, favoritos, más tarde, listas).
49. Menú de Descargas: añadir a la cola y más de este canal.
50. Lista abierta: guardar audio, quitar vistos y contador.
51. "Más..." con el menú completo en las pulsaciones largas de Favoritos, Me gusta, Más tarde y listas.
52. Historial: textos de "vacío" y "sin resultados".
53. Historial: Reproducir todo / Aleatorio respetan el filtro.
54. Historial: confirmación antes de borrar lo antiguo.
55. Búsqueda: texto de "sin resultados".
56. Búsqueda: pista en el teclado y espacios recortados.
57. Búsqueda: confirmación al borrar el historial y aviso de la pulsación larga.
58. Inicio: tocar el error reintenta.
59. Inicio: contador de canales y texto cuando no hay ninguno.
60. Canal: texto cuando la pestaña Vídeos está vacía.
61. Canal: anular la suscripción pide dos toques y avisa.
62. Guardar una descarga que ya existe lo dice en vez de repetirla.

## Reproductor (19)
63. Aviso al saltar con la cruceta o ZL/ZR (cuánto y dónde).
64. Ajuste del salto de ZL/ZR (5, 10, 30, 60 s).
65. Aviso "Continuando desde..." al reanudar.
66. Aviso al reintentar la conexión del vídeo.
67. Temporizador de sueño: aviso y guarda la posición.
68. Pausar guarda la posición.
69. Pausa automática con la batería casi vacía.
70. La barra muestra lo ya descargado por delante.
71. La barra muestra el bucle A-B.
72. La barra muestra tus marcadores.
73. El nombre del capítulo en la vista previa al arrastrar la barra.
74. Tocar el tiempo cambia entre duración y tiempo restante.
75. Nuevas acciones para L/R/X/Y: -10 s, silenciar, capítulo anterior y siguiente.
76. Ajuste de volumen por defecto.
77. Ajuste de volumen máximo.
78. Ajuste "Retroceder tras una pausa".
79. Herramienta "Ir al directo".
80. Aviso cuando el vídeo termina y no sigue nada.
81. La herramienta Estado añade el tiempo de hoy y los datos usados.

## Interfaz y robustez (15)
82. Diálogo de error: botones con su zona táctil real, nombres de archivo sin colisiones y descripción en dos líneas.
83. Errores de red en lenguaje claro.
84. Los avisos largos duran más.
85. Un GET cortado se reintenta una vez; los errores nunca salen vacíos; se arregló un fallo que ocultaba errores de sesión.
86. Las miniaturas que fallan esperan cada vez más para reintentar.
87. El marcador de carga de miniaturas deja de pulsar si la descarga falló.
88. Las filas de vídeo no recalculan el id y la duración en cada fotograma.
89. La barra inferior exige pulsar y soltar en el mismo sitio (y se corrigió `touch_y_prev`).
90. Los selectores aceptan toques en los huecos entre botones.
91. La barra inferior respeta el alto contraste.
92. Menú de listas de la cuenta: nombres recortados por ancho y aviso si no cargan.
93. Canales fijados, recientes y ocultos se guardan de forma atómica.
94. El registro no repite la misma línea en la SD.
95. La inercia del desplazamiento recorre lo mismo hacia arriba y hacia abajo.
96. Medir un texto vacío ya no es un comportamiento indefinido.
97. El cargador de miniaturas despierta menos cuando no hay trabajo.
98. El informe incluye el tamaño de las listas.
99. Guía de uso actualizada.
100. Roadmap de la web actualizado.

## Propuestas revisadas y descartadas (por riesgo o por no poder probarlas sin consola)
Reiniciar solo una pestaña de ajustes, tope de favoritos, caché de anchos de letra, reducir miniaturas antes de subirlas, nuevo orden "por canal" en el historial,
mini indicador en la pantalla superior, avance acelerado con la cruceta, cambiar la curva de la inercia, límite de la caché de miniaturas por bytes.

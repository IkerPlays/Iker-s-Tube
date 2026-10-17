# Cómo activar la subida y edición en Studio

Las estadísticas del Studio (suscriptores, visitas, vídeos) funcionan sin nada más. Para **subir vídeos, editar tus
subidas, miniaturas y borrar**, YouTube exige la API de datos (YouTube Data API v3), y el inicio de sesión normal de la
app (el de YouTube TV) no puede usarla: da el error 403. Por eso Studio tiene su **propio inicio de sesión**, con un
cliente OAuth tuyo de Google Cloud. Se hace una sola vez (unos 10 minutos, gratis).

## 1. Crear el proyecto y activar la API
1. Entra en https://console.cloud.google.com con tu cuenta de Google y crea un proyecto nuevo (por ejemplo "Ikers Tube Studio").
2. Menú "APIs y servicios" -> "Biblioteca" -> busca **YouTube Data API v3** -> **Habilitar**.

## 2. Pantalla de consentimiento
1. "APIs y servicios" -> "Pantalla de consentimiento de OAuth" (o "Google Auth Platform").
2. Tipo de usuario: **Externo**. Nombre de la app: lo que quieras. Tu correo como contacto.
3. En "Usuarios de prueba" añade **tu propia cuenta de Google** (la del canal).
4. En "Permisos/Scopes" añade `.../auth/youtube` (gestionar tu cuenta de YouTube).
5. Déjala en modo "Pruebas". Al iniciar sesión Google avisará de que la app no está verificada: pulsa "Avanzado" -> "Continuar".
   (En modo pruebas la sesión caduca a los 7 días: basta con volver a pulsar "Iniciar sesión en Studio".)

## 3. Crear el cliente OAuth
1. "APIs y servicios" -> "Credenciales" -> "Crear credenciales" -> "ID de cliente de OAuth".
2. Tipo de aplicación: **Televisores y dispositivos de entrada limitada**.
3. Copia el **ID de cliente** y el **Secreto de cliente**.

## 4. Poner las claves en la 3DS
Crea en la tarjeta SD el archivo `3ds/IkersTube/studio_client.txt` con **dos líneas**:

```
ID_DE_CLIENTE.apps.googleusercontent.com
SECRETO_DE_CLIENTE
```

## 5. Iniciar sesión en Studio
1. En la app, pestaña **Studio** -> **Iniciar sesión en Studio**.
2. La pantalla muestra una dirección (google.com/device) y un código. Ábrela en el móvil u ordenador, escribe el código y acepta.
3. Cuando salga "Studio conectado", ya puedes subir, editar y borrar.

## Notas
- Una subida gasta unas 1600 unidades de la cuota diaria (10.000 gratis), o sea unas 6 subidas al día.
- Las miniaturas personalizadas exigen que el canal esté verificado por teléfono en YouTube.
- Si algo falla, el motivo sale en pantalla y en `3ds/IkersTube/log.txt`.

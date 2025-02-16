# Sistema de Control de Aforo  
*Universidad Técnica Federico Santa María – Práctica Industrial*

## Descripción General

Este proyecto tiene como objetivo registrar, almacenar y subir los registros de ingresos y egresos en salas de clase utilizando dos módulos que se comunican entre sí:

- **Arduino R4 WiFi:**  
  - Se encarga de detectar y contar los ingresos y egresos mediante dos sensores ultrasónicos (definidos como _sensor INTERIOR_ y _sensor EXTERIOR_).  
  - Registra cada evento en un archivo de log fijo ("REG.TXT") en una tarjeta SD.  
  - Actualiza su reloj (RTC DS1307) mediante mensajes recibidos por UART del ESP32.

- **ESP32 (Tarjeta de Desarrollo):**  
  - Se conecta a la red eduroam utilizando WPA2 Enterprise (EAP PEAP).  
  - Obtiene la hora actual desde una API (por ejemplo, worldtimeapi.org) y la envía al Arduino para sincronizar su RTC.  
  - Recibe el archivo "REG.TXT" desde el Arduino a través de UART, lo renombra utilizando identificadores (campus, edificio, sala) y la hora actual, y lo sube a un servidor de almacenamiento en red mediante HTTP POST.
  - Verifica la conectividad tanto a la red WiFi como al servidor (almacenamiento en red) antes de proceder con la sincronización y la transferencia de archivos.

## Flujo de Trabajo

### Arduino (arduino_aforo.cpp)
1. **Inicialización y Verificación:**
   - Se configuran los identificadores (campus, edificio, número de sala).
   - Se definen los pines de dos sensores ultrasónicos:
     - **TRIG_PIN_INTERIOR / ECHO_PIN_INTERIOR**
     - **TRIG_PIN_EXTERIOR / ECHO_PIN_EXTERIOR**
   - Se inicializan el módulo RTC DS1307, la tarjeta SD y la comunicación UART (Serial1) con el ESP32.
   - Se realiza un handshake enviando "PING" y esperando "ESP32_OK" para asegurar la comunicación.

2. **Registro de Eventos:**
   - Se utiliza una máquina de estados (IDLE, WAIT_FOR_INTERIOR, WAIT_FOR_EXTERIOR) para determinar correctamente el flujo de personas:
     - **Ingreso:** Se detecta primero la activación del sensor EXTERIOR y luego la del sensor INTERIOR.
     - **Egreso:** Se detecta primero la activación del sensor INTERIOR y luego la del sensor EXTERIOR.
   - Cada evento se registra en "REG.TXT" en el siguiente formato:  
     `Campus;Edificio;Numero;Contador-Ingresos;Contador-Egresos;Ingreso-Egreso;Fecha;Hora`

3. **Sincronización y Transferencia:**
   - El Arduino recibe por UART mensajes de hora en el formato `TIME;YYYY-MM-DD;HH:MM:SS` enviados por el ESP32 para actualizar su RTC.
   - Dentro de una ventana de 5 segundos (por ejemplo, entre 22:00:00 y 22:00:04) se inicia el envío del archivo:
     - Se transmite el contenido del archivo línea a línea, finalizando con "END_OF_FILE".
     - El Arduino espera la confirmación "ACK_FILE" para eliminar el archivo y reiniciar el registro.

4. **Manejo de Errores:**
   - Si falla la inicialización del RTC, la tarjeta SD o la comunicación UART, el sistema se detiene y muestra un mensaje de error.
   - Se utilizan timeouts para evitar bloqueos durante la transferencia de datos.

### ESP32 (esp32.c)
1. **Conexión a eduroam y Configuración de Red:**
   - Se conecta a la red eduroam utilizando WPA2 Enterprise (configurando identidad, usuario y contraseña).
   - Se inicializa la pila TCP/IP en modo STA y se registran manejadores de eventos para gestionar reintentos y la obtención de IP.
   - Un LED indica el estado de la conexión (encendido al obtener IP, apagado en desconexión).

2. **Sincronización de Hora:**
   - La tarea `time_sync_task` espera a obtener conexión WiFi (con reintentos) y verifica la conectividad al servidor de almacenamiento durante al menos 10 segundos.
   - Se realiza una solicitud HTTP GET a la API de tiempo para obtener la hora actual, la cual se formatea en `TIME;YYYY-MM-DD;HH:MM:SS` y se envía al Arduino vía UART.
   - Se manejan errores en caso de fallos en la solicitud, asignación de memoria o parseo del JSON.

3. **Recepción y Subida del Archivo:**
   - Al recibir el comando "UPLOAD_NOW" por UART, se inicia la tarea `file_upload_task` (con una pila aumentada a 16 KB).
   - Se establece un protocolo de transferencia:
     - El ESP32 envía "READY_TO_SEND" y espera la respuesta "READY_TO_RECEIVE" del Arduino.
     - Recibe el archivo línea a línea hasta detectar "END_OF_FILE".
     - Genera un nuevo nombre para el archivo usando los identificadores y la hora actual (por ejemplo, `SanJoaquin_A_001_YYYYMMDD_HHMMSS.TXT`).
     - Realiza un HTTP POST para subir el archivo al servidor de almacenamiento. Si la transferencia es exitosa (HTTP 200), envía "ACK_FILE" al Arduino para que éste elimine el archivo.
   
4. **Manejo de Errores:**
   - Se implementan reintentos y timeouts para la conexión WiFi y para la comunicación con el servidor.
   - Se verifica la correcta asignación de memoria y el parseo del JSON.
   - Si ocurre algún error en la transferencia o en la conexión, se registran los detalles mediante logs (ESP_LOG).

## Compilación y Herramientas

- **Arduino (arduino_aforo.cpp):**  
  El código para Arduino se compila y sube mediante el IDE de Arduino. Asegúrate de contar con las librerías necesarias (RTClib, NewPing, SD, etc.).

- **ESP32 (esp32.c):**  
  Este código debe compilarse y flashearse utilizando el framework **ESP-IDF**.  
  Para compilar y flashear, se recomienda utilizar los comandos:  

´´´cpp
idf.py build idf.py flash

Asegúrate de configurar correctamente el entorno ESP-IDF y los parámetros de conexión.

## Requisitos del Sistema

- **Hardware:**
- Arduino R4 WiFi
- ESP32 (Tarjeta de Desarrollo)
- Sensores Ultrasónicos HC-SR04
- Módulo SD para almacenamiento
- Conexión a la red eduroam
- Servidor de almacenamiento (p. ej., configurado en un servidor web)

- **Software:**
- IDE de Arduino (para el código de Arduino)
- ESP-IDF (para compilar y flashear el código del ESP32)
- Librerías: RTClib, NewPing, SD, SPI, ArduinoJson (según corresponda)

## Conclusiones

Este sistema integra dos módulos que se comunican de forma robusta a través de UART, permitiendo registrar de forma precisa el flujo de personas en salas de clase y subir los registros a un servidor de almacenamiento en red. Se implementan mecanismos de verificación y manejo de errores en cada etapa del proceso (inicialización, sincronización de hora, transferencia de datos y subida a red), asegurando una operación confiable incluso en entornos con condiciones variables de red.

---

*Documentación para uso industrial. Para mayor información, consulte los archivos de código y la documentación de ESP-IDF y del IDE de Arduino.*

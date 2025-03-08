# Contador de Aforo con ESP32 y Arduino
*Universidad Técnica Federico Santa María – Práctica Industrial*

## Descripción del Proyecto

Este proyecto implementa un sistema de conteo de aforo utilizando un **ESP32** y un **Arduino**. El sistema detecta la presencia de personas a través de sensores ultrasónicos, almacena registros en una tarjeta **SD** y transmite los datos a un **servidor en la nube**.

- **ESP32**: Se encarga de la conectividad WiFi, sincronización horaria mediante **NTP**, y transmisión de datos al servidor.
- **Arduino**: Realiza la detección de personas mediante sensores ultrasónicos y almacena los registros en una tarjeta **SD**.

## Diagrama UML

El siguiente diagrama UML describe el flujo de ejecución del sistema:

![Diagrama UML](uml.pdf)

## Componentes y Librerías Utilizadas

### En el ESP32:
- **WiFi**: Manejo de conexión inalámbrica.
- **NTPClient**: Sincronización de hora con servidores NTP.
- **ArduinoJson**: Procesamiento de datos en formato JSON.
- **FS y SPIFFS**: Manejo de almacenamiento interno.
- **ESPAsyncWebServer**: Servidor web para interacción con el sistema.
- **FreeRTOS**: Gestión de tareas concurrentes.

### En el Arduino:
- **NewPing**: Manejo de sensores ultrasónicos.
- **RTCZero**: Control del reloj en tiempo real.
- **SD y SPI**: Manejo de almacenamiento en tarjeta SD.
- **SoftwareSerial**: Comunicación serie con el ESP32.

## Análisis del Flujo del Sistema

### 1. ESP32 - Inicialización

1. `nvs_flash_init()`: Inicializa el almacenamiento No Volátil (**NVS**).
2. `ESP_ERROR_CHECK()`: Verifica errores en la inicialización.
3. `uart_init()`: Configura la comunicación **UART** con el Arduino.
4. `led_init()`: Inicializa el **LED indicador**.
5. `wifi_init_sta()`: Establece la conexión **WiFi**.

### 2. ESP32 - Creación de Tareas

- Se crean tareas en **FreeRTOS** para manejar procesos concurrentes.
- `xTaskCreate(time_sync_task)`: Sincroniza la hora con un servidor **NTP**.

### 3. ESP32 - Bucle Principal

- `loop()`: Monitorea eventos y espera comandos.
- Si recibe `UPLOAD_NOW`, se inicia `file_upload_task()` para la **transferencia de archivos**.

### 4. ESP32 - Gestión de WiFi

1. `wifi_init_sta()`: Inicializa WiFi.
2. `event_handler(WiFi)`: Gestiona eventos de conexión.
3. Si WiFi está conectado:
   - Se enciende el **LED**.
   - `wifi_connected = true`.
4. Si se pierde la conexión, se reintenta.

### 5. ESP32 - Sincronización de Hora

1. `time_sync_task()`: Sincroniza con servidores NTP.
2. Si hay conexión, obtiene la hora y la envía al **Arduino**.
3. Si falla, genera un **error** y reintenta.

### 6. ESP32 - Recepción de Archivo desde Arduino

1. `file_upload_task()`: Recibe datos desde el Arduino.
2. Envía `READY_TO_SEND` y espera `READY_TO_RECEIVE`.
3. Si la respuesta es positiva:
   - Recibe el archivo línea por línea hasta `END_OF_FILE`.
   - **Sube el archivo** al servidor.
4. Si la subida es **exitosa**, envía `ACK_FILE`.

### 7. ESP32 - Subida a Servidor

1. `check_storage_connectivity()`: Verifica conexión con el servidor.
2. Si responde `HTTP 200`, la subida se realiza.
3. Si hay error, se reporta y se **reintenta**.

### 8. Arduino - Inicialización

1. `setup()`: Configuración inicial.
2. Verifica **RTC, SD y comunicación con ESP32**.
3. Si todo está correcto:
   - `systemReady = true`.
4. Muestra mensaje de **bienvenida**.

### 9. Arduino - Bucle Principal

1. `loop()`: Se ejecuta continuamente.
2. Recibe la **hora desde el ESP32**.
3. Si es hora de enviar logs:
   - Llama a `enviarArchivoESP32()`.

### 10. Arduino - Máquina de Estados - Detección

1. Lee sensores `sonarInterior.ping_cm()` y `sonarExterior.ping_cm()`.
2. Si está en estado **IDLE**, determina cuál sensor se activa primero:
   - **EXTERIOR activado**: entra en `WAIT_FOR_INTERIOR`.
   - **INTERIOR activado**: entra en `WAIT_FOR_EXTERIOR`.
3. Si ambos sensores se activan en orden correcto:
   - **Registra entrada/salida**.

### 11. Arduino - Interacción con ESP32

1. Envía `READY_TO_SEND` y espera `READY_TO_RECEIVE`.
2. Envía **datos línea por línea**.
3. Si recibe `ACK_FILE`, **borra el archivo** de la SD.

## Instalación y Configuración

### Requisitos
- **ESP32 DevKit v1**
- **Arduino Uno**
- **Sensores ultrasónicos HC-SR04**
- **Módulo RTC (opcional)**
- **Tarjeta microSD y lector SD**
- **Conexión WiFi activa**

### Pasos para la Instalación
1. **Clonar el repositorio**:
   ```bash
   git clone https://github.com/tu_usuario/contador-aforo.git
   ```
2. **Compilar y cargar código en Arduino**:
   - Abrir `arduino_aforo.cpp` en el IDE de Arduino.
   - Instalar librerías necesarias.
   - Compilar y subir código al Arduino.
3. **Compilar y cargar código en ESP32**:
   - Abrir `esp32.c` en PlatformIO o Arduino IDE.
   - Configurar credenciales WiFi.
   - Subir código al ESP32.

## Conclusión

Este sistema permite la detección de aforo con sensores ultrasónicos en un **Arduino**, que almacena registros en una **SD** y los transmite al **ESP32** para su subida a un servidor en la nube. 

El **ESP32** maneja la conexión **WiFi**, la sincronización de hora y la transmisión de datos. 

Se han definido detalladamente todas las etapas del proceso reflejadas en el diagrama UML, asegurando que cada componente cumple con su función en la arquitectura del sistema.

---
**Autor:** Eduardo Sebastián Palma Olave
**Repositorio:** [GitHub](https://github.com/adreoud/utfsm-control-de-aforo)

## Conclusiones

Este sistema integra dos módulos que se comunican de forma robusta a través de UART, permitiendo registrar de forma precisa el flujo de personas en salas de clase y subir los registros a un servidor de almacenamiento en red. Se implementan mecanismos de verificación y manejo de errores en cada etapa del proceso (inicialización, sincronización de hora, transferencia de datos y subida a red), asegurando una operación confiable incluso en entornos con condiciones variables de red.

---

*Documentación para uso industrial. Para mayor información, consulte los archivos de código y la documentación de ESP-IDF y del IDE de Arduino.*

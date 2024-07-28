#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <SPI.h>
#include <NewPing.h>

// Identificación del Arduino
const char* campus = "SanJoaquin";
const char* edificio = "A";
const char* numeroSala = "001";
const char* entradaA = "EntradaA";
const char* entradaB = "EntradaB";

// Definición de los pines para los sensores
#define TRIG_PIN_DERECHO 6
#define ECHO_PIN_DERECHO 5
#define TRIG_PIN_IZQUIERDO 8
#define ECHO_PIN_IZQUIERDO 7

// Definición de los pines para los sensores adicionales
#define TRIG_PIN_DERECHO_B 10
#define ECHO_PIN_DERECHO_B 9
#define TRIG_PIN_IZQUIERDO_B 12
#define ECHO_PIN_IZQUIERDO_B 11

// Definición de la distancia umbral y tolerancia
#define DISTANCIA_UMBRAL_DERECHO 50  // Distancia umbral en cm para el sensor derecho
#define DISTANCIA_UMBRAL_IZQUIERDO 50 // Distancia umbral en cm para el sensor izquierdo
#define TOLERANCIA 5  // Tolerancia en cm

// Tiempo máximo en milisegundos para considerar un ingreso o egreso válido
#define TIEMPO_RESPUESTA_MAXIMO 1000

NewPing sonarDerecho(TRIG_PIN_DERECHO, ECHO_PIN_DERECHO, 400); // Máxima distancia de medición en cm
NewPing sonarIzquierdo(TRIG_PIN_IZQUIERDO, ECHO_PIN_IZQUIERDO, 400);

// Declaración de sensores adicionales (Entrada B)
/*
NewPing sonarDerechoB(TRIG_PIN_DERECHO_B, ECHO_PIN_DERECHO_B, 400);
NewPing sonarIzquierdoB(TRIG_PIN_IZQUIERDO_B, ECHO_PIN_IZQUIERDO_B, 400);
*/

unsigned long tiempoDerecho = 0;
unsigned long tiempoIzquierdo = 0;
bool pausado = false;

// Variables adicionales para los segundos sensores (Entrada B)
/*
unsigned long tiempoDerechoB = 0;
unsigned long tiempoIzquierdoB = 0;
bool pausadoB = false;
*/

const char* ssid     = "NOMBRE-RED";
const char* password = "CONTRASEÑA-RED";
const char* host = "worldtimeapi.org";
const int httpPort = 80;
const char* url = "/api/timezone/America/Santiago";
const int chipSelect = 10;

RTC_DS1307 rtc;
bool rtcInitialized = false;
bool errorDisplayedA = false;
bool sdErrorDisplayed = false;
// bool errorDisplayedB = false;

int contadorIngresosA = 0;
int contadorEgresosA = 0;
int contadorIngresosEgresosA = 0;

/*
int contadorIngresosB = 0;
int contadorEgresosB = 0;
int contadorIngresosEgresosB = 0;
*/

void setup() {
  Serial.begin(9600);
  while (!Serial); // Espera a que se abra el puerto serie (necesario solo para algunos boards como el Leonardo)

  // Inicializar la tarjeta SD
  if (!SD.begin(chipSelect)) {
    Serial.println("Error al inicializar la tarjeta SD");
    return;
  }

  // Conexión WiFi
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println();
    Serial.println("Error: No se pudo conectar a la red WiFi");
  } else {
    Serial.println();
    Serial.println("WiFi conectado");
    Serial.println("Dirección IP: ");
    Serial.println(WiFi.localIP());

    // Inicializa el RTC
    rtcInitialized = rtc.begin();
    if (!rtcInitialized) {
      Serial.println("Error al inicializar el RTC");
    }

    // Obtener la hora de la API
    if (!adjustRTCFromAPI()) {
      Serial.println("Error: No se pudo obtener la hora. Por favor, reinicie la placa manualmente.");
    } else {
      // Mostrar pantalla de bienvenida después de obtener la hora correctamente
      DateTime now = rtc.now();
      printWelcomeMessage(now);
    }
  }

  // Verificar si los sensores están conectados correctamente
  verificarSensores();

  // Verificar la tarjeta SD escribiendo y leyendo un archivo de prueba
  if (!testSDCard()) {
    Serial.println("Error: No se puede escribir y leer en la tarjeta SD. Verifique la tarjeta y reinicie la placa.");
    sdErrorDisplayed = true;
  }

  // Prueba de creación de archivo en el setup
  DateTime now = rtc.now();
  char testFilename[32];
  snprintf(testFilename, sizeof(testFilename), "logfile.txt");
  Serial.print("Intentando crear archivo de prueba: ");
  Serial.println(testFilename);

  // Agregar el encabezado si el archivo no existe
  if (!SD.exists(testFilename)) {
    File testFile = SD.open(testFilename, FILE_WRITE);
    if (testFile) {
      testFile.println("Campus;Edificio;Numero;Entrada;Contador-Ingresos;Contador-Egresos;Ingreso-Egreso;Fecha;Hora");
      testFile.close();
      Serial.println("Archivo de prueba creado exitosamente con encabezado.");
    } else {
      Serial.println("Error al crear el archivo de prueba en la tarjeta SD");
    }
  } else {
    Serial.println("El archivo de prueba ya existe.");
  }
}

void loop() {
  if (sdErrorDisplayed) {
    return; // Detener el loop si hay un error con la tarjeta SD
  }

  // Verificación del sensor derecho
  unsigned int distanciaDerecho = sonarDerecho.ping_cm();
  if (distanciaDerecho <= (DISTANCIA_UMBRAL_DERECHO - TOLERANCIA)) {
    if (tiempoDerecho == 0) {
      tiempoDerecho = millis(); // Registrar el tiempo de activación
    }
  }

  // Verificación del sensor izquierdo
  unsigned int distanciaIzquierdo = sonarIzquierdo.ping_cm();
  if (distanciaIzquierdo <= (DISTANCIA_UMBRAL_IZQUIERDO - TOLERANCIA)) {
    if (tiempoIzquierdo == 0) {
      tiempoIzquierdo = millis(); // Registrar el tiempo de activación
    }
  }

  // Verificación de estado pausado
  if (distanciaDerecho <= (DISTANCIA_UMBRAL_DERECHO - TOLERANCIA) && distanciaIzquierdo <= (DISTANCIA_UMBRAL_IZQUIERDO - TOLERANCIA)) {
    pausado = true;
    Serial.println("Estado pausado");
  } else {
    pausado = false;
  }

  // Verificación de ingreso
  if (!pausado && tiempoDerecho > 0 && tiempoIzquierdo > tiempoDerecho && (millis() - tiempoDerecho) <= TIEMPO_RESPUESTA_MAXIMO) {
    Serial.println("Ingreso detectado");
    tiempoDerecho = 0;
    tiempoIzquierdo = 0;
    contadorIngresosA++;
    contadorIngresosEgresosA++;
    guardarEnSD("Ingreso", entradaA);
  }

  // Verificación de egreso
  if (!pausado && tiempoIzquierdo > 0 && tiempoDerecho > tiempoIzquierdo && (millis() - tiempoIzquierdo) <= TIEMPO_RESPUESTA_MAXIMO) {
    Serial.println("Egreso detectado");
    tiempoDerecho = 0;
    tiempoIzquierdo = 0;
    contadorEgresosA++;
    contadorIngresosEgresosA++;
    guardarEnSD("Egreso", entradaA);
  }

  // Resetear tiempos si los sensores se desactivan
  if (distanciaDerecho > (DISTANCIA_UMBRAL_DERECHO - TOLERANCIA)) {
    tiempoDerecho = 0;
  }

  if (distanciaIzquierdo > (DISTANCIA_UMBRAL_IZQUIERDO - TOLERANCIA)) {
    tiempoIzquierdo = 0;
  }

  delay(50); // Espera de 50 ms antes de la siguiente medición

  // Código adicional para los segundos sensores (Entrada B)
  /*
  // Verificación del sensor derecho B
  unsigned int distanciaDerechoB = sonarDerechoB.ping_cm();
  if (distanciaDerechoB <= (DISTANCIA_UMBRAL_DERECHO - TOLERANCIA)) {
    if (tiempoDerechoB == 0) {
      tiempoDerechoB = millis(); // Registrar el tiempo de activación
    }
  }

  // Verificación del sensor izquierdo B
  unsigned int distanciaIzquierdoB = sonarIzquierdoB.ping_cm();
  if (distanciaIzquierdoB <= (DISTANCIA_UMBRAL_IZQUIERDO - TOLERANCIA)) {
    if (tiempoIzquierdoB == 0) {
      tiempoIzquierdoB = millis(); // Registrar el tiempo de activación
    }
  }

  // Verificación de estado pausado para los segundos sensores (Entrada B)
  if (distanciaDerechoB <= (DISTANCIA_UMBRAL_DERECHO - TOLERANCIA) && distanciaIzquierdoB <= (DISTANCIA_UMBRAL_IZQUIERDO - TOLERANCIA)) {
    pausadoB = true;
    Serial.println("Estado pausado B");
  } else {
    pausadoB = false;
  }

  // Verificación de ingreso para los segundos sensores (Entrada B)
  if (!pausadoB && tiempoDerechoB > 0 && tiempoIzquierdoB > tiempoDerechoB && (millis() - tiempoDerechoB) <= TIEMPO_RESPUESTA_MAXIMO) {
    Serial.println("Ingreso detectado B");
    tiempoDerechoB = 0;
    tiempoIzquierdoB = 0;
    contadorIngresosB++;
    contadorIngresosEgresosB++;
    guardarEnSD("Ingreso", entradaB);
  }

  // Verificación de egreso para los segundos sensores (Entrada B)
  if (!pausadoB && tiempoIzquierdoB > 0 && tiempoDerechoB > tiempoIzquierdoB && (millis() - tiempoIzquierdoB) <= TIEMPO_RESPUESTA_MAXIMO) {
    Serial.println("Egreso detectado B");
    tiempoDerechoB = 0;
    tiempoIzquierdoB = 0;
    contadorEgresosB++;
    contadorIngresosEgresosB++;
    guardarEnSD("Egreso", entradaB);
  }

  // Resetear tiempos si los segundos sensores se desactivan (Entrada B)
  if (distanciaDerechoB > (DISTANCIA_UMBRAL_DERECHO - TOLERANCIA)) {
    tiempoDerechoB = 0;
  }

  if (distanciaIzquierdoB > (DISTANCIA_UMBRAL_IZQUIERDO - TOLERANCIA)) {
    tiempoIzquierdoB = 0;
  }
  */
}

void verificarSensores() {
  unsigned int distanciaDerecho = sonarDerecho.ping_cm();
  unsigned int distanciaIzquierdo = sonarIzquierdo.ping_cm();
  // unsigned int distanciaDerechoB = sonarDerechoB.ping_cm();
  // unsigned int distanciaIzquierdoB = sonarIzquierdoB.ping_cm();

  // Verificación de sensores Entrada A
  if (distanciaDerecho == 0) {
    Serial.println("Error en el sensor derecho de Entrada A. Por favor, verifique la conexión y reinicie la placa.");
    errorDisplayedA = true;
  }
  if (distanciaIzquierdo == 0) {
    Serial.println("Error en el sensor izquierdo de Entrada A. Por favor, verifique la conexión y reinicie la placa.");
    errorDisplayedA = true;
  }
  if (!errorDisplayedA) {
    Serial.println("Sensores Entrada A conectados correctamente.");
  }

  // Verificación de sensores Entrada B
  /*
  if (distanciaDerechoB == 0) {
    Serial.println("Error en el sensor derecho de Entrada B. Por favor, verifique la conexión y reinicie la placa.");
    errorDisplayedB = true;
  }
  if (distanciaIzquierdoB == 0) {
    Serial.println("Error en el sensor izquierdo de Entrada B. Por favor, verifique la conexión y reinicie la placa.");
    errorDisplayedB = true;
  }
  if (!errorDisplayedB) {
    Serial.println("Sensores Entrada B conectados correctamente.");
  }
  */
}

bool adjustRTCFromAPI() {
  WiFiClient client;

  if (!client.connect(host, httpPort)) {
    Serial.println("Error de conexión a worldtimeapi.org");
    return false;
  }

  // Realizar solicitud HTTP
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");

  // Esperar respuesta
  while (!client.available()) {
    delay(100);
  }

  // Leer la respuesta y buscar el cuerpo JSON
  String payload = "";
  bool jsonStart = false;
  while (client.available()) {
    String line = client.readStringUntil('\n');
    if (line.startsWith("{")) {
      jsonStart = true;
    }
    if (jsonStart) {
      payload += line;
    }
  }

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payload);
  
  if (error) {
    Serial.print(F("Error al parsear JSON: "));
    Serial.println(error.c_str());
    return false;
  }

  const char* datetime = doc["datetime"];
  int year, month, day, hour, minute, second;
  sscanf(datetime, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second);

  rtc.adjust(DateTime(year, month, day, hour, minute, second));
  return true;
}

void printWelcomeMessage(DateTime now) {
  Serial.println("**********************************************");
  Serial.println("* Bienvenido al Sistema de Gestión de Aforo  *");
  Serial.println("* de la Universidad Técnica Federico Santa   *");
  Serial.println("* María                                      *");
  Serial.println("**********************************************");
  Serial.println("Identificadores de Placa Arduino R4 WiFi:");
  Serial.print("Campus: "); Serial.println(campus);
  Serial.print("Edificio: "); Serial.println(edificio);
  Serial.print("Sala: "); Serial.println(numeroSala);
  Serial.print("Entrada: "); Serial.println("A y B");
  Serial.print("Hora: ");
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.println(now.second(), DEC);
  Serial.println();
}

void guardarEnSD(const char* tipo, const char* entrada) {
  int* contadorIngresos;
  int* contadorEgresos;
  int* contadorIngresosEgresos;

  if (strcmp(entrada, entradaA) == 0) {
    contadorIngresos = &contadorIngresosA;
    contadorEgresos = &contadorEgresosA;
    contadorIngresosEgresos = &contadorIngresosEgresosA;
  } else {
    contadorIngresos = nullptr;  // Dejar nulos ya que estamos comentando la parte B
    contadorEgresos = nullptr;
    contadorIngresosEgresos = nullptr;
    /*
    contadorIngresos = &contadorIngresosB;
    contadorEgresos = &contadorEgresosB;
    contadorIngresosEgresos = &contadorIngresosEgresosB;
    */
  }

  DateTime now = rtc.now();
  char filename[32];  // Cambiar el tamaño del buffer a 32 caracteres
  snprintf(filename, sizeof(filename), "logfile.txt");
  
  File dataFile = SD.open(filename, FILE_WRITE);
  if (dataFile) {
    dataFile.print(campus); dataFile.print(';');
    dataFile.print(edificio); dataFile.print(';');
    dataFile.print(numeroSala); dataFile.print(';');
    dataFile.print(entrada); dataFile.print(';');
    dataFile.print(*contadorIngresos); dataFile.print(';');
    dataFile.print(*contadorEgresos); dataFile.print(';');
    dataFile.print(tipo); dataFile.print(';');
    dataFile.print(now.year(), DEC); dataFile.print('/');
    dataFile.print(now.month(), DEC); dataFile.print('/');
    dataFile.print(now.day(), DEC); dataFile.print(';');
    dataFile.print(now.hour(), DEC); dataFile.print(':');
    dataFile.print(now.minute(), DEC); dataFile.print(':');
    dataFile.println(now.second(), DEC);
    dataFile.close();
    Serial.println("Archivo guardado en SD.");

    // Mensaje cuando se han guardado cinco registros nuevos
    if (*contadorIngresosEgresos >= 5) {
      Serial.println("Se han guardado los cinco registros nuevos");
      *contadorIngresosEgresos = 0;
    }
  } else {
    Serial.println("Error al guardar el archivo en la tarjeta SD");
  }
}

bool testSDCard() {
  // Crear un archivo de prueba
  File testFile = SD.open("test.txt", FILE_WRITE);
  if (!testFile) {
    return false;
  }
  testFile.println("Prueba de escritura en la tarjeta SD");
  testFile.close();

  // Leer el archivo de prueba
  testFile = SD.open("test.txt");
  if (!testFile) {
    return false;
  }
  while (testFile.available()) {
    testFile.read();
  }
  testFile.close();

  // Eliminar el archivo de prueba
  if (!SD.remove("test.txt")) {
    return false;
  }
  return true;
}

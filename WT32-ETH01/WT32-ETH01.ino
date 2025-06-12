#include <WebServer_WT32_ETH01.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// Configuración de Ethernet
#define ETH_PHY_TYPE        ETH_PHY_LAN8720
#define ETH_PHY_ADDR         0
#define ETH_PHY_MDC         23
#define ETH_PHY_MDIO        18
#define ETH_PHY_POWER       -1
#define ETH_CLK_MODE   ETH_CLOCK_GPIO0_IN

// Pines
#define PLC_TX_PIN 12
#define PLC_RX_PIN 14
#define RS485_CONTROL_PIN 15  // Nuevo pin para controlar RE y DE
#define GM65_TX_PIN 4
#define GM65_RX_PIN 2

HardwareSerial SerialGM65(1);
HardwareSerial SerialPLC(2);
Preferences preferences;

// Variables almacenadas en memoria con valores por defecto
String apiUrl = "https://servicios.siesacloud.com/api/siesa/v3.1/conectoresimportar?idCompania=7501&idSistema=2&idDocumento=196286&nombreDocumento=ENTREGAS_KOS";
String conniKey = "83abda414b156d28462c41d38d511f6d";
String conniToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJodHRwOi8vc2NoZW1hcy54bWxzb2FwLm9yZy93cy8yMDA1LzA1L2lkZW50aXR5L2NsYWltcy9uYW1laWRlbnRpZmllciI6ImIzNTEzOTFjLWViNDAtNDRiZS04NGQxLTFkMmNiZTllYThmYSIsImh0dHA6Ly9zY2hlbWFzLm1pY3Jvc29mdC5jb20vd3MvMjAwOC8wNi9pZGVudGl0eS9jbGFpbXMvcHJpbWFyeXNpZCI6IjMxOWFmNDU2LTllMGEtNDNkZC05MDAwLTc3M2QxYzRkYThjYiJ9.JKGmoT5-Kzw9n_c9e-Q20fwnTKc4sG2RTWAF01rwC14";
String f470_id_bodega = "020";
String f350_id_tipo_docto = "EPT";

// Variables temporales
String f350_fecha;

// Inicializar el servidor Ethernet
WebServer server(80);
bool internetConectado = false; //Conexión a internet

// Obtener fecha actual
String obtenerFechaActual() {
    configTime(-18000, 0, "pool.ntp.org");
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "20250226";
    }
    char fecha[9];
    strftime(fecha, sizeof(fecha), "%Y%m%d", &timeinfo);
    return String(fecha);
}

void setup() {
    Serial.begin(115200);
    SerialGM65.begin(9600, SERIAL_8N1, GM65_RX_PIN, GM65_TX_PIN);
    SerialPLC.begin(9600, SERIAL_8N1, PLC_RX_PIN, PLC_TX_PIN);
    pinMode(RS485_CONTROL_PIN, OUTPUT);
    digitalWrite(RS485_CONTROL_PIN, LOW);  // Recepción por defecto

    preferences.begin("config", false);
    apiUrl = preferences.getString("apiUrl", apiUrl);
    conniKey = preferences.getString("conniKey", conniKey);
    conniToken = preferences.getString("conniToken", conniToken);
    f470_id_bodega = preferences.getString("f470_id_bodega", f470_id_bodega);
    f350_id_tipo_docto = preferences.getString("f350_id_tipo_docto", f350_id_tipo_docto);
    preferences.end();

    Serial.println("Iniciando Ethernet...");
    recibirConfiguracionPLC();
    WT32_ETH01_onEvent();
    ETH.begin();  // Iniciar Ethernet

    unsigned long startTime = millis();
    bool connected = false;

    while (millis() - startTime < 10000) {  // Esperar hasta 10 segundos
        if (ETH.linkUp()) {  // Verificar si ya está conectado
            connected = true;
            break;
        }
        delay(500);  // Pequeñas pausas para evitar consumo excesivo de CPU
    }

    if (!connected) {  // Si no se conectó en el tiempo límite
        internetConectado = false;
        Serial.println("No se pudo conectar a Internet");
        digitalWrite(RS485_CONTROL_PIN, HIGH);
        SerialPLC.println("ERROR: Sin Internet");
        digitalWrite(RS485_CONTROL_PIN, LOW);
    } else {
        Serial.println("Ethernet conectado");
        internetConectado = true;
        Serial.println(ETH.localIP());
    }

    server.on("/", handleRoot);
    server.begin();
    f350_fecha = obtenerFechaActual(); 
}

void loop() {
    server.handleClient();
    bool estadoActualInternet = ETH.linkUp();
    if (estadoActualInternet != internetConectado) {
        internetConectado = estadoActualInternet;
        if (internetConectado) {
            Serial.println("Conexión a Internet restablecida");
            sendToPLC("Conexión a Internet restablecida");
        } else {
            Serial.println("Se perdió la conexión a Internet");
            sendToPLC("ERROR: Sin conexión a Internet");
        }
    }
    if (SerialGM65.available()) {
        String barcode = SerialGM65.readStringUntil('\n');
        barcode.trim();
        sendToPLC("#");
        Serial.println(barcode);
        if (internetConectado) {
            String jsonBody = generateJson(barcode);
            Serial.println("Enviando JSON:");
            Serial.println(jsonBody);
            sendJson(jsonBody);
        }
    }
}

void recibirConfiguracionPLC() {
    Serial.println("Esperando configuración del PLC...");
    String input;
    while (true) {
        if (SerialPLC.available()) {
            char c = SerialPLC.read();
            if (c == '#') break;
            if (c == '\n') {
                Serial.println(input);
                procesarConfiguracion(input);
                input = "";
            } else {
                input += c;
            }
        }
    }
    Serial.println("Configuración finalizada.");
}

void procesarConfiguracion(String input) {
    int delimiter = input.indexOf(": ");
    if (delimiter == -1) return;
    String key = input.substring(0, delimiter);
    String value = input.substring(delimiter + 2);
    
    preferences.begin("config", false);
    if (key == "apiUrl") preferences.putString("apiUrl", value);
    else if (key == "conniKey") preferences.putString("conniKey", value);
    else if (key == "conniToken") preferences.putString("conniToken", value);
    else if (key == "f470_id_bodega") preferences.putString("f470_id_bodega", value);
    else if (key == "f350_id_tipo_docto") preferences.putString("f350_id_tipo_docto", value);
    preferences.end();
}


String generateJson(String barcode) {
    int fechaIndex = -1;
    for (int i = 0; i <= barcode.length() - 8; i++) {
        String posible = barcode.substring(i, i + 8);
        int year  = posible.substring(0, 4).toInt();
        int month = posible.substring(4, 6).toInt();
        int day   = posible.substring(6, 8).toInt();

        // Ajusta estos rangos a tu contexto real
        if (year  >= 2000 && year  <= 2100 &&
            month >=    1 && month <=   12 &&
            day   >=    1 && day   <=   31) {
            fechaIndex = i;
            break;
        }
    }

    if (fechaIndex == -1) {
        Serial.println("Error: No se encontró una fecha válida en el código de barras.");
        return "{}";
    }

  // Extraer los campos del código de barras
  String f470_codigo_barras = barcode.substring(0, 10);
  String f850_consec_docto = barcode.substring(10, fechaIndex);
  String f470_id_lote = barcode.substring(fechaIndex, fechaIndex +15);
  String f470_cant_base_entrega = barcode.substring(fechaIndex+15, fechaIndex + 19);

  // Crear el JSON
  String jsonBody = "{";
  jsonBody += "\"Documentos\":[{";
  jsonBody += "\"f350_id_tipo_docto\":\"" + f350_id_tipo_docto + "\",";
  jsonBody += "\"f350_consec_docto\":\"1\",";
  jsonBody += "\"f350_fecha\":\"" + f350_fecha + "\",";
  jsonBody += "\"f350_notas\":\"\"";
  jsonBody += "}],";
  jsonBody += "\"Movimientos\":[{";
  jsonBody += "\"f470_id_tipo_docto\":\"" + f350_id_tipo_docto + "\",";
  jsonBody += "\"f470_consec_docto\":\"1\",";
  jsonBody += "\"f470_nro_registro\":\"1\",";
  jsonBody += "\"f850_consec_docto\":\"" + f850_consec_docto + "\",";
  jsonBody += "\"f470_id_item\":\"\",";
  jsonBody += "\"f470_referencia_item\":\"\",";
  jsonBody += "\"f470_codigo_barras\":\"" + f470_codigo_barras + "\",";
  jsonBody += "\"f470_id_ext1_detalle\":\"\",";
  jsonBody += "\"f470_id_ext2_detalle\":\"\",";
  jsonBody += "\"f470_id_bodega\":\"" + f470_id_bodega + "\",";
  jsonBody += "\"f470_id_lote\":\"" + f470_id_lote + "\",";
  jsonBody += "\"f470_cant_base_entrega\":\"" + f470_cant_base_entrega + "\"";
  jsonBody += "}]";
  jsonBody += "}";

  return jsonBody;
}



void sendToPLC(String data) {
    digitalWrite(RS485_CONTROL_PIN, HIGH);
    delay(500);
    Serial.println("Enviando al PLC:");
    Serial.println(data);
    SerialPLC.println(data);
    delay(500);
    digitalWrite(RS485_CONTROL_PIN, LOW);
    Serial.println("Enviado!");
}

void sendJson(String jsonBody) {
    HTTPClient http;
    http.begin(apiUrl);
    http.setTimeout(30000);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("ConniKey", conniKey);
    http.addHeader("ConniToken", conniToken);
    int httpResponseCode = http.POST(jsonBody);

    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("Respuesta de la API:");
        Serial.println(response);

        // Deserializar el JSON
        StaticJsonDocument<1024> doc; // Ajusta el tamaño según la complejidad de tu JSON
        DeserializationError error = deserializeJson(doc, response);

        if (!error) {
            // Acceder al valor de f_detalle
            if (doc.containsKey("detalle") && doc["detalle"].is<JsonArray>() && doc["detalle"][0].is<JsonObject>() && doc["detalle"][0].containsKey("f_detalle")) {
                String f_detalle = doc["detalle"][0]["f_detalle"].as<String>();
                Serial.print("f_detalle: ");
                Serial.println(f_detalle);
                Serial.println("Enviando f_detalle al PLC");
                sendToPLC("ERROR");
                sendToPLC(f_detalle);
            } else {
                Serial.println("Error: No se encontró el campo 'f_detalle' en la respuesta JSON o la estructura es incorrecta.");
                sendToPLC("OK");
            }
        } else {
            Serial.print("Error al deserializar la respuesta JSON: ");
            Serial.println(error.c_str());
        }
    } else {
        Serial.print("Error en la solicitud HTTP: ");
        Serial.println(http.errorToString(httpResponseCode));
        sendToPLC(http.errorToString(httpResponseCode));
    }
    http.end();
}

void handleRoot() {
    server.send(200, "text/plain", "Servidor WT32-ETH01 funcionando");
}

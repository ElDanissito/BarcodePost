------

# 🛠 Proyecto: Integración WT32-ETH01 + GM65 + PLC para Envío de Datos a API SIESA

Este proyecto conecta un escáner de códigos de barras GM65 y un PLC a través del módulo **WT32-ETH01** usando Ethernet. El sistema lee un código de barras, lo procesa, genera un JSON estructurado y lo envía automáticamente a un **endpoint de SIESA Cloud** mediante HTTP.

---

## 📦 Requisitos

### Hardware

* WT32-ETH01 (ESP32 con Ethernet LAN8720)
* Escáner de códigos de barras GM65 (UART)
* PLC (RS-485, UART)
* Fuente de 5V / 12V según el setup
* Módulo RS485 TTL (RE/DE controlados por GPIO)

### Software

* [Arduino IDE](https://www.arduino.cc/en/software)
* Librerías:

  * [`WebServer_WT32_ETH01`](https://github.com/khoih-prog/WebServer_WT32_ETH01)
  * `Preferences`
  * `HTTPClient`
  * `ArduinoJson`

---

## 🔌 Conexiones

| Dispositivo | Pin ESP32 | Notas                       |
| ----------- | --------- | --------------------------- |
| GM65 RX     | GPIO 4    | TX del ESP32 al RX del GM65 |
| GM65 TX     | GPIO 2    | RX del ESP32 al TX del GM65 |
| PLC TX      | GPIO 12   |                             |
| PLC RX      | GPIO 14   |                             |
| RE/DE RS485 | GPIO 15   | Control de transmisión      |
| MDC         | GPIO 23   | Ethernet LAN8720            |
| MDIO        | GPIO 18   | Ethernet LAN8720            |
| CLK         | GPIO 0    | Modo ETH\_CLOCK\_GPIO0\_IN  |

---

## ⚙️ Funcionamiento

1. **Inicio del sistema**
   El ESP32 inicializa la red Ethernet, abre un servidor web en el puerto `80` y espera configuración desde el PLC (en formato `clave: valor`).

2. **Lectura de código de barras (GM65)**
   Una vez escaneado un código, el sistema lo interpreta, valida que contenga una fecha, extrae los campos necesarios, y genera un JSON con estructura compatible con la API de SIESA.

3. **Envío de datos**
   Si hay conexión a Internet, se realiza un `POST` con headers personalizados (`ConniKey`, `ConniToken`) hacia la API de SIESA.

4. **Comunicación con el PLC (RS-485)**
   Se informa al PLC si hay errores, éxito o estado de la red.

---

## 🧠 Estructura del JSON generado

```json
{
  "Documentos": [
    {
      "f350_id_tipo_docto": "EPT",
      "f350_consec_docto": "1",
      "f350_fecha": "YYYYMMDD",
      "f350_notas": ""
    }
  ],
  "Movimientos": [
    {
      "f470_id_tipo_docto": "EPT",
      "f470_consec_docto": "1",
      "f470_nro_registro": "1",
      "f850_consec_docto": "...",
      "f470_codigo_barras": "...",
      "f470_id_bodega": "020",
      "f470_id_lote": "...",
      "f470_cant_base_entrega": "..."
    }
  ]
}
```

---

## 🌐 Configuración vía PLC

Se puede actualizar la configuración persistente enviando al PLC líneas con el siguiente formato:

```
apiUrl: https://tu-nueva-url.com/...
conniKey: abc123...
conniToken: jwt_token_aqui
f470_id_bodega: 020
f350_id_tipo_docto: EPT
#
```

El `#` al final indica que se ha terminado la configuración.

---

## 🚨 Consideraciones

* La conexión Ethernet puede tardar hasta 10 segundos; si no se logra, se notifica al PLC.
* El sistema espera que el código de barras contenga una **fecha en formato YYYYMMDD** para validar y separar los campos.
* El envío del JSON se realiza solo si hay conexión activa.

---

## 📁 Archivos importantes

* `WT32-ETH01.ino`: Lógica principal
* `README.md`: Documentación del proyecto

---

## 🤝 Créditos

Desarrollado por **Daniel Rojas**

Integración con SIESA Cloud y automatización industrial personalizada.

---

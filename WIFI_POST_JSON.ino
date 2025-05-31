#include <WiFi.h>
#include <Preferences.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Objeto para manejar las preferencias (NVS)
Preferences preferences;

// Servidor web en puerto 80
WebServer server(80);

// Cliente HTTP para peticiones
HTTPClient http;

// Variables para almacenar credenciales WiFi
String ssid = "";
String password = "";

// Variables para API
String lastApiResponse = "Sin respuestas aún";
String lastApiUrl = "";
String lastApiPayload = "";
int lastStatusCode = 0;
unsigned long lastRequestTime = 0;

// Configuración
const unsigned long WIFI_TIMEOUT = 10000; // 10 segundos timeout
const String NVS_NAMESPACE = "wifi_config";

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== ESP32 WiFi Manager con NVS ===");
  Serial.println("Inicializando sistema...\n");
  
  // Inicializar NVS
  if (!initializeNVS()) {
    Serial.println("Error: No se pudo inicializar NVS");
    return;
  }
  
  // Intentar conectar con credenciales almacenadas
  if (loadWiFiCredentials()) {
    Serial.println("Credenciales encontradas en memoria NVS");
    Serial.println("SSID: " + ssid);
    Serial.println("Intentando conectar...\n");
    
    if (connectToWiFi()) {
      Serial.println("¡Conexión WiFi exitosa!");
      printWiFiStatus();
      startWebServer(); // Iniciar servidor web después de conectar
    } else {
      Serial.println("No se pudo conectar con las credenciales almacenadas");
      requestNewCredentials();
    }
  } else {
    Serial.println("No se encontraron credenciales almacenadas");
    requestNewCredentials();
  }
}

void loop() {
  // Manejar peticiones del servidor web
  server.handleClient();
  
  // Verificar estado de conexión cada 30 segundos
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 30000) {
    lastCheck = millis();
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("¡Conexión WiFi perdida! Reintentando...");
      if (!connectToWiFi()) {
        Serial.println("No se pudo reconectar. Solicitando nuevas credenciales...");
        requestNewCredentials();
      } else {
        startWebServer(); // Reiniciar servidor web después de reconectar
      }
    } else {
      Serial.println("WiFi conectado - IP: " + WiFi.localIP().toString());
    }
  }
  
  delay(100);
}

bool initializeNVS() {
  // Inicializar las preferencias con el namespace
  preferences.begin(NVS_NAMESPACE.c_str(), false);
  Serial.println("Sistema NVS inicializado correctamente");
  return true;
}

bool loadWiFiCredentials() {
  // Leer SSID desde NVS
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  
  // Verificar si las credenciales existen
  if (ssid.length() > 0 && password.length() > 0) {
    return true;
  }
  
  return false;
}

bool saveWiFiCredentials() {
  // Guardar credenciales en NVS
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  
  Serial.println("Credenciales guardadas en memoria NVS");
  return true;
}

bool connectToWiFi() {
  if (ssid.length() == 0) {
    return false;
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  Serial.print("Conectando a " + ssid);
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  } else {
    WiFi.disconnect();
    return false;
  }
}

void requestNewCredentials() {
  Serial.println("\n=== Configuración de Red WiFi ===");
  Serial.println("Ingrese las credenciales de la red WiFi:");
  
  // Solicitar SSID
  Serial.print("SSID: ");
  while (Serial.available() == 0) {
    delay(100);
  }
  ssid = Serial.readStringUntil('\n');
  ssid.trim();
  
  // Solicitar contraseña
  Serial.print("Contraseña: ");
  while (Serial.available() == 0) {
    delay(100);
  }
  password = Serial.readStringUntil('\n');
  password.trim();
  
  Serial.println("\nCredenciales recibidas:");
  Serial.println("SSID: " + ssid);
  Serial.println("Contraseña: " + String(password.length()) + " caracteres\n");
  
  // Intentar conectar con las nuevas credenciales
  Serial.println("Intentando conectar con las nuevas credenciales...");
  if (connectToWiFi()) {
    Serial.println("¡Conexión exitosa!");
    saveWiFiCredentials();
    printWiFiStatus();
    startWebServer(); // Iniciar servidor web después de conectar
  } else {
    Serial.println("Error: No se pudo conectar con las credenciales proporcionadas");
    Serial.println("Verifique el SSID y contraseña e intente nuevamente\n");
    
    // Limpiar credenciales inválidas
    ssid = "";
    password = "";
    
    // Reintentar
    delay(2000);
    requestNewCredentials();
  }
}

void printWiFiStatus() {
  Serial.println("\n=== Estado de Conexión WiFi ===");
  Serial.println("Estado: Conectado");
  Serial.println("SSID: " + WiFi.SSID());
  Serial.println("IP: " + WiFi.localIP().toString());
  Serial.println("Gateway: " + WiFi.gatewayIP().toString());
  Serial.println("DNS: " + WiFi.dnsIP().toString());
  Serial.println("RSSI: " + String(WiFi.RSSI()) + " dBm");
  Serial.println("MAC: " + WiFi.macAddress());
  Serial.println("===============================\n");
}

void startWebServer() {
  // Configurar rutas del servidor web
  server.on("/", handleRoot);
  server.on("/api", HTTP_POST, handleApiPost);
  server.on("/status", handleStatus);
  server.onNotFound(handleNotFound);
  
  // Iniciar servidor
  server.begin();
  Serial.println("Servidor web iniciado en: http://" + WiFi.localIP().toString());
  Serial.println("Interfaz web disponible en: http://" + WiFi.localIP().toString() + "/");
}

void handleRoot() {
  String html = R"(
<!DOCTYPE html>
<html lang='es'>
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>ESP32 API Client</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 800px;
            margin: 20px auto;
            padding: 20px;
            background: #f5f5f5;
        }
        .container {
            background: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 {
            color: #333;
            text-align: center;
            margin-bottom: 30px;
        }
        .form-group {
            margin-bottom: 15px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
            color: #555;
        }
        input, textarea, select {
            width: 100%;
            padding: 10px;
            border: 1px solid #ddd;
            border-radius: 4px;
            font-size: 14px;
            box-sizing: border-box;
        }
        textarea {
            height: 120px;
            resize: vertical;
            font-family: monospace;
        }
        button {
            background: #007bff;
            color: white;
            padding: 12px 30px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 16px;
            margin-right: 10px;
        }
        button:hover {
            background: #0056b3;
        }
        .btn-secondary {
            background: #6c757d;
        }
        .btn-secondary:hover {
            background: #545b62;
        }
        .response-area {
            margin-top: 20px;
            padding: 15px;
            background: #f8f9fa;
            border-radius: 4px;
            border-left: 4px solid #007bff;
        }
        .status-success { border-left-color: #28a745; }
        .status-error { border-left-color: #dc3545; }
        .info-panel {
            background: #e9ecef;
            padding: 15px;
            border-radius: 4px;
            margin-bottom: 20px;
        }
        pre {
            background: #f8f9fa;
            padding: 10px;
            border-radius: 4px;
            overflow-x: auto;
            white-space: pre-wrap;
        }
    </style>
</head>
<body>
    <div class='container'>
        <h1>🌐 ESP32 API Client</h1>
        
        <div class='info-panel'>
            <strong>ESP32 Info:</strong><br>
            IP: )" + WiFi.localIP().toString() + R"(<br>
            SSID: )" + WiFi.SSID() + R"(<br>
            Estado: Conectado 
        </div>

        <form id='apiForm'>
            <div class='form-group'>
                <label for='method'>Método HTTP:</label>
                <select id='method' name='method'>
                    <option value='POST'>POST</option>
                    <option value='GET'>GET</option>
                    <option value='PUT'>PUT</option>
                    <option value='DELETE'>DELETE</option>
                </select>
            </div>

            <div class='form-group'>
                <label for='url'>URL del API:</label>
                <input type='url' id='url' name='url' placeholder='https://jsonplaceholder.typicode.com/posts' required>
            </div>

            <div class='form-group'>
                <label for='headers'>Headers (JSON):</label>
                <textarea id='headers' name='headers' placeholder='{"Content-Type": "application/json", "Authorization": "Bearer token"}'></textarea>
            </div>

            <div class='form-group'>
                <label for='payload'>Payload JSON:</label>
                <textarea id='payload' name='payload' placeholder='{"title": "Test", "body": "Mensaje de prueba", "userId": 1}'></textarea>
            </div>

            <button type='submit'> Enviar Petición</button>
            <button type='button' onclick='loadStatus()' class='btn-secondary'>🔄 Ver Estado</button>
        </form>

        <div id='responseArea' style='display:none'>
            <h3> Respuesta del Servidor</h3>
            <div id='responseContent'></div>
        </div>
    </div>

    <script>
        document.getElementById('apiForm').addEventListener('submit', async function(e) {
            e.preventDefault();
            
            const formData = new FormData(this);
            const data = {
                method: formData.get('method'),
                url: formData.get('url'),
                headers: formData.get('headers'),
                payload: formData.get('payload')
            };

            try {
                const response = await fetch('/api', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify(data)
                });
                
                const result = await response.json();
                displayResponse(result);
            } catch (error) {
                displayResponse({success: false, error: 'Error de conexión: ' + error.message});
            }
        });

        function displayResponse(data) {
            const area = document.getElementById('responseArea');
            const content = document.getElementById('responseContent');
            
            area.style.display = 'block';
            area.className = 'response-area ' + (data.success ? 'status-success' : 'status-error');
            
            content.innerHTML = '<pre>' + JSON.stringify(data, null, 2) + '</pre>';
        }

        async function loadStatus() {
            try {
                const response = await fetch('/status');
                const data = await response.json();
                displayResponse(data);
            } catch (error) {
                displayResponse({success: false, error: 'Error al cargar estado'});
            }
        }

        // Ejemplos predefinidos
        const examples = {
            'POST': {
                url: 'https://jsonplaceholder.typicode.com/posts',
                headers: '{"Content-Type": "application/json"}',
                payload: '{"title": "Test desde ESP32", "body": "Mensaje de prueba", "userId": 1}'
            },
            'GET': {
                url: 'https://jsonplaceholder.typicode.com/posts/1',
                headers: '{}',
                payload: ''
            }
        };

        document.getElementById('method').addEventListener('change', function() {
            const method = this.value;
            const example = examples[method];
            if (example) {
                document.getElementById('url').value = example.url;
                document.getElementById('headers').value = example.headers;
                document.getElementById('payload').value = example.payload;
            }
        });
    </script>
</body>
</html>
  )";
  
  server.send(200, "text/html", html);
}

void handleApiPost() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"success\": false, \"error\": \"No se recibió payload JSON\"}");
    return;
  }

  String body = server.arg("plain");
  DynamicJsonDocument doc(2048);
  
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    server.send(400, "application/json", "{\"success\": false, \"error\": \"JSON inválido\"}");
    return;
  }

  // Extraer datos de la petición
  String method = doc["method"] | "POST";
  String url = doc["url"] | "";
  String headersStr = doc["headers"] | "{}";
  String payload = doc["payload"] | "";

  if (url.length() == 0) {
    server.send(400, "application/json", "{\"success\": false, \"error\": \"URL requerida\"}");
    return;
  }

  // Realizar petición HTTP
  String response = makeHttpRequest(method, url, headersStr, payload);
  server.send(200, "application/json", response);
}

String makeHttpRequest(String method, String url, String headersStr, String payload) {
  DynamicJsonDocument responseDoc(4096);
  lastRequestTime = millis();
  lastApiUrl = url;
  lastApiPayload = payload;
  
  http.begin(url);
  
  // Configurar headers
  DynamicJsonDocument headersDoc(1024);
  deserializeJson(headersDoc, headersStr);
  
  for (JsonPair header : headersDoc.as<JsonObject>()) {
    http.addHeader(header.key().c_str(), header.value().as<String>());
  }
  
  // Realizar petición según el método
  int httpResponseCode = -1;
  String httpResponse = "";
  
  if (method == "POST") {
    httpResponseCode = http.POST(payload);
  } else if (method == "GET") {
    httpResponseCode = http.GET();
  } else if (method == "PUT") {
    httpResponseCode = http.PUT(payload);
  } else if (method == "DELETE") {
    httpResponseCode = http.sendRequest("DELETE", payload);
  }
  
  lastStatusCode = httpResponseCode;
  
  if (httpResponseCode > 0) {
    httpResponse = http.getString();
    lastApiResponse = httpResponse;
    
    responseDoc["success"] = true;
    responseDoc["statusCode"] = httpResponseCode;
    responseDoc["method"] = method;
    responseDoc["url"] = url;
    responseDoc["timestamp"] = millis();
    
    // Intentar parsear respuesta como JSON
    DynamicJsonDocument apiResponseDoc(2048);
    DeserializationError parseError = deserializeJson(apiResponseDoc, httpResponse);
    
    if (parseError) {
      responseDoc["response"] = httpResponse; // Respuesta como texto
    } else {
      responseDoc["response"] = apiResponseDoc; // Respuesta como JSON
    }
    
    Serial.println("\n=== Petición HTTP Enviada ===");
    Serial.println("Método: " + method);
    Serial.println("URL: " + url);
    Serial.println("Código: " + String(httpResponseCode));
    Serial.println("Respuesta: " + httpResponse.substring(0, 200) + (httpResponse.length() > 200 ? "..." : ""));
    Serial.println("============================\n");
    
  } else {
    lastApiResponse = "Error: " + String(httpResponseCode);
    responseDoc["success"] = false;
    responseDoc["error"] = "Error HTTP: " + String(httpResponseCode);
    responseDoc["statusCode"] = httpResponseCode;
    
    Serial.println("Error en petición HTTP: " + String(httpResponseCode));
  }
  
  http.end();
  
  String jsonResponse;
  serializeJson(responseDoc, jsonResponse);
  return jsonResponse;
}

void handleStatus() {
  DynamicJsonDocument doc(2048);
  
  doc["wifi"]["connected"] = WiFi.status() == WL_CONNECTED;
  doc["wifi"]["ssid"] = WiFi.SSID();
  doc["wifi"]["ip"] = WiFi.localIP().toString();
  doc["wifi"]["rssi"] = WiFi.RSSI();
  
  doc["api"]["lastUrl"] = lastApiUrl;
  doc["api"]["lastStatusCode"] = lastStatusCode;
  doc["api"]["lastRequestTime"] = lastRequestTime;
  doc["api"]["lastResponse"] = lastApiResponse.substring(0, 500);
  
  doc["system"]["uptime"] = millis();
  doc["system"]["freeHeap"] = ESP.getFreeHeap();
  doc["system"]["timestamp"] = millis();
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleNotFound() {
  server.send(404, "text/plain", "Página no encontrada");
}

void clearStoredCredentials() {
  // Función para limpiar credenciales almacenadas (útil para debug)
  preferences.remove("ssid");
  preferences.remove("password");
  Serial.println("Credenciales eliminadas de la memoria NVS");
}

// Función para mostrar información de memoria NVS
void showNVSInfo() {
  Serial.println("\n=== Información de Memoria NVS ===");
  Serial.println("Namespace: " + NVS_NAMESPACE);
  
  String stored_ssid = preferences.getString("ssid", "No encontrado");
  String stored_pass = preferences.getString("password", "No encontrado");
  
  Serial.println("SSID almacenado: " + stored_ssid);
  Serial.println("Contraseña almacenada: " + (stored_pass != "No encontrado" ? String(stored_pass.length()) + " caracteres" : "No encontrada"));
  Serial.println("================================\n");
}

#include <WiFi.h>
#include <Preferences.h>

// Objeto para manejar las preferencias (NVS)
Preferences preferences;

// Variables para almacenar credenciales WiFi
String ssid = "";
String password = "";

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
  // Verificar estado de conexión cada 30 segundos
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 30000) {
    lastCheck = millis();
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("¡Conexión WiFi perdida! Reintentando...");
      if (!connectToWiFi()) {
        Serial.println("No se pudo reconectar. Solicitando nuevas credenciales...");
        requestNewCredentials();
      }
    } else {
      Serial.println("WiFi conectado - IP: " + WiFi.localIP().toString());
    }
  }
  
  delay(1000);
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
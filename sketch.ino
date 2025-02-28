#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "HX711.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";

#define solenoid 26

const int PINO_DOUT_CELULA_CARGA = 16;
const int PINO_SCK_CELULA_CARGA = 4;
HX711 balanca;
float fator_calibracao = -1365;
float limiteMinimo = 10;
float volumeTotal = 50; 
String base_url = "https://api.tago.io/data";
String TAGOIO_DEVICE_TOKEN = "3207e1fb-e308-4de0-9664-29df4d7d20ae";

float ultimoNivel = -1;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const byte ROW_NUM = 4;
const byte COLUMN_NUM = 4;
char keys[ROW_NUM][COLUMN_NUM] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte pin_rows[ROW_NUM] = {12, 13, 14, 15}; 
byte pin_column[COLUMN_NUM] = {5, 17, 18, 19}; 
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

void displayValues() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Limite Minimo: ");
  display.print(limiteMinimo);
  display.setCursor(0, 10);
  display.print("Volume Total: ");
  display.print(volumeTotal);
  display.display();
}

String readNumberFromKeypad() {
  String input = "";
  while (true) {
    char key = keypad.getKey();
    if (key) {
      if (key == '#') break;
      if (key >= '0' && key <= '9') {
        input += key;
        Serial.print(key);
      }
    }
    delay(50);
  }
  Serial.println();
  return input;
}

void setup_wifi() {
  Serial.print("Conectando ao WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado com sucesso!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void sendTagoIoData(float nivel) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(base_url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Device-Token", TAGOIO_DEVICE_TOKEN);

    StaticJsonDocument<200> doc;
    doc["variable"] = "Nivel";
    doc["value"] = nivel;

    String payload;
    serializeJson(doc, payload);

    int responseCode = http.POST(payload);
    Serial.print("HTTP Response Code: ");
    Serial.println(responseCode);

    http.end();
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  pinMode(solenoid, OUTPUT);

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  displayValues();

  Serial.println("Inicializando sensor...");
  balanca.begin(PINO_DOUT_CELULA_CARGA, PINO_SCK_CELULA_CARGA);
  balanca.set_scale();
  delay(1000);
  Serial.println("Sensor pronto.");
}

void loop() {
  char key = keypad.getKey();
  
  if (key) {
    Serial.print("Tecla pressionada: ");
    Serial.println(key);
    
    if (key == 'A') {
      Serial.println("Digite o novo Limite Minimo e pressione #: ");
      limiteMinimo = readNumberFromKeypad().toFloat();
    } else if (key == 'C') {
      Serial.println("Digite o novo Volume Total e pressione #: ");
      volumeTotal = readNumberFromKeypad().toFloat();
    } else if (key == '#') {
      Serial.println("Valores resetados!");
      limiteMinimo = 10;
      volumeTotal = 50;
    }
    displayValues();
  }


  unsigned int ADC = balanca.get_units();
  float peso = float(ADC) / 2100 * 5;
  float nivel = (peso / volumeTotal) * 100;

  Serial.print("Nível = ");
  Serial.print(nivel);
  Serial.println("%\n");

  if (nivel > limiteMinimo) {
    if (nivel != ultimoNivel) {
      sendTagoIoData(nivel);
      ultimoNivel = nivel;
      digitalWrite(solenoid, LOW);
    }
  } else {
    digitalWrite(solenoid, HIGH);
    Serial.println("Reservatório atingiu o nível mínimo. Fornecimento interrompido!");
    sendTagoIoData(nivel);  
  }
  delay(500);
}
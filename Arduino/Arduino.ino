#include <WiFi.h>
#include <time.h>
#include <HTTPClient.h>
#include "Grove_Temperature_And_Humidity_Sensor.h"

// Variáveis WiFi e NTP
String uid;
char *ssid = "Nome WIFI";
char *pwd = "Senha WIFI";
char *ntpServer = "br.pool.ntp.org";
long gmtOffset = -3 * 3600; // Ajuste do fuso horário em segundos
int daylight = 0;
time_t now;
struct tm timeinfo;
String serverName = "http://IPMaquina:4000/mongodb/salvarMedida";

// Definições de pinos e tipos dos sensores
#define DHTTYPE DHT11
#define LUMINOSIDADEPIN 4
#define DHTPIN 14

DHT dht(DHTPIN, DHTTYPE); 

// Variáveis para tasks e semáforos
TaskHandle_t task1;
TaskHandle_t task2;
SemaphoreHandle_t mutex;

// Função para conectar ao WiFi
void connectWiFi() {
  Serial.print("Conectando ");
  WiFi.begin(ssid, pwd);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Conectado com sucesso, com o IP ");
  Serial.println(WiFi.localIP());
}

// Função para verificar a conectividade com o servidor
bool isServerAvailable() {
  WiFiClient client;
  if (client.connect("IPMaquina", 4000)) {
    client.stop();
    return true;
  }
  return false;
}

// Função para enviar dados ao servidor
void sendData(String payload) {
  if (WiFi.status() == WL_CONNECTED) {
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Erro ao coletar data/hora");
    } else {
      time(&now); // Atualize o tempo antes de usá-lo

      if (isServerAvailable()) {
        WiFiClient wclient;
        HTTPClient http_post;

        Serial.println("Iniciando conexão HTTP para enviar dados");

        http_post.begin(wclient, serverName);
        http_post.setTimeout(10000); // Aumenta o timeout para 10 segundos
        http_post.addHeader("Content-Type", "application/json");
        http_post.addHeader("x-api-key", "4554545sdsdsd5454");

        Serial.print("Enviando dados: ");
        Serial.println(payload);

        int http_response_code = http_post.POST(payload.c_str());

        Serial.print("Código de resposta HTTP: ");
        Serial.println(http_response_code);
        if (http_response_code > 0) {
          Serial.println("Resposta do servidor:");
          Serial.println(http_post.getString());
        } else {
          Serial.print("Erro ao executar o POST. Código: ");
          Serial.println(http_response_code);
        }
        http_post.end(); // Não se esqueça de finalizar a conexão
      } else {
        Serial.println("Servidor não está acessível");
      }
    }
  } else {
    Serial.println("Na Fatec nunca tem internet...");
    connectWiFi();
  }
}

// Task 1 para atualizar e enviar temperatura e umidade
void minhaTask1(void *pvParameters) {
  Serial.println("Começou a Task1");
  while (true) {
    // Região crítica
    xSemaphoreTake(mutex, portMAX_DELAY);
    float temp_hum_val[2] = {0};
    if (!dht.readTempAndHumidity(temp_hum_val)) {
      float temp = temp_hum_val[1];
      float umi = temp_hum_val[0];
    } else {
      Serial.println("Falha ao ler temperatura e umidade.");
    }
    xSemaphoreGive(mutex);

    String payload = "{\"uid\":\"" + uid + "\",\"unixtime\":" + String(now) + ",\"Temp\":" + String(temp) + ",\"Umidade\":" + String(umi) + "}";
    sendData(payload);

    delay(10000); // Delay de 10 segundos entre envios
  }
}

// Task 2 para atualizar e enviar luminosidade
void minhaTask2(void *pvParameters) {
  Serial.println("Começou a Task2");
  while (true) {
    // Região crítica
    xSemaphoreTake(mutex, portMAX_DELAY);
    analogReadResolution(10);
    float volts = analogRead(LUMINOSIDADEPIN) * 5 / 1024.0;
    float amps = volts / 10000.0;
    float microamps = amps * 1000000;
    float lux = microamps * 2.0;
    xSemaphoreGive(mutex);

    String payload = "{\"uid\":\"" + uid + "\",\"unixtime\":" + String(now) + ",\"Luminosidade\":" + String(lux) + "}";
    sendData(payload);

    delay(10000); // Delay de 10 segundos entre envios
  }
}

void setup() {
  Serial.begin(115200);
  uid = WiFi.macAddress();
  uid.replace(":", "");
  connectWiFi();
  configTime(gmtOffset, daylight, ntpServer);

  if (!getLocalTime(&timeinfo)) {
    Serial.println("Erro ao acessar o servidor NTP");
  } else {
    Serial.print("A hora agora é ");
    time(&now);
    Serial.println(now);
  }

  // Inicializa o sensor DHT
  dht.begin();
  
  // Criação do semáforo
  mutex = xSemaphoreCreateMutex();
  if (mutex == NULL)
    Serial.println("Erro ao criar o mutex");

  // Criação das tasks com tamanho de pilha aumentado
  Serial.println("Criando a task 1");
  xTaskCreatePinnedToCore(
    minhaTask1, // função da task
    "MinhaTask1", // nome da task
    2048, // tamanho da task (aumentado)
    NULL, // parâmetros task
    1, // prioridade da task
    &task1, // task handle
    0 // core (loop = 0)
  );

  Serial.println("Criando a task 2");
  xTaskCreatePinnedToCore(
    minhaTask2, // função da task
    "MinhaTask2", // nome da task
    2048, // tamanho da task (aumentado)
    NULL, // parâmetros task
    1, // prioridade da task
    &task2, // task handle
    1 // core (loop = 1)
  );
}

void loop() {
  // Região crítica para leitura segura dos dados para debug
  xSemaphoreTake(mutex, portMAX_DELAY);
  Serial.print("Temperatura: ");
  Serial.println(temp);
  Serial.print("Umidade: ");
  Serial.println(umi);
  Serial.print("Luminosidade: ");
  Serial.println(lux);
  xSemaphoreGive(mutex);

  delay(5000);
}

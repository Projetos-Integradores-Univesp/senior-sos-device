#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================= Configurações de Hardware =================
const int BUTTON_PIN = 0; 

// Configurações do Display OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ================= Configurações da Rede e MQTT =================
const char* ssid = "OnePlus 13 3EC0";
const char* password = "ippt2993";
const char* mqtt_server = "iot.gtpc.com.br"; // Ou IP do seu VPS
const int mqtt_port = 1883;

// Tópicos MQTT
const char* topico_hw = "gtpc/iot/botao";
const char* topico_virtual = "gtpc/iot/virtual";

WiFiClient espClient;
PubSubClient client(espClient);

// ================= Variáveis de Estado =================
int lastButtonState = HIGH;
String estadoBotaoHW = "SOLTO";
String estadoVirtual = "OFF"; // Variável interna controlada pela Web

// Função para atualizar as informações na tela OLED
void atualizarDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Linha 1: IP do ESP8266
  display.setCursor(0, 0);
  display.print("IP: ");
  display.println(WiFi.localIP().toString());
  
  // Linha 2: Status MQTT
  display.setCursor(0, 12);
  display.print("MQTT: ");
  display.println(client.connected() ? "CONECTADO" : "CAIU");
  
  display.drawLine(0, 22, 128, 22, SSD1306_WHITE); // Linha divisória
  
  // Linha 3: Status Botão Físico
  display.setCursor(0, 30);
  display.print("Botao HW: ");
  display.println(estadoBotaoHW);
  
  // Linha 4: Status Variável Virtual
  display.setCursor(0, 45);
  display.print("Var Web : ");
  display.println(estadoVirtual);
  
  display.display();
}

// Função executada sempre que chega uma mensagem MQTT da Nuvem
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String mensagem = "";
  for (int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }
  
  // Verifica se a mensagem veio do tópico virtual
  if (String(topic) == topico_virtual) {
    estadoVirtual = mensagem;
    Serial.println("Comando Web Recebido: " + estadoVirtual);
    atualizarDisplay(); // Atualiza a tela com o novo valor
  }
}

void setup_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "ESP8266-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      client.publish(topico_hw, estadoBotaoHW.c_str()); // Publica estado atual
      client.subscribe(topico_virtual); // Assina o tópico para receber comandos da Web
      atualizarDisplay();
    } else {
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Configurando o I2C para GPIO12 (SDA) e GPIO14 (SCL)
  Wire.begin(14, 12);
  
  // Inicializa o OLED no endereço 0x3C
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Falha ao iniciar o OLED"));
    for(;;);
  }
  
  // Tela inicial
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println("Conectando WiFi...");
  display.display();

  setup_wifi();
  
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback); // Registra a função de escuta
  
  atualizarDisplay();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Mantém o MQTT vivo e processa callbacks

  // Lógica do botão físico (com Debounce)
  int currentButtonState = digitalRead(BUTTON_PIN);
  if (currentButtonState != lastButtonState) {
    delay(50);
    currentButtonState = digitalRead(BUTTON_PIN);
    
    if (currentButtonState != lastButtonState) {
      if (currentButtonState == LOW) {
        estadoBotaoHW = "PRESSIONADO";
        client.publish(topico_hw, "PRESSIONADO");
      } else {
        estadoBotaoHW = "SOLTO";
        client.publish(topico_hw, "SOLTO");
      }
      lastButtonState = currentButtonState;
      atualizarDisplay(); // Atualiza a tela com o clique físico
    }
  }
}
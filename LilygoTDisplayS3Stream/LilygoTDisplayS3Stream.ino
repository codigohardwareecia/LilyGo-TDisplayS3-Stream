#include <WiFi.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>

// Configurações de Rede
const char* ssid = "Archenar";
const char* password = "PASS1234567890000";
WiFiServer server(4210);

TFT_eSPI tft = TFT_eSPI();
uint8_t frameBuffer[40000]; // Buffer para o JPEG (40KB é seguro para 320x170)

// Função de saída para o decodificador JPEG
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return false;
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

void setup() {
  // 1. Pinos de hardware do T-Display-S3
  pinMode(15, OUTPUT); digitalWrite(15, HIGH);
  pinMode(38, OUTPUT); digitalWrite(38, HIGH);

  tft.init();
  tft.setSwapBytes(true);
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // 2. Tela de Boas-vindas / Status
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawCentreString("SISTEMA JARVIS VIDEO", 160, 10, 2);
  tft.drawFastHLine(0, 35, 320, TFT_WHITE);
  
  tft.setCursor(0, 50);
  tft.setTextColor(TFT_WHITE);
  tft.println(" Iniciando Wi-Fi...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
  }

  // 3. Exibição do IP e início do Servidor
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN);
  tft.drawCentreString("CONECTADO!", 160, 40, 4);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(0, 85);
  tft.print(" IP: ");
  tft.println(WiFi.localIP());
  tft.println(" Porta: 4210 (TCP)");
  tft.println("\n Aguardando stream...");

  server.begin();
  
  // Configura o decodificador
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);
}

void loop() {
  WiFiClient client = server.available(); // Verifica se o C# conectou

  if (client) {
    // Enquanto o app C# estiver conectado
    while (client.connected()) {
      // O C# deve mandar 4 bytes com o TAMANHO da imagem primeiro
      if (client.available() >= 4) {
        uint32_t imgSize;
        client.read((uint8_t*)&imgSize, 4);
        
        if (imgSize > sizeof(frameBuffer)) {
            // Se a imagem for maior que o buffer, descarta para não travar
            while(client.available()) client.read();
            continue;
        }

        // Lê os bytes da imagem baseados no tamanho recebido
        uint32_t bytesRead = 0;
        uint32_t timeout = millis();
        while (bytesRead < imgSize && (millis() - timeout < 2000)) {
          if (client.available()) {
            frameBuffer[bytesRead++] = client.read();
          }
        }

        // Se recebeu tudo, desenha na tela
        if (bytesRead == imgSize) {
          TJpgDec.drawJpg(0, 0, frameBuffer, imgSize);
        }
      }
    }
    // Se o C# desconectar, volta a mostrar o IP
    setup(); 
  }
}

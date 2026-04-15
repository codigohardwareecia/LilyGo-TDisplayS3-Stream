# Comunicando-se com o Lilygo T Display S3 - Stream

### Código Arduino (ESP32-S3)

Abra o Arduino IDE e crie um novo Sketch

**Adicionar a URl com as definições de pacote do ESP32 T Displauy S3**

Vá em **File > Preferences** > **Additional Boards Manager URLs**, cole o seguinte link: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
    
Vá em **Tools > Board > Boards Manager**, procure por **esp32** (da Espressif Systems) e instale a versão mais recente.

**Download das configuraçöes de pinos**

Baixa o repositório do GitHub
https://github.com/Xinyuan-LilyGO/T-Display-S3

Após baixar o repositório, acesse a pasta Lib e copie a pasta TFT_eSPI para a biblioteca do Arduino que fica por padrão em "C:\Users\seu user\Documents\Arduino\libraries"

Também instalar biblioteca `TJpg_Decoder`.

O código do projeto e listado abaixo, copie e cole o código no seu Sketch, não esqueça de alterar o ssid e o password

```C++
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>

// Configurações de Rede
const char* ssid = "SUA_REDE_WIFI";
const char* password = "Senha";
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
```

### Código C# (.NET 8)

Instalar pacote NuGet `System.Drawing.Common`. **Rodar como Administrador**.

C#

```
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Net.Sockets;
using System.Text;

string esp32Ip = "192.168.15.132";
TcpClient client = new TcpClient(esp32Ip, 4210);
NetworkStream stream = client.GetStream();

int targetWidth = 320;
int targetHeight = 170;

while (client.Connected)
{
    // Ajuste para sua resolução se necessário
    using Bitmap screenshot = new Bitmap(1920, 1080); 
    using (Graphics g = Graphics.FromImage(screenshot))
    {
        g.CopyFromScreen(0, 0, 0, 0, screenshot.Size);
    }

    // 2. Redimensiona para o tamanho do LCD do S3
    using Bitmap resized = new Bitmap(targetWidth, targetHeight);
    using (Graphics g = Graphics.FromImage(resized))
    {
        // O SEGREDO ESTÁ AQUI:
        g.InterpolationMode = InterpolationMode.HighQualityBicubic;
        g.SmoothingMode = SmoothingMode.HighQuality;
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
        g.CompositingQuality = CompositingQuality.HighQuality;

        g.DrawImage(screenshot, new Rectangle(0, 0, targetWidth, targetHeight));
    }

    byte[] img = GetJpegBytes(resized, 40); // Qualidade 50 (fica nítido)

    // Manda o tamanho da imagem primeiro (4 bytes) para o ESP saber quanto ler
    byte[] size = BitConverter.GetBytes(img.Length);
    stream.Write(size, 0, 4);

    // Manda a imagem
    stream.Write(img, 0, img.Length);
    Thread.Sleep(1);
}
static byte[] GetJpegBytes(Bitmap bitmap, long quality)
{
    EncoderParameters encoderParameters = new EncoderParameters(1);
    encoderParameters.Param[0] = new EncoderParameter(System.Drawing.Imaging.Encoder.Quality, quality);
    ImageCodecInfo codecInfo = ImageCodecInfo.GetImageDecoders().First(c => c.FormatID == ImageFormat.Jpeg.Guid);

    using MemoryStream ms = new MemoryStream();
    bitmap.Save(ms, codecInfo, encoderParameters);
    return ms.ToArray();
}

```

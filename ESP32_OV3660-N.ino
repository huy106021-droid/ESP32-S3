#include <WiFi.h>
#include <WebServer.h>
#include <driver/i2s.h>

// ===== WIFI =====
const char* ssid = "TANG 3-4 NHA 15";
const char* password = "tang34doanket2023";

// ===== I2S =====
#define I2S_WS 42
#define I2S_SD 2
#define I2S_SCK 41
#define I2S_PORT I2S_NUM_0

#define bufferLen 1024
int16_t sBuffer[bufferLen];

WebServer server(80);

// ===== I2S SETUP =====
void i2s_install() {
  const i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 512,
    .use_apll = true
  };
  i2s_driver_install(I2S_PORT, &config, 0, NULL);
}

void i2s_setpin() {
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };
  i2s_set_pin(I2S_PORT, &pin_config);
}

// ===== STREAM HANDLER =====
void handleAudio() {
  WiFiClient client = server.client();

  // HTTP header
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: audio/wav");
  client.println("Connection: close");
  client.println();

  // WAV HEADER (PCM 16bit mono 16kHz)
  uint8_t wavHeader[44] = {
    'R','I','F','F', 0,0,0,0, 'W','A','V','E',
    'f','m','t',' ', 16,0,0,0, 1,0, 1,0,
    0x80,0x3E,0,0, 0x00,0x7D,0,0,
    2,0, 16,0,
    'd','a','t','a', 0,0,0,0
  };

  client.write(wavHeader, 44);

  while (client.connected()) {
    size_t bytesRead;
    i2s_read(I2S_PORT, sBuffer, bufferLen * sizeof(int16_t), &bytesRead, portMAX_DELAY);

    if (bytesRead > 0) {
      client.write((uint8_t*)sBuffer, bytesRead);
    }
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());

  i2s_install();
  i2s_setpin();
  i2s_start(I2S_PORT);

  server.on("/audio", handleAudio);
  server.begin();
}

// ===== LOOP =====
void loop() {
  server.handleClient();
}

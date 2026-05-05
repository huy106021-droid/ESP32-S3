#include <WiFi.h>
#include <WebServer.h>
#include <driver/i2s.h>

// ===== WIFI =====
const char* ssid = "TANG 3-4 NHA 15";
const char* password = "tang34doanket2023";

// ===== I2S PIN (tránh trùng camera) =====
#define I2S_WS   42
#define I2S_SCK  41
#define I2S_SD   39   // đổi từ GPIO2 -> 39 cho ổn định

#define LR_PIN   38   // chọn Left/Right

#define I2S_PORT I2S_NUM_0

#define bufferLen 512
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
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 512,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
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
    size_t bytesRead = 0;

    if (i2s_read(I2S_PORT, sBuffer, sizeof(sBuffer), &bytesRead, portMAX_DELAY) == ESP_OK) {
      if (bytesRead > 0) {
        client.write((uint8_t*)sBuffer, bytesRead);
      }
    }

    // tránh watchdog reset
    delay(1);
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32-S3 CAM AUDIO STREAM");

  // ===== L/R SELECT =====
  pinMode(LR_PIN, OUTPUT);
  digitalWrite(LR_PIN, LOW);   // LOW = LEFT, HIGH = RIGHT

  // ===== WIFI =====
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // ===== I2S =====
  i2s_install();
  i2s_setpin();
  i2s_zero_dma_buffer(I2S_PORT);
  i2s_start(I2S_PORT);

  // ===== SERVER =====
  server.on("/audio", handleAudio);
  server.begin();

  Serial.println("Open: http://IP/audio");
}

// ===== LOOP =====
void loop() {
  server.handleClient();
}

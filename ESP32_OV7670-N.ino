#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

//================ WIFI =================
const char* ssid = "TANG 3-4 NHA 15";
const char* pass = "tang34doanket2023";

//================ PIN =================
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5

#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM       11
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

//================ STREAM =================
#define PART_BOUNDARY "123456789000000000000987654321"

static const char* STREAM_CONTENT_TYPE =
"multipart/x-mixed-replace;boundary=" PART_BOUNDARY;

static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";

static const char* STREAM_PART =
"Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

//================ STREAM HANDLER =================
esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    char part_buf[64];

    res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if(res != ESP_OK) return res;

    while(true){

        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Capture fail");
            return ESP_FAIL;
        }

        // Boundary
        res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
        if(res != ESP_OK) goto exit;

        // Header
        size_t hlen = snprintf(part_buf, 64, STREAM_PART, fb->len);
        res = httpd_resp_send_chunk(req, part_buf, hlen);
        if(res != ESP_OK) goto exit;

        // Image
        res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        if(res != ESP_OK) goto exit;

        // Trả buffer ngay
        esp_camera_fb_return(fb);
        fb = NULL;

        // Giảm tải hệ thống
        delay(80);   // ~10 FPS
        yield();     // tránh watchdog reset
    }

exit:
    if (fb) esp_camera_fb_return(fb);
    Serial.println("Client disconnected");
    return res;
}

//================ ROOT PAGE =================
esp_err_t index_handler(httpd_req_t *req){
    const char* html =
    "<html><body style='text-align:center'>"
    "<h2>ESP32-S3 CAM</h2>"
    "<img src='/stream'>"
    "</body></html>";

    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

//================ SERVER =================
void startCameraServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port   = 32768;

    httpd_handle_t server = NULL;

    if(httpd_start(&server, &config) == ESP_OK){

        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler
        };
        httpd_register_uri_handler(server, &index_uri);

        httpd_uri_t stream_uri = {
            .uri = "/stream",
            .method = HTTP_GET,
            .handler = stream_handler
        };
        httpd_register_uri_handler(server, &stream_uri);
    }
}

//================ SETUP =================
void setup(){
    Serial.begin(115200);

    // PSRAM check
    if(psramFound()) Serial.println("PSRAM OK");
    else Serial.println("PSRAM FAIL");

    // CAMERA CONFIG
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;

    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;

    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;

    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;

    config.pin_pwdn  = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;

    config.xclk_freq_hz = 10000000;
    config.pixel_format = PIXFORMAT_JPEG;

    // 🔥 CẤU HÌNH ỔN ĐỊNH NHẤT
    config.frame_size   = FRAMESIZE_QQVGA; // giảm lag mạnh
    config.jpeg_quality = 15;              // giảm tải
    config.fb_count     = 1;               // tránh tràn RAM

    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode   = CAMERA_GRAB_LATEST;

    if(esp_camera_init(&config) != ESP_OK){
        Serial.println("Camera init FAILED");
        return;
    }

    Serial.println("Camera OK");

    // WIFI
    WiFi.begin(ssid, pass);
    WiFi.setSleep(false);

    while(WiFi.status() != WL_CONNECTED){
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi OK");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    startCameraServer();
}

//================ LOOP =================
void loop(){}
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "sdkconfig.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#endif

// Cấu trúc để gửi dữ liệu JPEG
typedef struct {
    httpd_req_t *req;
    size_t len;
} jpg_chunking_t;

// Định nghĩa boundary cho stream
#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;

// HTML đơn giản - chỉ stream, không có điều khiển
const char index_html[] = "<!DOCTYPE html>"
"<html>"
"<head>"
"    <title>ESP32-CAM Stream</title>"
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
"    <style>"
"        body { font-family: Arial; text-align: center; margin-top: 20px; background: #000; color: #fff; }"
"        img { width: 100%; max-width: 800px; border: 2px solid #333; background: #000; }"
"    </style>"
"</head>"
"<body>"
"    <h1>ESP32-S3 Camera Stream</h1>"
"    <img src=\"/stream\">"
"</body>"
"</html>";

// Hàm nén JPEG từng chunk
static size_t jpg_encode_stream(void *arg, size_t index, const void *data, size_t len) {
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if (!index) j->len = 0;
    if (httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK) return 0;
    j->len += len;
    return len;
}

// Handler stream video
static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) return res;
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            log_e("Camera capture failed");
            res = ESP_FAIL;
        } else {
            if (fb->format != PIXFORMAT_JPEG) {
                bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                esp_camera_fb_return(fb);
                fb = NULL;
                if (!jpeg_converted) {
                    log_e("JPEG compression failed");
                    res = ESP_FAIL;
                }
            } else {
                _jpg_buf_len = fb->len;
                _jpg_buf = fb->buf;
            }
        }
        
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if (res == ESP_OK) {
            size_t hlen = snprintf(NULL, 0, _STREAM_PART, _jpg_buf_len);
            char *part_buf = (char*)malloc(hlen + 1);
            if (part_buf) {
                snprintf(part_buf, hlen + 1, _STREAM_PART, _jpg_buf_len);
                res = httpd_resp_send_chunk(req, part_buf, hlen);
                free(part_buf);
            } else {
                res = ESP_FAIL;
            }
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        
        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if (_jpg_buf) {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        
        if (res != ESP_OK) break;
        
        vTaskDelay(1);
    }
    return res;
}

// Handler trang chủ
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, index_html, strlen(index_html));
}

// Khởi tạo server
void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.server_port = 80;

    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
        .user_ctx = NULL
    };

    httpd_uri_t stream_uri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = NULL
    };

    log_i("Starting web server on port: '%d'", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &stream_uri);
        log_i("Server started successfully");
    } else {
        log_e("Failed to start server");
    }
}
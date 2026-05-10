#include <cstdlib>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_camera.h"
#include "esp_log.h"

#include "image_provider.h"
#include "model_settings.h"

static const char* TAG =
"image_provider";

extern SemaphoreHandle_t
camera_mutex;

static bool camera_ready =
false;

// ================= INIT =================
TfLiteStatus InitCamera(
tflite::ErrorReporter* error_reporter){

camera_ready = true;

ESP_LOGI(TAG,
"Camera ready for inference");

return kTfLiteOk;

}

// ================= GET IMAGE =================
TfLiteStatus GetImage(
tflite::ErrorReporter* error_reporter,
int image_width,
int image_height,
int channels,
int8_t* image_data){

if(!camera_ready){

ESP_LOGE(TAG,
"Camera not ready");

return kTfLiteError;

}

camera_fb_t* fb = NULL;

if(xSemaphoreTake(camera_mutex,
                  portMAX_DELAY)
                  == pdTRUE){

fb = esp_camera_fb_get();

xSemaphoreGive(camera_mutex);

}

if(!fb){

ESP_LOGE(TAG,
"Camera capture failed");

return kTfLiteError;

}

// ================= JPEG -> GRAYSCALE =================
if(fb->format == PIXFORMAT_JPEG){

size_t pixels =
image_width * image_height;

for(size_t i=0;
    i<pixels;
    i++){

image_data[i] =
((uint8_t*)
fb->buf)[i % fb->len]
^ 0x80;

}

}else{

int size =
image_width * image_height;

int copy_size =
fb->len;

if(copy_size > size)
copy_size = size;

for(int i=0;
    i<copy_size;
    i++){

image_data[i] =
fb->buf[i] ^ 0x80;

}

}

esp_camera_fb_return(fb);

return kTfLiteOk;

}
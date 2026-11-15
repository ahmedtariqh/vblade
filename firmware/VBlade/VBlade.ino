// SPDX-License-Identifier: MIT
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <esp_camera.h>
#include <esp_timer.h>
#include "Settings.h"

enum class ScreenState { Starting, NoVideo, Live, Pattern, SystemFault };

struct CapturedFrame {
  camera_fb_t *buffer;
  TaskHandle_t owner;
};

// The library starts its initialization at 32 MHz unless begin() supplies a rate.
class BenchDisplay : public Adafruit_ST7789 {
 public:
  using Adafruit_ST7789::Adafruit_ST7789;

  void begin(uint32_t frequency) override {
    Adafruit_ST7789::begin(frequency == 0 ? TFT_SPI_HZ : frequency);
  }
};

BenchDisplay screen(TFT_CS, TFT_DC, TFT_RST);
QueueHandle_t frameQueue = nullptr;
ScreenState screenState = ScreenState::Starting;
int64_t displayedCaptureUs = 0;
uint32_t framesShown = 0;

int64_t captureTimeUs(const camera_fb_t &frame) {
  return static_cast<int64_t>(frame.timestamp.tv_sec) * 1'000'000 +
         frame.timestamp.tv_usec;
}

bool isFresh(int64_t capturedUs) {
  const int64_t elapsed = esp_timer_get_time() - capturedUs;
  return capturedUs > 0 && elapsed >= 0 && elapsed <= MAX_FRAME_AGE_US;
}

void drawBanner(const char *label, uint16_t color) {
  screen.fillRect(0, 0, VIDEO_WIDTH, BANNER_HEIGHT, ST77XX_BLACK);
  screen.setTextSize(1);
  screen.setTextColor(color);
  screen.setCursor(4, 2);
  screen.print(label);
}

void showStatus(ScreenState state, const char *message) {
  screen.fillScreen(ST77XX_BLACK);
  drawBanner("VBLADE | AIRWAY TRAINER ONLY", ST77XX_CYAN);
  screen.setTextColor(ST77XX_WHITE);
  screen.setTextSize(2);
  screen.setCursor(12, 92);
  screen.print(message);
  screenState = state;
}

esp_err_t startCamera() {
  camera_config_t config{};
  config.pin_pwdn = -1;
  config.pin_reset = -1;
  config.pin_xclk = 10;
  config.pin_sccb_sda = 40;
  config.pin_sccb_scl = 39;
  config.pin_d7 = 48;
  config.pin_d6 = 11;
  config.pin_d5 = 12;
  config.pin_d4 = 14;
  config.pin_d3 = 16;
  config.pin_d2 = 18;
  config.pin_d1 = 17;
  config.pin_d0 = 15;
  config.pin_vsync = 38;
  config.pin_href = 47;
  config.pin_pclk = 13;
  config.ledc_timer = LEDC_TIMER_0;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.xclk_freq_hz = 20'000'000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  const esp_err_t result = esp_camera_init(&config);
  if (result == ESP_OK) {
    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor != nullptr) {
      sensor->set_hmirror(sensor, CAMERA_HMIRROR);
      sensor->set_vflip(sensor, CAMERA_VFLIP);
    }
  }
  return result;
}

void runCamera(void *) {
  const TaskHandle_t owner = xTaskGetCurrentTaskHandle();
  for (;;) {
    const esp_err_t status = startCamera();
    if (status != ESP_OK) {
      Serial.printf("Camera init failed: %s\n", esp_err_to_name(status));
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    unsigned invalidFrames = 0;
    while (invalidFrames < 3) {
      camera_fb_t *frame = esp_camera_fb_get();
      const bool usable = frame != nullptr && frame->format == PIXFORMAT_RGB565 &&
                          frame->width == VIDEO_WIDTH && frame->height == VIDEO_HEIGHT &&
                          frame->len == VIDEO_WIDTH * VIDEO_HEIGHT * sizeof(uint16_t) &&
                          isFresh(captureTimeUs(*frame));
      if (usable) {
        const CapturedFrame packet{frame, owner};
        if (xQueueSend(frameQueue, &packet, 0) == pdPASS) {
          // The display owns the read access until its synchronous SPI write returns.
          ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }
        invalidFrames = 0;
      } else {
        ++invalidFrames;
      }
      if (frame != nullptr) {
        esp_camera_fb_return(frame);
      }
      vTaskDelay(1);
    }

    Serial.println("Camera stream invalid; reinitializing.");
    esp_camera_deinit();
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void drawFrame(camera_fb_t &frame) {
  if (screenState != ScreenState::Live) {
    drawBanner("LIVE | AIRWAY TRAINER ONLY", ST77XX_GREEN);
  }
  auto *pixels = reinterpret_cast<uint16_t *>(frame.buf) + VIDEO_WIDTH * BANNER_HEIGHT;
  screen.startWrite();
  screen.setAddrWindow(0, BANNER_HEIGHT, VIDEO_WIDTH, VIDEO_HEIGHT - BANNER_HEIGHT);
  screen.writePixels(pixels, VIDEO_WIDTH * (VIDEO_HEIGHT - BANNER_HEIGHT),
                     true, CAMERA_MS_BYTE_FIRST);
  screen.endWrite();

  // Advances only when the display receives a recent frame from the camera.
  if ((framesShown++ % 4) == 0) {
    screen.fillRect(308, 3, 7, 6, (framesShown & 4) ? ST77XX_GREEN : ST77XX_BLACK);
  }
  displayedCaptureUs = captureTimeUs(frame);
  screenState = ScreenState::Live;
}

void showTestPattern() {
  const uint16_t colors[] = {ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE,
                              ST77XX_WHITE, ST77XX_BLACK};
  constexpr int stripeWidth = VIDEO_WIDTH / 5;
  for (int i = 0; i < 5; ++i) {
    screen.fillRect(i * stripeWidth, BANNER_HEIGHT, stripeWidth,
                    VIDEO_HEIGHT - BANNER_HEIGHT, colors[i]);
  }
  drawBanner("LCD TEST | AIRWAY TRAINER ONLY", ST77XX_CYAN);
  screenState = ScreenState::Pattern;
}

void setup() {
  Serial.begin(115200);
  SPI.begin(TFT_SCK, -1, TFT_MOSI, -1);
  screen.init(VIDEO_HEIGHT, VIDEO_WIDTH);
  screen.setRotation(TFT_ROTATION);
  screen.setTextWrap(false);

  if (DISPLAY_TEST_PATTERN) {
    showTestPattern();
    return;
  }

  showStatus(ScreenState::Starting, "STARTING CAMERA");
  if (!psramFound()) {
    showStatus(ScreenState::SystemFault, "CHECK PSRAM SETTING");
    return;
  }

  frameQueue = xQueueCreate(1, sizeof(CapturedFrame));
  if (frameQueue == nullptr ||
      xTaskCreatePinnedToCore(runCamera, "camera", 6144, nullptr, 2, nullptr, 0) != pdPASS) {
    showStatus(ScreenState::SystemFault, "CAMERA TASK ERROR");
    return;
  }
  displayedCaptureUs = esp_timer_get_time();
}

void loop() {
  if (screenState == ScreenState::Pattern || screenState == ScreenState::SystemFault) {
    delay(50);
    return;
  }

  CapturedFrame packet{};
  if (xQueueReceive(frameQueue, &packet, pdMS_TO_TICKS(20)) == pdPASS) {
    if (isFresh(captureTimeUs(*packet.buffer))) {
      drawFrame(*packet.buffer);
    }
    xTaskNotifyGive(packet.owner);
  }

  if (screenState != ScreenState::NoVideo && !isFresh(displayedCaptureUs)) {
    showStatus(ScreenState::NoVideo, "NO VIDEO");
  }
}

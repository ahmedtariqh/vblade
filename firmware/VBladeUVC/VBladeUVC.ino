// SPDX-License-Identifier: MIT
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_ST7789.h>
#include <USB_STREAM.h>
#include <JPEGDEC.h>
#include <esp_timer.h>
#include <atomic>
#include "Settings.h"

static_assert((CAMERA_WIDTH == WIDTH && CAMERA_HEIGHT == HEIGHT) ||
              (CAMERA_WIDTH == 2 * WIDTH && CAMERA_HEIGHT == 2 * HEIGHT),
              "Select a supported MJPEG size");
static_assert(CAMERA_FPS > 0, "Camera FPS must be positive");

class BenchDisplay : public Adafruit_ST7789 {
 public:
  using Adafruit_ST7789::Adafruit_ST7789;
  void begin(uint32_t frequency) override {
    Adafruit_ST7789::begin(frequency ? frequency : TFT_SPI_HZ);
  }
};

struct FrameStamp {
  size_t bytes = 0;
  int64_t receivedUs = 0;
  uint32_t connection = 0;
};

enum class Screen { Starting, NoVideo, Live, Error };

BenchDisplay screen(TFT_CS, TFT_DC, TFT_RST);
JPEGDEC decoder;
SemaphoreHandle_t frameLock;
uint8_t *pendingJpeg;
uint8_t *decodeJpeg;
uint16_t *pixels;
FrameStamp pending;
FrameStamp shown;
Screen screenState = Screen::Starting;
// Odd generations are connected. A reconnect invalidates frames from the old stream.
std::atomic<uint32_t> connection{0};

void showMessage(Screen state, const char *message) {
  screen.fillScreen(ST77XX_BLACK);
  screen.setCursor(4, 2);
  screen.setTextSize(1);
  screen.setTextColor(ST77XX_CYAN);
  screen.print("VBLADE UVC | AIRWAY TRAINER ONLY");
  screen.setCursor(12, 92);
  screen.setTextSize(2);
  screen.setTextColor(ST77XX_WHITE);
  screen.print(message);
  screenState = state;
}

void onUsbState(usb_stream_state_t state, void *) {
  const uint32_t prior = connection.load();
  const bool nowConnected = state == STREAM_CONNECTED;
  if (nowConnected != bool(prior & 1)) {
    connection.store(prior + 1);
  }
}

void onUsbFrame(uvc_frame_t *frame, void *) {
  const uint32_t stream = connection.load();
  if (!(stream & 1) || !frame || !frame->data ||
      frame->frame_format != UVC_FRAME_FORMAT_MJPEG ||
      frame->width != CAMERA_WIDTH || frame->height != CAMERA_HEIGHT ||
      frame->data_bytes == 0 || frame->data_bytes > FRAME_BYTES) return;
  if (xSemaphoreTake(frameLock, 0) != pdTRUE) return;
  memcpy(pendingJpeg, frame->data, frame->data_bytes);
  pending = FrameStamp{frame->data_bytes, esp_timer_get_time(), stream};
  xSemaphoreGive(frameLock);
}

FrameStamp takeFrame() {
  if (xSemaphoreTake(frameLock, 0) != pdTRUE) return {};
  const FrameStamp frame = pending;
  if (frame.bytes) {
    std::swap(pendingJpeg, decodeJpeg);
    pending = {};
  }
  xSemaphoreGive(frameLock);
  return frame;
}

bool isCurrent(const FrameStamp &frame) {
  return frame.bytes && frame.connection == connection.load() &&
         esp_timer_get_time() - frame.receivedUs <= FRAME_TIMEOUT_US;
}

int storeJpegBlock(JPEGDRAW *block) {
  const int rows = min(block->iHeight, HEIGHT - block->y);
  const int cols = min(block->iWidthUsed, WIDTH - block->x);
  if (block->x < 0 || block->y < 0 || rows <= 0 || cols <= 0) return 1;
  for (int y = 0; y < rows; ++y) {
    uint16_t *dst = pixels + (block->y + y) * WIDTH + block->x;
    const uint16_t *src = block->pPixels + y * block->iWidth;
    memcpy(dst, src, cols * sizeof(*dst));
  }
  return 1;
}

bool decodeFrame(size_t bytes) {
  if (!decoder.openRAM(decodeJpeg, bytes, storeJpegBlock)) return false;
  bool valid = decoder.getWidth() == CAMERA_WIDTH && decoder.getHeight() == CAMERA_HEIGHT;
  if (valid) {
    decoder.setPixelType(RGB565_LITTLE_ENDIAN);
    const int scale = CAMERA_WIDTH == WIDTH ? 0 : JPEG_SCALE_HALF;
    valid = decoder.decode(0, 0, scale);
  }
  decoder.close();
  if (valid && ROTATE_CAMERA_180) {
    for (int i = 0; i < WIDTH * HEIGHT / 2; ++i) {
      std::swap(pixels[i], pixels[WIDTH * HEIGHT - 1 - i]);
    }
  }
  return valid;
}

void showFrame() {
  if (screenState != Screen::Live) {
    screen.fillRect(0, 0, WIDTH, BANNER_HEIGHT, ST77XX_BLACK);
    screen.setTextSize(1);
    screen.setTextColor(ST77XX_GREEN);
    screen.setCursor(4, 2);
    screen.print("LIVE | AIRWAY TRAINER ONLY");
    screenState = Screen::Live;
  }
  screen.startWrite();
  screen.setAddrWindow(0, BANNER_HEIGHT, WIDTH, HEIGHT - BANNER_HEIGHT);
  screen.writePixels(pixels + WIDTH * BANNER_HEIGHT,
                     WIDTH * (HEIGHT - BANNER_HEIGHT), true, false);
  screen.endWrite();
}

bool startUsb() {
  frameLock = xSemaphoreCreateMutex();
  pendingJpeg = static_cast<uint8_t *>(ps_malloc(FRAME_BYTES));
  decodeJpeg = static_cast<uint8_t *>(ps_malloc(FRAME_BYTES));
  pixels = static_cast<uint16_t *>(ps_calloc(WIDTH * HEIGHT, sizeof(uint16_t)));
  uvc_config_t camera{};
  camera.frame_width = CAMERA_WIDTH;
  camera.frame_height = CAMERA_HEIGHT;
  camera.frame_interval = FPS2INTERVAL(CAMERA_FPS);
  camera.xfer_buffer_size = FRAME_BYTES;
  camera.xfer_buffer_a = static_cast<uint8_t *>(ps_malloc(FRAME_BYTES));
  camera.xfer_buffer_b = static_cast<uint8_t *>(ps_malloc(FRAME_BYTES));
  camera.frame_buffer_size = FRAME_BYTES;
  camera.frame_buffer = static_cast<uint8_t *>(ps_malloc(FRAME_BYTES));
  camera.frame_cb = onUsbFrame;

  if (!frameLock || !pendingJpeg || !decodeJpeg || !pixels ||
      !camera.xfer_buffer_a || !camera.xfer_buffer_b || !camera.frame_buffer) return false;
  esp_err_t error = uvc_streaming_config(&camera);
  if (error == ESP_OK) error = usb_streaming_state_register(onUsbState, nullptr);
  if (error == ESP_OK) error = usb_streaming_start();
  if (error != ESP_OK) Serial.printf("USB stream setup: %s\n", esp_err_to_name(error));
  return error == ESP_OK;
}

void setup() {
  Serial.begin(115200);
  SPI.begin(TFT_SCK, -1, TFT_MOSI, -1);
  screen.init(HEIGHT, WIDTH);
  screen.setRotation(1);
  screen.setTextWrap(false);
  showMessage(Screen::Starting, "CONNECT USB SCOPE");
  if (!psramFound() || !startUsb()) showMessage(Screen::Error, "USB / PSRAM ERROR");
}

void loop() {
  if (screenState == Screen::Error) {
    delay(50);
    return;
  }
  const FrameStamp frame = takeFrame();
  if (isCurrent(frame) && decodeFrame(frame.bytes) && isCurrent(frame)) {
    showFrame();
    shown = frame;
  }
  if (screenState == Screen::Live && !isCurrent(shown)) {
    showMessage(Screen::NoVideo, "NO VIDEO");
  } else if (screenState == Screen::Starting && esp_timer_get_time() > 3'000'000) {
    showMessage(Screen::NoVideo, "NO VIDEO");
  }
  delay(2);
}

// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>

// Seeed Studio XIAO ESP32S3 Sense with its stock camera expansion board.
constexpr int TFT_SCK = 7;    // D8
constexpr int TFT_MOSI = 9;   // D10
constexpr int TFT_CS = 4;     // D3
constexpr int TFT_DC = 5;     // D4
constexpr int TFT_RST = 6;    // D5
constexpr uint32_t TFT_SPI_HZ = 12'000'000;
constexpr uint8_t TFT_ROTATION = 1;
static_assert(TFT_ROTATION == 1 || TFT_ROTATION == 3, "Use a landscape LCD rotation");
constexpr bool CAMERA_HMIRROR = false;
constexpr bool CAMERA_VFLIP = false;
constexpr bool CAMERA_MS_BYTE_FIRST = true;
constexpr bool DISPLAY_TEST_PATTERN = false;

constexpr uint16_t VIDEO_WIDTH = 320;
constexpr uint16_t VIDEO_HEIGHT = 240;
constexpr uint16_t BANNER_HEIGHT = 12;
constexpr int64_t MAX_FRAME_AGE_US = 400'000;

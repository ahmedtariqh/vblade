// SPDX-License-Identifier: MIT
#pragma once

// ESP32-S3-DevKitC-1 N8R8 / ST7789 240x320 SPI.
// Native USB host uses GPIO19 (D-) and GPIO20 (D+).
constexpr int TFT_SCK = 12;
constexpr int TFT_MOSI = 11;
constexpr int TFT_CS = 10;
constexpr int TFT_DC = 9;
constexpr int TFT_RST = 8;
constexpr unsigned TFT_SPI_HZ = 20'000'000;

constexpr int WIDTH = 320;
constexpr int HEIGHT = 240;
constexpr int BANNER_HEIGHT = 12;
constexpr int CAMERA_WIDTH = 640; // Either 640x480 or 320x240 MJPEG.
constexpr int CAMERA_HEIGHT = 480;
constexpr unsigned CAMERA_FPS = 15;
constexpr unsigned FRAME_BYTES = 64 * 1024;
constexpr long long FRAME_TIMEOUT_US = 400'000;
constexpr bool ROTATE_CAMERA_180 = false;

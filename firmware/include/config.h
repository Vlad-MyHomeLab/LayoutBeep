#pragma once

// Waveshare RP2040-Zero onboard WS2812B is connected to GPIO16.
#define LB_RGB_PIN 16
#define LB_BUZZER_PIN 15

// Defaults used until the Windows host sends its saved configuration.
#define LB_ENG_R 0
#define LB_ENG_G 0
#define LB_ENG_B 28
#define LB_RUS_R 28
#define LB_RUS_G 0
#define LB_RUS_B 0

#define LB_OFFLINE_R 10
#define LB_OFFLINE_G 4
#define LB_OFFLINE_B 0

#define LB_FLASH_R 90
#define LB_FLASH_G 90
#define LB_FLASH_B 90
#define LB_FLASH_MS 80u

#define LB_ENG_TONE_HZ 1450u
#define LB_ENG_TONE_MS 75u
#define LB_RUS_TONE_HZ 850u
#define LB_RUS_TONE_MS 75u

#define LB_HOST_TIMEOUT_MS 4500u

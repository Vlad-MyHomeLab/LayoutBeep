#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

enum class Layout : uint8_t {
    Unknown = 0,
    English,
    Russian
};

struct CueConfig {
    uint32_t engHz;
    uint32_t engMs;
    uint8_t engR, engG, engB;
    uint32_t rusHz;
    uint32_t rusMs;
    uint8_t rusR, rusG, rusB;
    uint32_t flashMs;
    uint8_t flashR, flashG, flashB;
};

static Adafruit_NeoPixel g_pixel(1, LB_RGB_PIN, NEO_GRB + NEO_KHZ800);
static Layout g_layout = Layout::Unknown;
static bool g_hostOnline = false;
static bool g_flashActive = false;
static bool g_toneActive = false;
static bool g_nightActive = false;
static uint8_t g_dayVolumePct = 100;
static uint8_t g_nightVolumePct = 25;
static uint8_t g_nightBrightnessPct = 20;
static uint32_t g_flashUntil = 0;
static uint32_t g_toneUntil = 0;
static uint32_t g_lastHostPacket = 0;
static char g_rxLine[192];
static size_t g_rxLen = 0;
static CueConfig g_cfg = {
    LB_ENG_TONE_HZ, LB_ENG_TONE_MS,
    LB_ENG_R, LB_ENG_G, LB_ENG_B,
    LB_RUS_TONE_HZ, LB_RUS_TONE_MS,
    LB_RUS_R, LB_RUS_G, LB_RUS_B,
    LB_FLASH_MS, LB_FLASH_R, LB_FLASH_G, LB_FLASH_B
};

static uint8_t scaleChannel(uint8_t value) {
    const uint32_t pct = g_nightActive ? g_nightBrightnessPct : 100u;
    return static_cast<uint8_t>((static_cast<uint32_t>(value) * pct + 50u) / 100u);
}

static void setPixelRaw(uint8_t r, uint8_t g, uint8_t b) {
    g_pixel.setPixelColor(0, g_pixel.Color(r, g, b));
    g_pixel.show();
}

static void setPixel(uint8_t r, uint8_t g, uint8_t b) {
    g_pixel.setPixelColor(0, g_pixel.Color(scaleChannel(r), scaleChannel(g), scaleChannel(b)));
    g_pixel.show();
}

static void showSteadyState() {
    if (!g_hostOnline) {
        // Offline orange is a status indicator, not part of the day/night profile.
        setPixelRaw(LB_OFFLINE_R, LB_OFFLINE_G, LB_OFFLINE_B);
        return;
    }

    switch (g_layout) {
        case Layout::English:
            setPixel(g_cfg.engR, g_cfg.engG, g_cfg.engB);
            break;
        case Layout::Russian:
            setPixel(g_cfg.rusR, g_cfg.rusG, g_cfg.rusB);
            break;
        default:
            setPixelRaw(LB_OFFLINE_R, LB_OFFLINE_G, LB_OFFLINE_B);
            break;
    }
}

static void markHostAlive() {
    const bool wasOffline = !g_hostOnline;
    g_hostOnline = true;
    g_lastHostPacket = millis();
    if (wasOffline && !g_flashActive) {
        showSteadyState();
    }
}

static void stopTone() {
    analogWrite(LB_BUZZER_PIN, 0);
    g_toneActive = false;
}

static void startTone(uint32_t hz, uint32_t durationMs) {
    const uint32_t volumePct = g_nightActive ? g_nightVolumePct : g_dayVolumePct;
    if (volumePct == 0u || durationMs == 0u) {
        stopTone();
        return;
    }

    // 50% duty (128/255) is the loudest square wave. Night volume scales
    // the duty from silence to that 50% point. This is a relative, not dB,
    // volume control and works well with a passive piezo on GPIO15.
    uint32_t duty = (128u * volumePct + 99u) / 100u;
    if (duty < 1u) duty = 1u;
    if (duty > 128u) duty = 128u;
    analogWriteFreq(hz);
    analogWrite(LB_BUZZER_PIN, static_cast<int>(duty));
    g_toneActive = true;
    g_toneUntil = millis() + durationMs;
}

static void startCue(Layout target, bool updateLayout) {
    if (updateLayout) {
        g_layout = target;
    }

    setPixel(g_cfg.flashR, g_cfg.flashG, g_cfg.flashB);
    g_flashActive = true;
    g_flashUntil = millis() + g_cfg.flashMs;

    if (target == Layout::English) {
        startTone(g_cfg.engHz, g_cfg.engMs);
    } else if (target == Layout::Russian) {
        startTone(g_cfg.rusHz, g_cfg.rusMs);
    }
}

static void setLayout(Layout target, bool switched) {
    markHostAlive();
    g_layout = target;
    if (switched) {
        startCue(target, false);
    } else if (!g_flashActive) {
        showSteadyState();
    }
}

static bool parseUnsignedList(const char *p, uint32_t *v, size_t count) {
    if (!p || !v || !count) return false;
    for (size_t i = 0; i < count; ++i) {
        if (*p < '0' || *p > '9') return false;
        uint32_t value = 0;
        while (*p >= '0' && *p <= '9') {
            value = value * 10u + static_cast<uint32_t>(*p - '0');
            if (value > 100000u) return false;
            ++p;
        }
        v[i] = value;
        if (i + 1u < count) {
            if (*p != ',') return false;
            ++p;
        } else if (*p != '\0') {
            return false;
        }
    }
    return true;
}

static bool parseConfigLine(const char *line) {
    // C,engHz,engMs,engR,engG,engB,rusHz,rusMs,rusR,rusG,rusB,flashMs,flashR,flashG,flashB
    if (!line || line[0] != 'C' || line[1] != ',') return false;
    uint32_t v[14];
    if (!parseUnsignedList(line + 2, v, 14u)) return false;

    if (v[0] < 100u || v[0] > 10000u || v[5] < 100u || v[5] > 10000u) return false;
    if (v[1] < 10u || v[1] > 2000u || v[6] < 10u || v[6] > 2000u) return false;
    if (v[10] < 10u || v[10] > 2000u) return false;
    static const uint8_t colorIdx[] = {2u,3u,4u,7u,8u,9u,11u,12u,13u};
    for (size_t j = 0; j < sizeof(colorIdx); ++j) {
        if (v[colorIdx[j]] > 255u) return false;
    }

    g_cfg.engHz = v[0];
    g_cfg.engMs = v[1];
    g_cfg.engR = static_cast<uint8_t>(v[2]);
    g_cfg.engG = static_cast<uint8_t>(v[3]);
    g_cfg.engB = static_cast<uint8_t>(v[4]);
    g_cfg.rusHz = v[5];
    g_cfg.rusMs = v[6];
    g_cfg.rusR = static_cast<uint8_t>(v[7]);
    g_cfg.rusG = static_cast<uint8_t>(v[8]);
    g_cfg.rusB = static_cast<uint8_t>(v[9]);
    g_cfg.flashMs = v[10];
    g_cfg.flashR = static_cast<uint8_t>(v[11]);
    g_cfg.flashG = static_cast<uint8_t>(v[12]);
    g_cfg.flashB = static_cast<uint8_t>(v[13]);

    markHostAlive();
    if (!g_flashActive) showSteadyState();
    return true;
}

static bool parseNightLine(const char *line) {
    // N,active,dayVolumePct,nightVolumePct,nightBrightnessPct
    if (!line || line[0] != 'N' || line[1] != ',') return false;
    uint32_t v[4];
    if (!parseUnsignedList(line + 2, v, 4u)) return false;
    if (v[0] > 1u || v[1] > 100u || v[2] > 100u || v[3] > 100u) return false;

    g_nightActive = (v[0] != 0u);
    g_dayVolumePct = static_cast<uint8_t>(v[1]);
    g_nightVolumePct = static_cast<uint8_t>(v[2]);
    g_nightBrightnessPct = static_cast<uint8_t>(v[3]);
    markHostAlive();

    // Apply mute immediately if the active profile is muted. Other volume
    // changes take effect on the next short cue.
    const uint8_t activeVolume = g_nightActive ? g_nightVolumePct : g_dayVolumePct;
    if (activeVolume == 0u && g_toneActive) stopTone();
    if (!g_flashActive) showSteadyState();
    else setPixel(g_cfg.flashR, g_cfg.flashG, g_cfg.flashB);
    return true;
}

static void processCommand(const char *line) {
    if (!line || !line[0]) return;

    if (line[0] == '?' && line[1] == '\0') {
        markHostAlive();
        Serial.println("LB5 RP2040-ZERO 2.7");
        return;
    }

    if (line[0] == 'K' && line[1] == '\0') {
        markHostAlive();
        return;
    }

    if (line[0] == 'C' && line[1] == ',') {
        parseConfigLine(line);
        return;
    }

    if (line[0] == 'N' && line[1] == ',') {
        parseNightLine(line);
        return;
    }

    if ((line[0] == 'E' || line[0] == 'R') &&
        (line[1] == '0' || line[1] == '1') && line[2] == '\0') {
        const Layout target = (line[0] == 'E') ? Layout::English : Layout::Russian;
        setLayout(target, line[1] == '1');
        return;
    }

    if (line[0] == 'T' && (line[1] == 'E' || line[1] == 'R') && line[2] == '\0') {
        markHostAlive();
        const Layout target = (line[1] == 'E') ? Layout::English : Layout::Russian;
        startCue(target, false); // test only: do not alter steady layout
        return;
    }
}

static void serviceSerial() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r') continue;
        if (c == '\n') {
            g_rxLine[g_rxLen] = '\0';
            processCommand(g_rxLine);
            g_rxLen = 0;
            continue;
        }

        if (g_rxLen + 1 < sizeof(g_rxLine)) {
            g_rxLine[g_rxLen++] = c;
        } else {
            g_rxLen = 0;
        }
    }
}

static void serviceEffects() {
    const uint32_t now = millis();

    if (g_toneActive && static_cast<int32_t>(now - g_toneUntil) >= 0) {
        stopTone();
    }

    if (g_flashActive && static_cast<int32_t>(now - g_flashUntil) >= 0) {
        g_flashActive = false;
        showSteadyState();
    }

    if (g_hostOnline && static_cast<uint32_t>(now - g_lastHostPacket) > LB_HOST_TIMEOUT_MS) {
        g_hostOnline = false;
        if (!g_flashActive) showSteadyState();
    }
}

void setup() {
    pinMode(LB_BUZZER_PIN, OUTPUT);
    analogWriteRange(255);
    analogWrite(LB_BUZZER_PIN, 0);

    g_pixel.begin();
    g_pixel.clear();
    g_pixel.show();
    showSteadyState();

    Serial.begin(115200);
    Serial.ignoreFlowControl(true);
}

void loop() {
    serviceSerial();
    serviceEffects();
    delay(1);
}

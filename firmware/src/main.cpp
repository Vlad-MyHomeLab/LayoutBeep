#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

static constexpr uint8_t kMaxProfiles = 6;
static constexpr uint8_t kNoProfile = 0xff;

struct ProfileConfig {
    uint32_t hz;
    uint32_t ms;
    uint8_t r, g, b;
};

struct GlobalConfig {
    uint32_t flashMs;
    uint8_t flashR, flashG, flashB;
};

static Adafruit_NeoPixel g_pixel(1, LB_RGB_PIN, NEO_GRB + NEO_KHZ800);
static ProfileConfig g_profiles[kMaxProfiles] = {
    {LB_ENG_TONE_HZ, LB_ENG_TONE_MS, LB_ENG_R, LB_ENG_G, LB_ENG_B},
    {LB_RUS_TONE_HZ, LB_RUS_TONE_MS, LB_RUS_R, LB_RUS_G, LB_RUS_B},
    {1150u, 75u, 0u, 28u, 0u},
    {1750u, 75u, 28u, 18u, 0u},
    {2050u, 75u, 20u, 0u, 28u},
    {2550u, 75u, 0u, 24u, 28u}
};
static GlobalConfig g_cfg = {LB_FLASH_MS, LB_FLASH_R, LB_FLASH_G, LB_FLASH_B};

static uint8_t g_profileCount = 2;
static uint8_t g_currentProfile = kNoProfile;
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
        // Offline orange is a status indicator and is never dimmed by night mode.
        setPixelRaw(LB_OFFLINE_R, LB_OFFLINE_G, LB_OFFLINE_B);
        return;
    }
    if (g_currentProfile < g_profileCount && g_currentProfile < kMaxProfiles) {
        const ProfileConfig &p = g_profiles[g_currentProfile];
        setPixel(p.r, p.g, p.b);
    } else {
        setPixelRaw(LB_OFFLINE_R, LB_OFFLINE_G, LB_OFFLINE_B);
    }
}

static void markHostAlive() {
    const bool wasOffline = !g_hostOnline;
    g_hostOnline = true;
    g_lastHostPacket = millis();
    if (wasOffline && !g_flashActive) showSteadyState();
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
    uint32_t duty = (128u * volumePct + 99u) / 100u;
    if (duty < 1u) duty = 1u;
    if (duty > 128u) duty = 128u;
    analogWriteFreq(hz);
    analogWrite(LB_BUZZER_PIN, static_cast<int>(duty));
    g_toneActive = true;
    g_toneUntil = millis() + durationMs;
}

static void startCue(uint8_t profile, bool updateProfile) {
    if (profile >= g_profileCount || profile >= kMaxProfiles) return;
    if (updateProfile) g_currentProfile = profile;

    setPixel(g_cfg.flashR, g_cfg.flashG, g_cfg.flashB);
    g_flashActive = true;
    g_flashUntil = millis() + g_cfg.flashMs;

    const ProfileConfig &p = g_profiles[profile];
    startTone(p.hz, p.ms);
}

static void setProfile(uint8_t profile, bool switched) {
    if (profile >= g_profileCount || profile >= kMaxProfiles) return;
    markHostAlive();
    g_currentProfile = profile;
    if (switched) startCue(profile, false);
    else if (!g_flashActive) showSteadyState();
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

static bool parseGlobalLine(const char *line) {
    // G,flashMs,flashR,flashG,flashB
    uint32_t v[4];
    if (!line || line[0] != 'G' || line[1] != ',') return false;
    if (!parseUnsignedList(line + 2, v, 4u)) return false;
    if (v[0] < 10u || v[0] > 2000u || v[1] > 255u || v[2] > 255u || v[3] > 255u) return false;
    g_cfg.flashMs = v[0];
    g_cfg.flashR = static_cast<uint8_t>(v[1]);
    g_cfg.flashG = static_cast<uint8_t>(v[2]);
    g_cfg.flashB = static_cast<uint8_t>(v[3]);
    markHostAlive();
    return true;
}

static bool parseProfileLine(const char *line) {
    // P,index,hz,ms,r,g,b
    uint32_t v[6];
    if (!line || line[0] != 'P' || line[1] != ',') return false;
    if (!parseUnsignedList(line + 2, v, 6u)) return false;
    if (v[0] >= kMaxProfiles || v[1] < 100u || v[1] > 10000u || v[2] < 10u || v[2] > 2000u) return false;
    if (v[3] > 255u || v[4] > 255u || v[5] > 255u) return false;
    const uint8_t index = static_cast<uint8_t>(v[0]);
    g_profiles[index].hz = v[1];
    g_profiles[index].ms = v[2];
    g_profiles[index].r = static_cast<uint8_t>(v[3]);
    g_profiles[index].g = static_cast<uint8_t>(v[4]);
    g_profiles[index].b = static_cast<uint8_t>(v[5]);
    if (index + 1u > g_profileCount) g_profileCount = index + 1u;
    markHostAlive();
    if (!g_flashActive && g_currentProfile == index) showSteadyState();
    return true;
}

static bool parseLayoutLine(const char *line) {
    // L,index,switched
    uint32_t v[2];
    if (!line || line[0] != 'L' || line[1] != ',') return false;
    if (!parseUnsignedList(line + 2, v, 2u)) return false;
    if (v[0] >= kMaxProfiles || v[0] >= g_profileCount || v[1] > 1u) return false;
    setProfile(static_cast<uint8_t>(v[0]), v[1] != 0u);
    return true;
}

static bool parseTestLine(const char *line) {
    // T,index
    uint32_t v[1];
    if (!line || line[0] != 'T' || line[1] != ',') return false;
    if (!parseUnsignedList(line + 2, v, 1u)) return false;
    if (v[0] >= kMaxProfiles || v[0] >= g_profileCount) return false;
    markHostAlive();
    startCue(static_cast<uint8_t>(v[0]), false);
    return true;
}

static bool parseNightLine(const char *line) {
    // N,active,dayVolumePct,nightVolumePct,nightBrightnessPct
    uint32_t v[4];
    if (!line || line[0] != 'N' || line[1] != ',') return false;
    if (!parseUnsignedList(line + 2, v, 4u)) return false;
    if (v[0] > 1u || v[1] > 100u || v[2] > 100u || v[3] > 100u) return false;
    g_nightActive = (v[0] != 0u);
    g_dayVolumePct = static_cast<uint8_t>(v[1]);
    g_nightVolumePct = static_cast<uint8_t>(v[2]);
    g_nightBrightnessPct = static_cast<uint8_t>(v[3]);
    markHostAlive();
    const uint8_t activeVolume = g_nightActive ? g_nightVolumePct : g_dayVolumePct;
    if (activeVolume == 0u && g_toneActive) stopTone();
    if (!g_flashActive) showSteadyState();
    else setPixel(g_cfg.flashR, g_cfg.flashG, g_cfg.flashB);
    return true;
}

static bool parseLegacyConfigLine(const char *line) {
    // C,engHz,engMs,engR,engG,engB,rusHz,rusMs,rusR,rusG,rusB,flashMs,flashR,flashG,flashB
    uint32_t v[14];
    if (!line || line[0] != 'C' || line[1] != ',') return false;
    if (!parseUnsignedList(line + 2, v, 14u)) return false;
    if (v[0] < 100u || v[0] > 10000u || v[5] < 100u || v[5] > 10000u) return false;
    if (v[1] < 10u || v[1] > 2000u || v[6] < 10u || v[6] > 2000u || v[10] < 10u || v[10] > 2000u) return false;
    static const uint8_t colorIdx[] = {2u,3u,4u,7u,8u,9u,11u,12u,13u};
    for (size_t j = 0; j < sizeof(colorIdx); ++j) if (v[colorIdx[j]] > 255u) return false;

    g_profiles[0] = {v[0], v[1], static_cast<uint8_t>(v[2]), static_cast<uint8_t>(v[3]), static_cast<uint8_t>(v[4])};
    g_profiles[1] = {v[5], v[6], static_cast<uint8_t>(v[7]), static_cast<uint8_t>(v[8]), static_cast<uint8_t>(v[9])};
    g_profileCount = g_profileCount < 2u ? 2u : g_profileCount;
    g_cfg.flashMs = v[10];
    g_cfg.flashR = static_cast<uint8_t>(v[11]);
    g_cfg.flashG = static_cast<uint8_t>(v[12]);
    g_cfg.flashB = static_cast<uint8_t>(v[13]);
    markHostAlive();
    if (!g_flashActive) showSteadyState();
    return true;
}

static void processCommand(const char *line) {
    if (!line || !line[0]) return;

    if (line[0] == '?' && line[1] == '\0') {
        markHostAlive();
        Serial.println("LB6 RP2040-ZERO 2.8");
        return;
    }
    if (line[0] == 'K' && line[1] == '\0') { markHostAlive(); return; }
    if (line[0] == 'G' && line[1] == ',') { parseGlobalLine(line); return; }
    if (line[0] == 'P' && line[1] == ',') { parseProfileLine(line); return; }
    if (line[0] == 'L' && line[1] == ',') { parseLayoutLine(line); return; }
    if (line[0] == 'T' && line[1] == ',') { parseTestLine(line); return; }
    if (line[0] == 'N' && line[1] == ',') { parseNightLine(line); return; }

    // LB5 backwards compatibility.
    if (line[0] == 'C' && line[1] == ',') { parseLegacyConfigLine(line); return; }
    if ((line[0] == 'E' || line[0] == 'R') &&
        (line[1] == '0' || line[1] == '1') && line[2] == '\0') {
        setProfile(line[0] == 'E' ? 0u : 1u, line[1] == '1');
        return;
    }
    if (line[0] == 'T' && (line[1] == 'E' || line[1] == 'R') && line[2] == '\0') {
        markHostAlive();
        startCue(line[1] == 'E' ? 0u : 1u, false);
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
        if (g_rxLen + 1 < sizeof(g_rxLine)) g_rxLine[g_rxLen++] = c;
        else g_rxLen = 0;
    }
}

static void serviceEffects() {
    const uint32_t now = millis();
    if (g_toneActive && static_cast<int32_t>(now - g_toneUntil) >= 0) stopTone();
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

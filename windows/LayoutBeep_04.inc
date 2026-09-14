__declspec(dllimport) HANDLE WINAPI SetupDiGetClassDevsW(const GUID*, LPCWSTR, HWND, DWORD);
__declspec(dllimport) BOOL WINAPI SetupDiEnumDeviceInfo(HANDLE, DWORD, SP_DEVINFO_DATA*);
__declspec(dllimport) BOOL WINAPI SetupDiGetDeviceRegistryPropertyW(HANDLE, SP_DEVINFO_DATA*, DWORD, LPDWORD, LPBYTE, DWORD, LPDWORD);
__declspec(dllimport) HKEY WINAPI SetupDiOpenDevRegKey(HANDLE, SP_DEVINFO_DATA*, DWORD, DWORD, DWORD, DWORD);
__declspec(dllimport) BOOL WINAPI SetupDiDestroyDeviceInfoList(HANDLE);

typedef struct {
    DWORD engHz, engMs;
    BYTE engR, engG, engB;
    DWORD rusHz, rusMs;
    BYTE rusR, rusG, rusB;
    DWORD flashMs;
    BYTE flashR, flashG, flashB;
    BOOL nightEnabled;
    DWORD nightStartMin;
    DWORD nightEndMin;
    DWORD dayVolumePct;
    DWORD nightVolumePct;
    DWORD nightBrightnessPct;
} BOARD_CONFIG;

#define MAX_LAYOUT_PROFILES 6u
#define INVALID_PROFILE_INDEX 0xffffffffu

typedef struct {
    DWORD id;
    HANDLE hkl;
    WCHAR section[32];
    WCHAR name[96];
    DWORD hz;
    DWORD ms;
    BYTE r, g, b;
} LAYOUT_PROFILE;

static HWND g_hwnd;
static HWND g_settingsHwnd;
static HINSTANCE g_instance;
static HICON g_icon;
static HFONT g_uiFont;
static HFONT g_titleFont;
static HFONT g_sectionFont;
static HFONT g_smallFont;
static UINT g_taskbarCreated;
static BOOL g_comInitialized;
static HWND g_trayIndicatorButton;
static WORD g_lastTrayPrimary;
static WORD g_trayCandidatePrimary;
static UINT g_trayCandidateCount;
static DWORD g_trayReadFailures;
static DWORD g_trayChanges;
static WCHAR g_lastTrayText[256];
static WORD g_lastPrimary;
static BOOL g_pcFallbackEnabled = FALSE;
static BOOL g_secureMode = FALSE;
static BOOL g_lockSupportReady = FALSE;
static UINT g_lockHandoffTicks = 0;
static BOOL g_desktopActive = TRUE;
static WORD g_secureLastPrimary;
static WORD g_secureCandidatePrimary;
static UINT g_secureCandidateCount;
static WCHAR g_configPath[1024];
static WCHAR g_exeDir[32768];
static WCHAR g_exePath[32768];
static WCHAR g_autoCommand[32768 + 4];
static WCHAR g_englishWav[32768];
static WCHAR g_russianWav[32768];
static WCHAR g_fileBuffer[32768];
static WCHAR g_regValue[32768 + 4];
static HANDLE g_boardHandle = INVALID_HANDLE_VALUE;
static BOOL g_boardConnected;
static WCHAR g_boardPort[32];
static DWORD g_boardConnectAttempts;
static DWORD g_boardTxErrors;
static DWORD g_boardTick;
static DWORD g_nightTick;
static BOOL g_nightActive;
static WCHAR g_aboutText[4096];
static BOARD_CONFIG g_boardCfg;
static BOARD_CONFIG g_editCfg;
static COLORREF g_customColors[16];

static LAYOUT_PROFILE g_profiles[MAX_LAYOUT_PROFILES];
static LAYOUT_PROFILE g_editProfiles[MAX_LAYOUT_PROFILES];
static WCHAR g_profileWav[MAX_LAYOUT_PROFILES][32768];
static UINT g_profileCount;
static UINT g_systemLayoutCount;
static UINT g_activeProfileIndex = INVALID_PROFILE_INDEX;
static UINT g_editProfileIndex;
static DWORD g_lastLayoutId;
static DWORD g_layoutCandidateId;
static UINT g_layoutCandidateCount;
static DWORD g_secureLastLayoutId;
static DWORD g_secureLayoutCandidateId;
static UINT g_secureLayoutCandidateCount;
static DWORD g_boardProtocol;

/* INI writes are deliberately surgical. Existing files are never regenerated,
   normalized or replaced. Only the byte range belonging to a user-changed key
   is updated; all unrelated bytes remain untouched. */
#define INI_SCAN_CAP (256u * 1024u)
#define INI_MOVE_CHUNK 4096u
static BYTE g_iniScan[INI_SCAN_CAP];
static BYTE g_iniMove[INI_MOVE_CHUNK];
static BYTE g_iniEncoded[65536];

static void update_tray_tooltip(void);
static void append_uint_dec(LPWSTR dst, UINT cap, DWORD v);
static BOOL board_send_config(void);
static BOOL board_send_night_mode(void);
static BOOL compute_night_active(void);
static BOOL board_try_connect(void);
static BOOL board_send_layout(WORD primary, BOOL switched);
static WCHAR wlower_ascii(WCHAR c);
static UINT wfind_ascii_ci(LPCWSTR text, LPCWSTR needle);
static void accept_language_primary(WORD primary, BOOL play, DWORD source);
static void board_close(void);

static BOOL refresh_layout_profiles(BOOL createMissingSections);
static void check_current_layout(BOOL play, BOOL secureDesktop);
static int find_profile_by_id(DWORD id);
static BOOL board_send_profiles(void);
static BOOL board_send_profile_index(UINT index, BOOL switched);
static BOOL board_send_profile_test(UINT index);
static void play_profile_sound(UINT index);
static void open_profiles_settings(HWND owner);
static LRESULT CALLBACK profiles_settings_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp);
static BOOL choose_profile_wav(HWND owner);
static BOOL reset_profile_wav(HWND owner);

#include "icon_bytes.h"

static void *lb_memset(void *dst, int value, unsigned long long count) {
    BYTE *p = (BYTE*)dst;
    while (count--) *p++ = (BYTE)value;
    return dst;
}

void *memset(void *dst, int value, unsigned long long count) {
    return lb_memset(dst, value, count);
}


static UINT wlen(LPCWSTR s) {
    UINT n = 0;
    if (!s) return 0;
    while (s[n]) ++n;
    return n;
}

static void wcopy(LPWSTR dst, UINT cap, LPCWSTR src) {
    UINT i = 0;
    if (!cap) return;
    if (src) {
        while (i + 1 < cap && src[i]) { dst[i] = src[i]; ++i; }
    }
    dst[i] = 0;
}

static void wcat(LPWSTR dst, UINT cap, LPCWSTR src) {
    UINT n = wlen(dst), i = 0;
    if (n >= cap) return;
    while (n + i + 1 < cap && src && src[i]) { dst[n+i] = src[i]; ++i; }
    dst[n+i] = 0;
}

static BOOL weq(LPCWSTR a, LPCWSTR b) {
    UINT i = 0;
    if (!a || !b) return FALSE;
    while (a[i] && b[i] && a[i] == b[i]) ++i;
    return a[i] == b[i];
}

static void wcopy_n(LPWSTR dst, UINT cap, LPCWSTR src, UINT count) {
    UINT i = 0;
    if (!cap) return;
    while (i + 1 < cap && src && i < count && src[i]) { dst[i] = src[i]; ++i; }
    dst[i] = 0;
}

static void get_dir_from_path(LPWSTR dst, UINT cap, LPCWSTR path) {
    UINT n = wlen(path);
    while (n > 0) {
        WCHAR c = path[n - 1u];
        if (c == L'\\' || c == L'/') break;
        n--;
    }
    if (!n) { wcopy(dst, cap, L"."); return; }
    wcopy_n(dst, cap, path, n - 1u);
}

static HICON create_app_icon(void) {
    DWORD size, offset;
    BYTE *bits;
    if (sizeof(g_iconIco) < 22u) return NULL;
    size = (DWORD)g_iconIco[14] | ((DWORD)g_iconIco[15] << 8) | ((DWORD)g_iconIco[16] << 16) | ((DWORD)g_iconIco[17] << 24);
    offset = (DWORD)g_iconIco[18] | ((DWORD)g_iconIco[19] << 8) | ((DWORD)g_iconIco[20] << 16) | ((DWORD)g_iconIco[21] << 24);
    if (!size || offset + size > sizeof(g_iconIco)) return NULL;
    bits = (BYTE*)(ULONG_PTR)((const BYTE*)g_iconIco + offset);
    return CreateIconFromResourceEx(bits, size, TRUE, 0x00030000u, 32, 32, 0);
}

static BOOL wparse_uint(LPCWSTR s, DWORD *out) {
    DWORD v = 0;
    UINT i = 0;
    if (!s || !s[0] || !out) return FALSE;
    while (s[i]) {
        if (s[i] < L'0' || s[i] > L'9') return FALSE;
        if (v > 1000000u) return FALSE;
        v = v * 10u + (DWORD)(s[i] - L'0');
        ++i;
    }
    *out = v;
    return TRUE;
}


static BOOL command_has(LPCWSTR needle) {
    LPCWSTR cmd = GetCommandLineW();
    return cmd && wfind_ascii_ci(cmd, needle) != 0xffffffffu;
}

static BOOL current_desktop_name(LPWSTR out, UINT cap) {
    HANDLE d;
    DWORD need = 0;
    if (!out || !cap) return FALSE;
    out[0] = 0;
    d = GetThreadDesktop(GetCurrentThreadId());
    if (!d) return FALSE;
    if (!GetUserObjectInformationW(d, UOI_NAME, out, cap * (DWORD)sizeof(WCHAR), &need)) return FALSE;
    out[cap - 1u] = 0;
    return out[0] != 0;
}

static BOOL current_desktop_is_input(void) {
    HANDLE d;
    BOOL isInput = FALSE;
    DWORD need = 0;
    d = GetThreadDesktop(GetCurrentThreadId());
    if (!d) return FALSE;
    if (!GetUserObjectInformationW(d, UOI_IO, &isInput, sizeof(isInput), &need)) return FALSE;
    return isInput;
}

static BOOL is_winlogon_desktop(void) {
    WCHAR name[64];
    if (!current_desktop_name(name, ARRAYSIZE(name))) return FALSE;
    return wfind_ascii_ci(name, L"winlogon") != 0xffffffffu;
}

static BOOL reg_set_sz(HKEY h, LPCWSTR name, LPCWSTR value) {
    DWORD bytes = (wlen(value) + 1u) * (DWORD)sizeof(WCHAR);
    return RegSetValueExW(h, name, 0, REG_SZ, (const BYTE*)value, bytes) == ERROR_SUCCESS;
}

static BOOL reg_set_dword(HKEY h, LPCWSTR name, DWORD value) {
    return RegSetValueExW(h, name, 0, REG_DWORD, (const BYTE*)&value, sizeof(value)) == ERROR_SUCCESS;
}


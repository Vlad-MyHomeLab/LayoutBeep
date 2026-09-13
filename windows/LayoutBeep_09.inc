static void acat_char(char *dst, UINT cap, char c) {
    UINT n = 0;
    while (n < cap && dst[n]) ++n;
    if (n + 1u >= cap) return;
    dst[n] = c; dst[n+1u] = 0;
}

static void acat_uint(char *dst, UINT cap, DWORD v) {
    char tmp[16]; UINT n = 0, i;
    if (v == 0) { acat_char(dst, cap, '0'); return; }
    while (v && n < ARRAYSIZE(tmp)) { tmp[n++] = (char)('0' + (v % 10u)); v /= 10u; }
    for (i = 0; i < n; ++i) acat_char(dst, cap, tmp[n - 1u - i]);
}

static void acat_value(char *dst, UINT cap, DWORD v, BOOL comma) {
    if (comma) acat_char(dst, cap, ',');
    acat_uint(dst, cap, v);
}

static BOOL board_send_config(void) {
    char msg[160];
    msg[0] = 'C'; msg[1] = 0;
    acat_value(msg, ARRAYSIZE(msg), g_boardCfg.engHz, TRUE);
    acat_value(msg, ARRAYSIZE(msg), g_boardCfg.engMs, TRUE);
    acat_value(msg, ARRAYSIZE(msg), g_boardCfg.engR, TRUE);
    acat_value(msg, ARRAYSIZE(msg), g_boardCfg.engG, TRUE);
    acat_value(msg, ARRAYSIZE(msg), g_boardCfg.engB, TRUE);
    acat_value(msg, ARRAYSIZE(msg), g_boardCfg.rusHz, TRUE);
    acat_value(msg, ARRAYSIZE(msg), g_boardCfg.rusMs, TRUE);
    acat_value(msg, ARRAYSIZE(msg), g_boardCfg.rusR, TRUE);
    acat_value(msg, ARRAYSIZE(msg), g_boardCfg.rusG, TRUE);
    acat_value(msg, ARRAYSIZE(msg), g_boardCfg.rusB, TRUE);
    acat_value(msg, ARRAYSIZE(msg), g_boardCfg.flashMs, TRUE);
    acat_value(msg, ARRAYSIZE(msg), g_boardCfg.flashR, TRUE);
    acat_value(msg, ARRAYSIZE(msg), g_boardCfg.flashG, TRUE);
    acat_value(msg, ARRAYSIZE(msg), g_boardCfg.flashB, TRUE);
    acat_char(msg, ARRAYSIZE(msg), '\n');
    return board_write_ascii(msg);
}

static BOOL board_send_night_mode(void) {
    char msg[80];
    msg[0] = 'N'; msg[1] = 0;
    acat_value(msg, ARRAYSIZE(msg), g_nightActive ? 1u : 0u, TRUE);
    acat_value(msg, ARRAYSIZE(msg), g_boardCfg.dayVolumePct, TRUE);
    acat_value(msg, ARRAYSIZE(msg), g_boardCfg.nightVolumePct, TRUE);
    acat_value(msg, ARRAYSIZE(msg), g_boardCfg.nightBrightnessPct, TRUE);
    acat_char(msg, ARRAYSIZE(msg), '\n');
    return board_write_ascii(msg);
}

static void service_night_schedule(BOOL forceSend) {
    BOOL active = compute_night_active();
    if (forceSend || active != g_nightActive) {
        g_nightActive = active;
        if (g_boardConnected) board_send_night_mode();
        update_tray_tooltip();
    }
}

static BOOL board_send_layout(WORD primary, BOOL switched) {
    char msg[4];
    if (primary != LANG_ENGLISH && primary != LANG_RUSSIAN) return FALSE;
    msg[0] = (primary == LANG_ENGLISH) ? 'E' : 'R';
    msg[1] = switched ? '1' : '0';
    msg[2] = '\n';
    msg[3] = 0;
    return board_write_ascii(msg);
}

static BOOL board_send_test(WORD primary) {
    char msg[4];
    if (primary != LANG_ENGLISH && primary != LANG_RUSSIAN) return FALSE;
    msg[0] = 'T';
    msg[1] = (primary == LANG_ENGLISH) ? 'E' : 'R';
    msg[2] = '\n';
    msg[3] = 0;
    return board_write_ascii(msg);
}

static BOOL board_try_port(LPCWSTR port) {
    WCHAR path[64];
    COMMTIMEOUTS to;
    HANDLE h;
    DWORD written = 0, got = 0, total = 0, attempt;
    BYTE reply[160];
    static const char hello[] = "?\n";

    if (!port || !port[0]) return FALSE;
    path[0] = 0;
    wcopy(path, ARRAYSIZE(path), L"\\\\.\\");
    wcat(path, ARRAYSIZE(path), port);

    h = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;

    lb_memset(&to, 0, sizeof(to));
    to.ReadIntervalTimeout = 20u;
    to.ReadTotalTimeoutConstant = 90u;
    to.WriteTotalTimeoutConstant = 180u;
    SetCommTimeouts(h, &to);
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    Sleep(25u);

    if (!WriteFile(h, hello, 2u, &written, NULL) || written != 2u) {
        CloseHandle(h);
        return FALSE;
    }
    /* USB CDC may split a short text reply into more than one packet.
       Accumulate several bounded reads instead of requiring one ReadFile()
       to contain the complete handshake line. */
    lb_memset(reply, 0, sizeof(reply));
    for (attempt = 0; attempt < 5u && total + 1u < (DWORD)sizeof(reply); ++attempt) {
        got = 0;
        if (!ReadFile(h, reply + total, (DWORD)sizeof(reply) - 1u - total, &got, NULL)) {
            CloseHandle(h);
            return FALSE;
        }
        total += got;
        reply[total] = 0;
        if ((bytes_contains(reply, total, "LB2") || bytes_contains(reply, total, "LB3") || bytes_contains(reply, total, "LB4") || bytes_contains(reply, total, "LB5")) && bytes_contains(reply, total, "RP2040-ZERO")) break;
    }
    if ((!bytes_contains(reply, total, "LB2") && !bytes_contains(reply, total, "LB3") && !bytes_contains(reply, total, "LB4") && !bytes_contains(reply, total, "LB5")) || !bytes_contains(reply, total, "RP2040-ZERO")) {
        CloseHandle(h);
        return FALSE;
    }

    g_boardHandle = h;
    g_boardConnected = TRUE;
    wcopy(g_boardPort, ARRAYSIZE(g_boardPort), port);
    board_send_config();
    board_send_night_mode();
    if (g_lastPrimary) board_send_layout(g_lastPrimary, FALSE);
    return TRUE;
}

static BOOL board_try_connect(void) {
    static const GUID GUID_DEVCLASS_PORTS = {0x4d36e978u,0xe325u,0x11ceu,{0xbfu,0xc1u,0x08u,0x00u,0x2bu,0xe1u,0x03u,0x18u}};
    HANDLE set;
    DWORD index;
    SP_DEVINFO_DATA dev;
    WCHAR hwid[512];
    WCHAR port[64];
    DWORD type, needed, cb;
    HKEY key;

    if (g_boardConnected) return TRUE;
    g_boardConnectAttempts++;
    set = SetupDiGetClassDevsW(&GUID_DEVCLASS_PORTS, NULL, NULL, DIGCF_PRESENT);
    if (set == INVALID_HANDLE_VALUE) return FALSE;

    for (index = 0; ; ++index) {
        lb_memset(&dev, 0, sizeof(dev));
        dev.cbSize = sizeof(dev);
        if (!SetupDiEnumDeviceInfo(set, index, &dev)) break;

        lb_memset(hwid, 0, sizeof(hwid));
        type = 0; needed = 0;
        if (!SetupDiGetDeviceRegistryPropertyW(set, &dev, SPDRP_HARDWAREID, &type,
                                               (LPBYTE)hwid, sizeof(hwid), &needed)) continue;
        /* The Arduino-Pico Waveshare board uses Raspberry Pi's VID 2E8A.
           Handshake below is the final identity check, so other Pico boards are safe. */
        if (wfind_ascii_ci(hwid, L"VID_2E8A") == 0xffffffffu) continue;

        key = SetupDiOpenDevRegKey(set, &dev, DICS_FLAG_GLOBAL, 0u, DIREG_DEV, KEY_QUERY_VALUE);
        if (key == (HKEY)INVALID_HANDLE_VALUE) continue;
        lb_memset(port, 0, sizeof(port));
        cb = sizeof(port); type = 0;
        if (RegQueryValueExW(key, L"PortName", NULL, &type, (LPBYTE)port, &cb) != ERROR_SUCCESS) {
            RegCloseKey(key);
            continue;
        }
        RegCloseKey(key);

        if (board_try_port(port)) {
            SetupDiDestroyDeviceInfoList(set);
            return TRUE;
        }
    }
    SetupDiDestroyDeviceInfoList(set);
    return FALSE;
}

static void board_service(void) {
    g_boardTick++;
    if (g_boardTick < 20u) return; /* timer is 50 ms => once per second */
    g_boardTick = 0;
    if (g_boardConnected) {
        if (!board_write_ascii("K\n")) update_tray_tooltip();
    } else {
        if (board_try_connect()) update_tray_tooltip();
    }
}

/* IAccessible derives from IDispatch. Release is vtable slot 2,
   get_accName is slot 10, get_accValue is slot 11. */
typedef ULONG_PTR (WINAPI *PFN_RELEASE_UNKNOWN)(LPVOID);
typedef HRESULT (WINAPI *PFN_GET_ACC_STRING)(LPVOID, VARIANT, BSTR*);


static void release_accessible(LPVOID acc) {
    void **vt;
    PFN_RELEASE_UNKNOWN fn;
    if (!acc) return;
    vt = *(void***)acc;
    fn = (PFN_RELEASE_UNKNOWN)vt[2];
    fn(acc);
}

static BOOL accessible_string(LPVOID acc, UINT slot, LPWSTR out, UINT cap) {
    void **vt;
    PFN_GET_ACC_STRING fn;
    VARIANT child;
    BSTR b = NULL;
    HRESULT hr;
    if (!acc || !out || !cap) return FALSE;
    out[0] = 0;
    lb_memset(&child, 0, sizeof(child));
    child.vt = 3; /* VT_I4 */
    child.data.lVal = CHILDID_SELF;
    vt = *(void***)acc;
    fn = (PFN_GET_ACC_STRING)vt[slot];
    hr = fn(acc, child, &b);
    if (!SUCCEEDED(hr) || !b) return FALSE;
    wcopy(out, cap, b);
    SysFreeString(b);
    return out[0] != 0;
}

static HWND find_input_indicator_button(void) {
    HWND tray = FindWindowW(L"Shell_TrayWnd", NULL);
    HWND notify, indicator, button;
    if (!tray) return NULL;
    notify = FindWindowExW(tray, NULL, L"TrayNotifyWnd", NULL);
    if (!notify) return NULL;
    indicator = FindWindowExW(notify, NULL, L"TrayInputIndicatorWClass", NULL);
    if (indicator) {
        button = FindWindowExW(indicator, NULL, L"InputIndicatorButton", NULL);
        if (button) return button;
        button = FindWindowExW(indicator, NULL, L"Button", NULL);
        if (button) return button;
    }
    button = FindWindowExW(notify, NULL, L"InputIndicatorButton", NULL);
    return button;
}

static void set_tip_text(NOTIFYICONDATAW *nid, DWORD layoutId) {
    int idx = layoutId ? find_profile_by_id(layoutId) : -1;
    wcopy(nid->szTip, ARRAYSIZE(nid->szTip), L"LayoutBeep");
    if (idx >= 0) {
        wcat(nid->szTip, ARRAYSIZE(nid->szTip), L" - ");
        wcat(nid->szTip, ARRAYSIZE(nid->szTip), g_profiles[(UINT)idx].name);
    }
    wcat(nid->szTip, ARRAYSIZE(nid->szTip), g_boardConnected ? L" | RP2040: " : L" | RP2040: offline");
    if (g_boardConnected) wcat(nid->szTip, ARRAYSIZE(nid->szTip), g_boardPort);
    if (g_boardCfg.nightEnabled) wcat(nid->szTip, ARRAYSIZE(nid->szTip), g_nightActive ? L" | ночь" : L" | день");
}

static void update_tray_tooltip(void) {
    NOTIFYICONDATAW nid;
    lb_memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_TIP;
    set_tip_text(&nid, g_lastLayoutId);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

static void add_tray_icon(void) {
    NOTIFYICONDATAW nid;
    lb_memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = g_icon;
    set_tip_text(&nid, g_lastLayoutId);
    Shell_NotifyIconW(NIM_ADD, &nid);
}

static void remove_tray_icon(void) {
    NOTIFYICONDATAW nid;
    lb_memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hwnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

/* Persist only fields that the user actually changed. Never rewrite the profile. */
static BOOL patch_board_config(const BOARD_CONFIG *oldc, const BOARD_CONFIG *newc) {
    BOOL ok = TRUE;
    if (oldc->engHz != newc->engHz && !patch_ini_uint(L"Board", L"EnglishHz", newc->engHz)) ok = FALSE;
    if (oldc->engMs != newc->engMs && !patch_ini_uint(L"Board", L"EnglishMs", newc->engMs)) ok = FALSE;
    if (oldc->engR != newc->engR && !patch_ini_uint(L"Board", L"EnglishR", newc->engR)) ok = FALSE;
    if (oldc->engG != newc->engG && !patch_ini_uint(L"Board", L"EnglishG", newc->engG)) ok = FALSE;
    if (oldc->engB != newc->engB && !patch_ini_uint(L"Board", L"EnglishB", newc->engB)) ok = FALSE;
    if (oldc->rusHz != newc->rusHz && !patch_ini_uint(L"Board", L"RussianHz", newc->rusHz)) ok = FALSE;
    if (oldc->rusMs != newc->rusMs && !patch_ini_uint(L"Board", L"RussianMs", newc->rusMs)) ok = FALSE;
    if (oldc->rusR != newc->rusR && !patch_ini_uint(L"Board", L"RussianR", newc->rusR)) ok = FALSE;
    if (oldc->rusG != newc->rusG && !patch_ini_uint(L"Board", L"RussianG", newc->rusG)) ok = FALSE;
    if (oldc->rusB != newc->rusB && !patch_ini_uint(L"Board", L"RussianB", newc->rusB)) ok = FALSE;
    if (oldc->flashMs != newc->flashMs && !patch_ini_uint(L"Board", L"FlashMs", newc->flashMs)) ok = FALSE;
    if (oldc->flashR != newc->flashR && !patch_ini_uint(L"Board", L"FlashR", newc->flashR)) ok = FALSE;
    if (oldc->flashG != newc->flashG && !patch_ini_uint(L"Board", L"FlashG", newc->flashG)) ok = FALSE;
    if (oldc->flashB != newc->flashB && !patch_ini_uint(L"Board", L"FlashB", newc->flashB)) ok = FALSE;
    if (oldc->nightEnabled != newc->nightEnabled &&
        !patch_ini_key(L"Night", L"Enabled", newc->nightEnabled ? L"1" : L"0")) ok = FALSE;
    if (oldc->nightStartMin != newc->nightStartMin && !patch_ini_time(L"Night", L"Start", newc->nightStartMin)) ok = FALSE;
    if (oldc->nightEndMin != newc->nightEndMin && !patch_ini_time(L"Night", L"End", newc->nightEndMin)) ok = FALSE;
    if (oldc->dayVolumePct != newc->dayVolumePct && !patch_ini_uint(L"Audio", L"DayVolume", newc->dayVolumePct)) ok = FALSE;
    if (oldc->nightVolumePct != newc->nightVolumePct && !patch_ini_uint(L"Night", L"Volume", newc->nightVolumePct)) ok = FALSE;
    if (oldc->nightBrightnessPct != newc->nightBrightnessPct && !patch_ini_uint(L"Night", L"Brightness", newc->nightBrightnessPct)) ok = FALSE;
    return ok;
}

static HKEY open_run_key(DWORD access) {
    HKEY h = NULL;
    DWORD disposition = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                        0, NULL, 0, access, NULL, &h, &disposition) != ERROR_SUCCESS) {
        return NULL;
    }
    return h;
}

static void build_auto_command(void) {
    UINT i = 0, j = 0;
    g_autoCommand[j++] = L'"';
    while (g_exePath[i] && j + 2 < ARRAYSIZE(g_autoCommand)) g_autoCommand[j++] = g_exePath[i++];
    g_autoCommand[j++] = L'"';
    g_autoCommand[j] = 0;
}

static BOOL get_autostart(LPWSTR valueOut, DWORD cchOut) {
    HKEY h = open_run_key(KEY_QUERY_VALUE);
    DWORD type = 0, cb;
    LSTATUS st;
    if (!h) return FALSE;
    cb = cchOut * (DWORD)sizeof(WCHAR);
    st = RegQueryValueExW(h, L"LayoutBeep", NULL, &type, (LPBYTE)valueOut, &cb);
    RegCloseKey(h);
    if (st != ERROR_SUCCESS || type != REG_SZ || !cchOut) {
        if (cchOut) valueOut[0] = 0;
        return FALSE;
    }
    valueOut[cchOut - 1] = 0;
    return TRUE;
}

static BOOL set_autostart(BOOL enable) {
    HKEY h = open_run_key(KEY_QUERY_VALUE | KEY_SET_VALUE);
    LSTATUS st;
    if (!h) return FALSE;
    if (enable) {
        DWORD bytes = (wlen(g_autoCommand) + 1u) * (DWORD)sizeof(WCHAR);
        st = RegSetValueExW(h, L"LayoutBeep", 0, REG_SZ, (const BYTE*)g_autoCommand, bytes);
    } else {
        st = RegDeleteValueW(h, L"LayoutBeep");
        if (st == ERROR_FILE_NOT_FOUND) st = ERROR_SUCCESS;
    }
    RegCloseKey(h);
    return st == ERROR_SUCCESS;
}

static BOOL init_paths_and_settings(void) {
    WCHAR tmp[16];
    DWORD n;
    BOOL configWasMissing;

    lb_memset(g_configPath, 0, sizeof(g_configPath));
    lb_memset(g_exeDir, 0, sizeof(g_exeDir));
    lb_memset(g_exePath, 0, sizeof(g_exePath));
    lb_memset(g_autoCommand, 0, sizeof(g_autoCommand));
    lb_memset(g_englishWav, 0, sizeof(g_englishWav));
    lb_memset(g_russianWav, 0, sizeof(g_russianWav));


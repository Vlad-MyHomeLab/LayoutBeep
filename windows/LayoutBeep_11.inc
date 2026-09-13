    n = GetModuleFileNameW(NULL, g_exePath, ARRAYSIZE(g_exePath));
    if (!n || n >= ARRAYSIZE(g_exePath)) return FALSE;
    get_dir_from_path(g_exeDir, ARRAYSIZE(g_exeDir), g_exePath);
    wcopy(g_configPath, ARRAYSIZE(g_configPath), g_exeDir);
    wcat(g_configPath, ARRAYSIZE(g_configPath), L"\\LayoutBeep.ini");
    build_auto_command();
    configWasMissing = (GetFileAttributesW(g_configPath) == INVALID_FILE_ATTRIBUTES);
    /* If the INI is absent, create a COMPLETE default configuration.
       This is the only startup-time write. An existing file is never regenerated:
       all later persistence uses surgical key patches only. */
    if (configWasMissing) {
        if (!patch_ini_key(L"General", L"PcFallbackEnabled", L"0") ||
            !patch_ini_key(L"Sounds", L"EnglishWav", L"") ||
            !patch_ini_key(L"Sounds", L"RussianWav", L"") ||
            !patch_ini_key(L"Board", L"EnglishHz", L"1450") ||
            !patch_ini_key(L"Board", L"EnglishMs", L"75") ||
            !patch_ini_key(L"Board", L"EnglishR", L"0") ||
            !patch_ini_key(L"Board", L"EnglishG", L"0") ||
            !patch_ini_key(L"Board", L"EnglishB", L"28") ||
            !patch_ini_key(L"Board", L"RussianHz", L"850") ||
            !patch_ini_key(L"Board", L"RussianMs", L"75") ||
            !patch_ini_key(L"Board", L"RussianR", L"28") ||
            !patch_ini_key(L"Board", L"RussianG", L"0") ||
            !patch_ini_key(L"Board", L"RussianB", L"0") ||
            !patch_ini_key(L"Board", L"FlashMs", L"80") ||
            !patch_ini_key(L"Board", L"FlashR", L"90") ||
            !patch_ini_key(L"Board", L"FlashG", L"90") ||
            !patch_ini_key(L"Board", L"FlashB", L"90") ||
            !patch_ini_key(L"Audio", L"DayVolume", L"100") ||
            !patch_ini_key(L"Night", L"Enabled", L"0") ||
            !patch_ini_key(L"Night", L"Start", L"23:00") ||
            !patch_ini_key(L"Night", L"End", L"07:00") ||
            !patch_ini_key(L"Night", L"Volume", L"25") ||
            !patch_ini_key(L"Night", L"Brightness", L"20")) return FALSE;
    }

    GetPrivateProfileStringW(L"General", L"PcFallbackEnabled", L"0", tmp, ARRAYSIZE(tmp), g_configPath);
    g_pcFallbackEnabled = !weq(tmp, L"0");
    GetPrivateProfileStringW(L"Sounds", L"EnglishWav", L"", g_englishWav, ARRAYSIZE(g_englishWav), g_configPath);
    GetPrivateProfileStringW(L"Sounds", L"RussianWav", L"", g_russianWav, ARRAYSIZE(g_russianWav), g_configPath);

    set_default_board_config(&g_boardCfg);
    g_boardCfg.engHz = ini_uint(L"Board", L"EnglishHz", g_boardCfg.engHz, 100u, 10000u);
    g_boardCfg.engMs = ini_uint(L"Board", L"EnglishMs", g_boardCfg.engMs, 10u, 2000u);
    g_boardCfg.engR = (BYTE)ini_uint(L"Board", L"EnglishR", g_boardCfg.engR, 0u, 255u);
    g_boardCfg.engG = (BYTE)ini_uint(L"Board", L"EnglishG", g_boardCfg.engG, 0u, 255u);
    g_boardCfg.engB = (BYTE)ini_uint(L"Board", L"EnglishB", g_boardCfg.engB, 0u, 255u);
    g_boardCfg.rusHz = ini_uint(L"Board", L"RussianHz", g_boardCfg.rusHz, 100u, 10000u);
    g_boardCfg.rusMs = ini_uint(L"Board", L"RussianMs", g_boardCfg.rusMs, 10u, 2000u);
    g_boardCfg.rusR = (BYTE)ini_uint(L"Board", L"RussianR", g_boardCfg.rusR, 0u, 255u);
    g_boardCfg.rusG = (BYTE)ini_uint(L"Board", L"RussianG", g_boardCfg.rusG, 0u, 255u);
    g_boardCfg.rusB = (BYTE)ini_uint(L"Board", L"RussianB", g_boardCfg.rusB, 0u, 255u);
    g_boardCfg.flashMs = ini_uint(L"Board", L"FlashMs", g_boardCfg.flashMs, 10u, 2000u);
    g_boardCfg.flashR = (BYTE)ini_uint(L"Board", L"FlashR", g_boardCfg.flashR, 0u, 255u);
    g_boardCfg.flashG = (BYTE)ini_uint(L"Board", L"FlashG", g_boardCfg.flashG, 0u, 255u);
    g_boardCfg.flashB = (BYTE)ini_uint(L"Board", L"FlashB", g_boardCfg.flashB, 0u, 255u);
    GetPrivateProfileStringW(L"Night", L"Enabled", g_boardCfg.nightEnabled ? L"1" : L"0", tmp, ARRAYSIZE(tmp), g_configPath);
    g_boardCfg.nightEnabled = !weq(tmp, L"0");
    g_boardCfg.nightStartMin = ini_time(L"Night", L"Start", g_boardCfg.nightStartMin);
    g_boardCfg.nightEndMin = ini_time(L"Night", L"End", g_boardCfg.nightEndMin);
    g_boardCfg.dayVolumePct = ini_uint(L"Audio", L"DayVolume", g_boardCfg.dayVolumePct, 0u, 100u);
    g_boardCfg.nightVolumePct = ini_uint(L"Night", L"Volume", g_boardCfg.nightVolumePct, 0u, 100u);
    g_boardCfg.nightBrightnessPct = ini_uint(L"Night", L"Brightness", g_boardCfg.nightBrightnessPct, 0u, 100u);
    g_nightActive = compute_night_active();

    if (configWasMissing) {
        /* First run: the complete default INI has just been created above. */
        set_autostart(TRUE);
    } else {
        if (get_autostart(g_regValue, ARRAYSIZE(g_regValue)) && !weq(g_regValue, g_autoCommand)) {
            set_autostart(TRUE); /* EXE was moved: follow the new location. */
        }
    }
    return TRUE;
}

static void play_language_sound(BOOL english) {
    LPCWSTR path = english ? g_englishWav : g_russianWav;
    if (path[0] && PlaySoundW(path, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT)) return;
    Beep(english ? g_boardCfg.engHz : g_boardCfg.rusHz, english ? g_boardCfg.engMs : g_boardCfg.rusMs);
}

static void accept_language_primary(WORD primary, BOOL play, DWORD source) {
    BOOL changed;
    BOOL sent = FALSE;
    if (!primary) return;

    changed = (g_lastPrimary == 0 || primary != g_lastPrimary);
    if (!changed) return;

    g_lastPrimary = primary;
    update_tray_tooltip();
    (void)source;

    if (!play) return;
    if (!g_boardConnected) board_try_connect();
    if (g_boardConnected) sent = board_send_layout(primary, TRUE);
    if (!sent && g_pcFallbackEnabled) {
        if (primary == LANG_ENGLISH) play_language_sound(TRUE);
        else if (primary == LANG_RUSSIAN) play_language_sound(FALSE);
    }
    update_tray_tooltip();
}


static void check_tray_indicator(BOOL play) {
    static const GUID IID_IAccessible = {0x618736e0u,0x3c3du,0x11cfu,{0x81u,0x0cu,0x00u,0xaau,0x00u,0x38u,0x9bu,0x71u}};
    LPVOID acc = NULL;
    WCHAR text[256];
    WORD primary = 0;
    HRESULT hr;

    if (!g_trayIndicatorButton || !IsWindow(g_trayIndicatorButton))
        g_trayIndicatorButton = find_input_indicator_button();
    if (!g_trayIndicatorButton) {
        g_trayReadFailures++;
        g_trayCandidatePrimary = 0;
        g_trayCandidateCount = 0;
        return;
    }

    hr = AccessibleObjectFromWindow(g_trayIndicatorButton, (DWORD)OBJID_CLIENT, &IID_IAccessible, &acc);
    if (!SUCCEEDED(hr) || !acc) {
        g_trayReadFailures++;
        g_trayCandidatePrimary = 0;
        g_trayCandidateCount = 0;
        return;
    }

    text[0] = 0;
    if (accessible_string(acc, 10u, text, ARRAYSIZE(text))) primary = primary_from_indicator_text(text);
    if (!primary) {
        WCHAR value[256]; value[0] = 0;
        if (accessible_string(acc, 11u, value, ARRAYSIZE(value))) {
            if (!text[0]) wcopy(text, ARRAYSIZE(text), value);
            primary = primary_from_indicator_text(value);
        }
    }
    release_accessible(acc);

    if (text[0]) wcopy(g_lastTrayText, ARRAYSIZE(g_lastTrayText), text);
    if (!primary) {
        g_trayReadFailures++;
        g_trayCandidatePrimary = 0;
        g_trayCandidateCount = 0;
        return;
    }

    /* First valid reading establishes the baseline silently. */
    if (!g_lastTrayPrimary) {
        g_lastTrayPrimary = primary;
        g_trayCandidatePrimary = 0;
        g_trayCandidateCount = 0;
        accept_language_primary(primary, FALSE, 0u);
        return;
    }

    /* Same as the already-confirmed tray state: there is no change. */
    if (primary == g_lastTrayPrimary) {
        g_trayCandidatePrimary = 0;
        g_trayCandidateCount = 0;
        return;
    }

    /* A different tray state must be observed twice in a row. This filters
       short-lived/stale Accessibility values and, most importantly, makes all
       keyboard/shell hook timing irrelevant to the sound decision. */
    if (primary != g_trayCandidatePrimary) {
        g_trayCandidatePrimary = primary;
        g_trayCandidateCount = 1u;
        return;
    }

    if (g_trayCandidateCount < 0xffffffffu) g_trayCandidateCount++;
    if (g_trayCandidateCount < 2u) return;

    g_lastTrayPrimary = primary;
    g_trayCandidatePrimary = 0;
    g_trayCandidateCount = 0;
    g_trayChanges++;
    accept_language_primary(primary, play, 4u);
}


static HWND create_child(HWND parent, LPCWSTR cls, LPCWSTR text, DWORD style,
                         int x, int y, int w, int h, UINT id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                            x, y, w, h, parent, (HMENU)(ULONG_PTR)id, g_instance, NULL);
    if (c && g_uiFont) SendMessageW(c, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
    return c;
}

static RECT rect_xywh(int x, int y, int w, int h) {
    RECT r; r.left=x; r.top=y; r.right=x+w; r.bottom=y+h; return r;
}

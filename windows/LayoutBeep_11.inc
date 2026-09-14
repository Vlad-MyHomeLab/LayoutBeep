    n = GetModuleFileNameW(NULL, g_exePath, ARRAYSIZE(g_exePath));
    if (!n || n >= ARRAYSIZE(g_exePath)) return FALSE;
    get_dir_from_path(g_exeDir, ARRAYSIZE(g_exeDir), g_exePath);
    wcopy(g_configPath, ARRAYSIZE(g_configPath), g_exeDir);
    wcat(g_configPath, ARRAYSIZE(g_configPath), L"\\LayoutBeep.ini");
    build_auto_command();
    configWasMissing = (GetFileAttributesW(g_configPath) == INVALID_FILE_ATTRIBUTES);

    /* A missing INI is created once. Its profile sections are generated from the
       keyboard layouts actually installed in Windows. Existing INI files are never
       regenerated; missing per-layout settings live only in memory until the user
       explicitly changes them. */
    if (configWasMissing) {
        if (!patch_ini_key(L"General", L"PcFallbackEnabled", L"0") ||
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

    /* Legacy 2.7 keys are read only as fallback for an upgrade. They are never
       created or rewritten by 2.8.0. */
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

    if (!refresh_layout_profiles(configWasMissing)) return FALSE;

    if (configWasMissing) {
        set_autostart(TRUE);
    } else {
        if (get_autostart(g_regValue, ARRAYSIZE(g_regValue)) && !weq(g_regValue, g_autoCommand)) {
            set_autostart(TRUE);
        }
    }
    return TRUE;
}

/* Legacy fallback for old call sites. Generic 2.8 paths use play_profile_sound(). */
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
    check_current_layout(play, FALSE);
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

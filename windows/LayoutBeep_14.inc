static void open_board_settings(HWND owner) {
    if (g_settingsHwnd && IsWindow(g_settingsHwnd)) {
        SetForegroundWindow(g_settingsHwnd);
        return;
    }
    g_editCfg = g_boardCfg;
    g_settingsHwnd = CreateWindowExW(0, L"LayoutBeepSettingsWindow", L"LayoutBeep — настройки RP2040",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        220, 60, 640, 770, owner, NULL, g_instance, NULL);
    if (!g_settingsHwnd) {
        MessageBoxW(owner, L"Не удалось открыть окно настроек.", L"LayoutBeep", MB_OK | MB_ICONERROR);
        return;
    }

    /* Legacy 2.7 settings window is retained only for source compatibility.
       The tray menu opens the dynamic profile window from LayoutBeep_18.inc. */
    create_child(g_settingsHwnd, L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 38, 145, 105, 30, IDC_ENG_HZ);
    create_child(g_settingsHwnd, L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 197, 145, 105, 30, IDC_ENG_MS);
    create_child(g_settingsHwnd, L"BUTTON", L"", BS_OWNERDRAW, 356, 143, 220, 34, IDC_ENG_COLOR);
    create_child(g_settingsHwnd, L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 38, 267, 105, 30, IDC_RUS_HZ);
    create_child(g_settingsHwnd, L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 197, 267, 105, 30, IDC_RUS_MS);
    create_child(g_settingsHwnd, L"BUTTON", L"", BS_OWNERDRAW, 356, 265, 220, 34, IDC_RUS_COLOR);
    create_child(g_settingsHwnd, L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 38, 389, 105, 30, IDC_FLASH_MS);
    create_child(g_settingsHwnd, L"BUTTON", L"", BS_OWNERDRAW, 197, 387, 260, 34, IDC_FLASH_COLOR);
    create_child(g_settingsHwnd, L"BUTTON", L"", BS_OWNERDRAW, 458, 444, 118, 32, IDC_NIGHT_TOGGLE);
    create_child(g_settingsHwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 58, 510, 64, 30, IDC_NIGHT_FROM);
    create_child(g_settingsHwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 158, 510, 64, 30, IDC_NIGHT_TO);
    create_child(g_settingsHwnd, L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 238, 510, 78, 30, IDC_DAY_VOLUME);
    create_child(g_settingsHwnd, L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 370, 510, 78, 30, IDC_NIGHT_VOLUME);
    create_child(g_settingsHwnd, L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 38, 566, 78, 30, IDC_NIGHT_BRIGHT);
    create_child(g_settingsHwnd, L"BUTTON", L"По умолчанию", BS_OWNERDRAW, 24, 638, 132, 36, IDC_DEFAULTS);
    create_child(g_settingsHwnd, L"BUTTON", L"Отмена", BS_OWNERDRAW, 382, 638, 96, 36, IDC_CANCEL);
    create_child(g_settingsHwnd, L"BUTTON", L"Сохранить", BS_OWNERDRAW, 488, 638, 116, 36, IDC_SAVE);
    populate_settings_window(g_settingsHwnd);
    ShowWindow(g_settingsHwnd, SW_SHOW);
    SetForegroundWindow(g_settingsHwnd);
}

static BOOL choose_wav(HWND owner, BOOL english) {
    static const WCHAR filter[] = L"WAV files (*.wav)\0*.wav\0All files (*.*)\0*.*\0\0";
    OPENFILENAMEW ofn;
    LPCWSTR current = english ? g_englishWav : g_russianWav;
    lb_memset(&ofn, 0, sizeof(ofn));
    wcopy(g_fileBuffer, ARRAYSIZE(g_fileBuffer), current);
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = g_fileBuffer;
    ofn.nMaxFile = ARRAYSIZE(g_fileBuffer);
    ofn.lpstrTitle = english ? L"Выберите звук для English" : L"Выберите звук для Русский";
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"wav";
    if (!GetOpenFileNameW(&ofn)) return FALSE;
    if (english) {
        if (!patch_ini_key(L"Sounds", L"EnglishWav", g_fileBuffer)) {
            MessageBoxW(owner, L"Не удалось точечно изменить EnglishWav в LayoutBeep.ini.",
                        L"LayoutBeep — ошибка записи INI", MB_OK | MB_ICONERROR);
            return FALSE;
        }
        wcopy(g_englishWav, ARRAYSIZE(g_englishWav), g_fileBuffer);
    } else {
        if (!patch_ini_key(L"Sounds", L"RussianWav", g_fileBuffer)) {
            MessageBoxW(owner, L"Не удалось точечно изменить RussianWav в LayoutBeep.ini.",
                        L"LayoutBeep — ошибка записи INI", MB_OK | MB_ICONERROR);
            return FALSE;
        }
        wcopy(g_russianWav, ARRAYSIZE(g_russianWav), g_fileBuffer);
    }
    play_language_sound(english);
    return TRUE;
}

static void append_uint_dec(LPWSTR dst, UINT cap, DWORD v) {
    WCHAR tmp[16];
    UINT n = 0, i;
    if (v == 0) { wcat(dst, cap, L"0"); return; }
    while (v && n < ARRAYSIZE(tmp)) { tmp[n++] = (WCHAR)(L'0' + (v % 10u)); v /= 10u; }
    for (i = 0; i < n; ++i) {
        WCHAR one[2]; one[0] = tmp[n - 1u - i]; one[1] = 0; wcat(dst, cap, one);
    }
}

static void show_about(HWND owner) {
    LPWSTR text = g_aboutText;
    text[0] = 0;
    wcat(text, ARRAYSIZE(g_aboutText), L"LayoutBeep 2.8.0 + RP2040-Zero\n\n");
    wcat(text, ARRAYSIZE(g_aboutText), L"Раскладки: список Windows (GetKeyboardLayoutList), до 6 профилей.\n");
    wcat(text, ARRAYSIZE(g_aboutText), L"Текущее состояние: HKL активного окна, подтверждение двумя чтениями.\n\n");

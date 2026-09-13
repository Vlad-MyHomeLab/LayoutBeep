    wcat(text, ARRAYSIZE(g_aboutText), L"RP2040-Zero: ");
    if (g_boardConnected) {
        wcat(text, ARRAYSIZE(g_aboutText), L"ПОДКЛЮЧЕНА, ");
        wcat(text, ARRAYSIZE(g_aboutText), g_boardPort);
    } else {
        wcat(text, ARRAYSIZE(g_aboutText), L"не найдена");
    }
    wcat(text, ARRAYSIZE(g_aboutText), L"\nПопыток поиска платы: ");
    append_uint_dec(text, ARRAYSIZE(g_aboutText), g_boardConnectAttempts);
    wcat(text, ARRAYSIZE(g_aboutText), L"\nОшибок передачи USB: ");
    append_uint_dec(text, ARRAYSIZE(g_aboutText), g_boardTxErrors);
    wcat(text, ARRAYSIZE(g_aboutText), L"\nEnglish: "); append_uint_dec(text, ARRAYSIZE(g_aboutText), g_boardCfg.engHz); wcat(text, ARRAYSIZE(g_aboutText), L" Гц / "); append_uint_dec(text, ARRAYSIZE(g_aboutText), g_boardCfg.engMs); wcat(text, ARRAYSIZE(g_aboutText), L" мс");
    wcat(text, ARRAYSIZE(g_aboutText), L"\nРусский: "); append_uint_dec(text, ARRAYSIZE(g_aboutText), g_boardCfg.rusHz); wcat(text, ARRAYSIZE(g_aboutText), L" Гц / "); append_uint_dec(text, ARRAYSIZE(g_aboutText), g_boardCfg.rusMs); wcat(text, ARRAYSIZE(g_aboutText), L" мс");
    wcat(text, ARRAYSIZE(g_aboutText), L"\nНочной режим: ");
    if (!g_boardCfg.nightEnabled) wcat(text, ARRAYSIZE(g_aboutText), L"выключен");
    else {
        WCHAR tb[8];
        wcat(text, ARRAYSIZE(g_aboutText), g_nightActive ? L"АКТИВЕН, " : L"ожидание, ");
        format_hhmm(g_boardCfg.nightStartMin, tb, ARRAYSIZE(tb)); wcat(text, ARRAYSIZE(g_aboutText), tb);
        wcat(text, ARRAYSIZE(g_aboutText), L"–");
        format_hhmm(g_boardCfg.nightEndMin, tb, ARRAYSIZE(tb)); wcat(text, ARRAYSIZE(g_aboutText), tb);
        wcat(text, ARRAYSIZE(g_aboutText), L", громкость ночь "); append_uint_dec(text, ARRAYSIZE(g_aboutText), g_boardCfg.nightVolumePct);
        wcat(text, ARRAYSIZE(g_aboutText), L"%, день "); append_uint_dec(text, ARRAYSIZE(g_aboutText), g_boardCfg.dayVolumePct);
        wcat(text, ARRAYSIZE(g_aboutText), L"%, яркость ночь "); append_uint_dec(text, ARRAYSIZE(g_aboutText), g_boardCfg.nightBrightnessPct); wcat(text, ARRAYSIZE(g_aboutText), L"%");
    }
    wcat(text, ARRAYSIZE(g_aboutText), L"\nРезервный звук на ПК: ");
    wcat(text, ARRAYSIZE(g_aboutText), g_pcFallbackEnabled ? L"включён" : L"выключен");
    wcat(text, ARRAYSIZE(g_aboutText), L"\nЭкран блокировки: ");
    wcat(text, ARRAYSIZE(g_aboutText), secure_support_installed() ? L"зарегистрирован" : L"не зарегистрирован");
    wcat(text, ARRAYSIZE(g_aboutText), L"\nEase of Access notification: ");
    wcat(text, ARRAYSIZE(g_aboutText), g_lockSupportReady ? L"OK" : L"нет");
    wcat(text, ARRAYSIZE(g_aboutText), L"\nWinlogon helper: ");
    wcat(text, ARRAYSIZE(g_aboutText), secure_helper_running() ? L"запущен" : L"пока не обнаружен");
    wcat(text, ARRAYSIZE(g_aboutText), L"\nINI-файл: ");
    wcat(text, ARRAYSIZE(g_aboutText), g_configPath);

    wcat(text, ARRAYSIZE(g_aboutText), L"\n\nInputIndicatorButton: ");
    wcat(text, ARRAYSIZE(g_aboutText), g_trayIndicatorButton ? L"НАЙДЕН" : L"не найден");
    if (g_lastTrayText[0]) {
        wcat(text, ARRAYSIZE(g_aboutText), L"\nAccessibility-текст: ");
        wcat(text, ARRAYSIZE(g_aboutText), g_lastTrayText);
    }
    wcat(text, ARRAYSIZE(g_aboutText), L"\nПодтверждённых изменений индикатора: ");
    append_uint_dec(text, ARRAYSIZE(g_aboutText), g_trayChanges);
    wcat(text, ARRAYSIZE(g_aboutText), L"\nОшибок/пустых чтений индикатора: ");
    append_uint_dec(text, ARRAYSIZE(g_aboutText), g_trayReadFailures);
    MessageBoxW(owner, text, L"LayoutBeep", MB_OK | MB_ICONINFORMATION);
}

static void append_menu(HMENU menu, UINT flags, UINT id, LPCWSTR text) {
    AppendMenuW(menu, flags, (ULONG_PTR)id, text);
}

static void show_menu(HWND owner) {
    HMENU menu = CreatePopupMenu();
    POINT p;
    UINT cmd;
    BOOL autoEnabled;
    if (!menu) return;

    { WCHAR status[96]; status[0] = 0;
      if (g_boardConnected) { wcat(status, ARRAYSIZE(status), L"RP2040: "); wcat(status, ARRAYSIZE(status), g_boardPort); wcat(status, ARRAYSIZE(status), L" — подключена"); }
      else wcat(status, ARRAYSIZE(status), L"RP2040: не найдена");
      append_menu(menu, MF_STRING | MF_GRAYED, 0, status); }
    append_menu(menu, MF_STRING, CMD_BOARD_RECONNECT, L"Переподключить RP2040");
    append_menu(menu, MF_STRING, CMD_BOARD_SETTINGS, L"Настройки RP2040...");
    append_menu(menu, MF_STRING, CMD_TEST_ENGLISH, L"Тест платы: English");
    append_menu(menu, MF_STRING, CMD_TEST_RUSSIAN, L"Тест платы: Русский");
    append_menu(menu, MF_SEPARATOR, 0, NULL);
    append_menu(menu, MF_STRING | (g_pcFallbackEnabled ? MF_CHECKED : 0), CMD_SOUNDS_ENABLED, L"Резервный звук на ПК");
    append_menu(menu, MF_SEPARATOR, 0, NULL);
    append_menu(menu, MF_STRING, CMD_ENGLISH_WAV, L"Звук English (WAV)...");
    append_menu(menu, MF_STRING, CMD_RUSSIAN_WAV, L"Звук Русский (WAV)...");
    append_menu(menu, MF_STRING, CMD_RESET_SOUNDS, L"Сбросить на встроенные звуки");
    append_menu(menu, MF_SEPARATOR, 0, NULL);

    { BOOL lockInstalled = secure_support_installed();
      BOOL lockReady = lockInstalled && g_lockSupportReady;
      append_menu(menu, MF_STRING | (lockReady ? MF_CHECKED : 0), CMD_LOCKSCREEN,
                  lockReady ? L"Работать на экране блокировки" :
                  (lockInstalled ? L"Повторно включить экран блокировки" : L"Включить работу на экране блокировки...")); }
    autoEnabled = get_autostart(g_regValue, ARRAYSIZE(g_regValue));
    append_menu(menu, MF_STRING | (autoEnabled ? MF_CHECKED : 0), CMD_AUTOSTART, L"Запускать вместе с Windows");
    append_menu(menu, MF_STRING, CMD_ABOUT, L"О программе");
    append_menu(menu, MF_SEPARATOR, 0, NULL);
    append_menu(menu, MF_STRING, CMD_EXIT, L"Выход");

    GetCursorPos(&p);
    SetForegroundWindow(owner);
    cmd = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD, p.x, p.y, 0, owner, NULL);
    PostMessageW(owner, WM_NULL, 0, 0);
    DestroyMenu(menu);

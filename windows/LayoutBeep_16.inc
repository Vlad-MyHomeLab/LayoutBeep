    switch (cmd) {
    case CMD_SOUNDS_ENABLED:
        {
            BOOL nv = !g_pcFallbackEnabled;
            if (patch_ini_key(L"General", L"PcFallbackEnabled", nv ? L"1" : L"0")) g_pcFallbackEnabled = nv;
            else MessageBoxW(owner, L"Не удалось точечно изменить PcFallbackEnabled в LayoutBeep.ini.",
                             L"LayoutBeep — ошибка записи INI", MB_OK | MB_ICONERROR);
        }
        break;
    case CMD_ENGLISH_WAV:
        choose_wav(owner, TRUE);
        break;
    case CMD_RUSSIAN_WAV:
        choose_wav(owner, FALSE);
        break;
    case CMD_BOARD_SETTINGS:
        open_board_settings(owner);
        break;
    case CMD_TEST_ENGLISH:
        if (!g_boardConnected) board_try_connect();
        if (!board_send_test(LANG_ENGLISH) && g_pcFallbackEnabled) play_language_sound(TRUE);
        update_tray_tooltip();
        break;
    case CMD_TEST_RUSSIAN:
        if (!g_boardConnected) board_try_connect();
        if (!board_send_test(LANG_RUSSIAN) && g_pcFallbackEnabled) play_language_sound(FALSE);
        update_tray_tooltip();
        break;
    case CMD_BOARD_RECONNECT:
        board_close();
        board_try_connect();
        update_tray_tooltip();
        break;
    case CMD_RESET_SOUNDS:
        {
            BOOL ok = TRUE;
            if (g_englishWav[0] && !patch_ini_key(L"Sounds", L"EnglishWav", L"")) ok = FALSE;
            if (g_russianWav[0] && !patch_ini_key(L"Sounds", L"RussianWav", L"")) ok = FALSE;
            if (ok) { g_englishWav[0] = 0; g_russianWav[0] = 0; }
            else MessageBoxW(owner, L"Не удалось точечно сбросить WAV-пути в LayoutBeep.ini.",
                             L"LayoutBeep — ошибка записи INI", MB_OK | MB_ICONERROR);
        }
        break;
    case CMD_LOCKSCREEN:
        if (secure_support_installed()) {
            if (g_lockSupportReady) request_secure_support_toggle(owner, FALSE);
            else {
                remove_at_configuration_item(AT_OLD_KEY_NAME);
                remove_at_configuration_item(AT_KEY_NAME);
                g_lockSupportReady = notify_ease_of_access(TRUE);
                MessageBoxW(owner,
                    g_lockSupportReady ? L"Windows повторно уведомлена. Можно проверить Win+L."
                                       : L"Повторное уведомление Ease of Access Center не удалось. USB на блокировке останется у обычного LayoutBeep.",
                    L"LayoutBeep — экран блокировки", MB_OK | (g_lockSupportReady ? MB_ICONINFORMATION : MB_ICONERROR));
            }
        } else request_secure_support_toggle(owner, TRUE);
        break;
    case CMD_AUTOSTART:
        if (!set_autostart(!autoEnabled)) {
            MessageBoxW(owner, L"Не удалось изменить автозапуск.", L"LayoutBeep", MB_OK | MB_ICONERROR);
        }
        break;
    case CMD_ABOUT:
        show_about(owner);
        break;
    case CMD_EXIT:
        KillTimer(owner, 1);
        if (!g_secureMode && secure_support_installed()) notify_ease_of_access(FALSE);
        board_close();
        remove_tray_icon();
        PostQuitMessage(0);
        break;
    }
}

static LRESULT CALLBACK window_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (g_taskbarCreated && msg == g_taskbarCreated) {
        add_tray_icon();
        return 0;
    }
    if (msg == WM_TIMER) {
        BOOL activeNow = current_desktop_is_input();
        if (++g_nightTick >= 1200u) {
            g_nightTick = 0u;
            service_night_schedule(FALSE);
        }
        if (g_secureMode) {
            if (activeNow != g_desktopActive) {
                g_desktopActive = activeNow;
                if (!g_desktopActive) board_close();
                else {
                    g_secureLastPrimary = 0;
                    g_secureCandidatePrimary = 0;
                    g_secureCandidateCount = 0;
                    check_secure_layout(FALSE);
                    board_try_connect();
                }
            }
            if (g_desktopActive) {
                check_secure_layout(TRUE);
                board_service();
            }
            return 0;
        }

        /* Normal desktop. Hand USB to the secure helper only after Ease of Access
           registration/notification succeeded. If the helper never appears, resume
           heartbeat from the inactive desktop after about one second so the RP2040
           keeps the last valid colour instead of going orange/offline. */
        if (activeNow != g_desktopActive) {
            g_desktopActive = activeNow;
            g_lockHandoffTicks = 0;
            if (g_desktopActive) {
                g_trayIndicatorButton = NULL;
                g_lastTrayPrimary = 0;
                g_trayCandidatePrimary = 0;
                g_trayCandidateCount = 0;
                check_tray_indicator(FALSE);
                board_try_connect();
            } else if (g_lockSupportReady && secure_support_installed()) {
                board_close();
            }
        }

        if (g_desktopActive) {
            check_tray_indicator(TRUE);
            board_service();
        } else if (g_lockSupportReady && secure_support_installed()) {
            if (secure_helper_running()) {
                board_close();
            } else {
                if (g_lockHandoffTicks < 0xffffffffu) g_lockHandoffTicks++;
                if (g_lockHandoffTicks <= 20u) {
                    board_close();
                } else {
                    if (!g_boardConnected) board_try_connect();
                    board_service();
                }
            }
        } else {
            if (!g_boardConnected) board_try_connect();
            board_service();
        }
        return 0;
    }
    if (msg == WM_TRAY) {
        UINT event = (UINT)((ULONG_PTR)lp & 0xffffu);
        if (event == WM_RBUTTONUP || event == WM_CONTEXTMENU) {
            show_menu(h);
            return 0;
        }
        if (event == WM_LBUTTONDBLCLK) {
            BOOL nv = !g_pcFallbackEnabled;
            if (patch_ini_key(L"General", L"PcFallbackEnabled", nv ? L"1" : L"0")) g_pcFallbackEnabled = nv;
            return 0;
        }
    }
    if (msg == WM_CLOSE || msg == WM_DESTROY) {
        KillTimer(h, 1);
        if (!g_secureMode && secure_support_installed()) notify_ease_of_access(FALSE);
        board_close();
        remove_tray_icon();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void WINAPI WinMainCRTStartup(void) {
    HANDLE mutex;
    HINSTANCE inst;
    WNDCLASSW wc;
    MSG msg;

    if (!init_paths_and_settings()) {
        MessageBoxW(NULL, L"Не удалось инициализировать LayoutBeep.", L"LayoutBeep", MB_OK | MB_ICONERROR);
        ExitProcess(1);
    }

    if (command_has(L"/secure-install")) ExitProcess(install_secure_support_elevated() ? 0u : 10u);
    if (command_has(L"/secure-remove")) ExitProcess(remove_secure_support_elevated() ? 0u : 11u);
    g_secureMode = command_has(L"/secure");
    if (g_secureMode && !is_winlogon_desktop()) ExitProcess(0);

    mutex = CreateMutexW(NULL, FALSE, g_secureMode ? L"Local\\LayoutBeep.Singleton.Secure" : L"Local\\LayoutBeep.Singleton.Normal");
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) ExitProcess(0);

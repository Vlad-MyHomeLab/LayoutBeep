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
        choose_profile_wav(owner);
        break;
    case CMD_RUSSIAN_WAV:
        choose_profile_wav(owner);
        break;
    case CMD_BOARD_SETTINGS:
        open_profiles_settings(owner);
        break;
    case CMD_TEST_ENGLISH:
    case CMD_TEST_RUSSIAN:
        if (g_activeProfileIndex != INVALID_PROFILE_INDEX) {
            if (!g_boardConnected) board_try_connect();
            if (!board_send_profile_test(g_activeProfileIndex) && g_pcFallbackEnabled)
                play_profile_sound(g_activeProfileIndex);
            update_tray_tooltip();
        }
        break;
    case CMD_BOARD_RECONNECT:
        board_close();
        board_try_connect();
        update_tray_tooltip();
        break;
    case CMD_RESET_SOUNDS:
        reset_profile_wav(owner);
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
                    g_secureLastLayoutId = 0;
                    g_secureLayoutCandidateId = 0;
                    g_secureLayoutCandidateCount = 0;
                    check_current_layout(FALSE, TRUE);
                    board_try_connect();
                }
            }
            if (g_desktopActive) {
                check_current_layout(TRUE, TRUE);
                board_service();
            }
            return 0;
        }

        if (activeNow != g_desktopActive) {
            g_desktopActive = activeNow;
            g_lockHandoffTicks = 0;
            if (g_desktopActive) {
                g_trayIndicatorButton = NULL;
                g_lastLayoutId = 0;
                g_layoutCandidateId = 0;
                g_layoutCandidateCount = 0;
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

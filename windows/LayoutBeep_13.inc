static void set_edit_uint(HWND wnd, UINT id, DWORD v) {
    WCHAR buf[24]; buf[0] = 0; append_uint_dec(buf, ARRAYSIZE(buf), v);
    SetWindowTextW(GetDlgItem(wnd, (int)id), buf);
}

static BOOL read_edit_uint(HWND wnd, UINT id, DWORD minv, DWORD maxv, DWORD *out) {
    WCHAR buf[32]; DWORD v;
    buf[0] = 0;
    GetWindowTextW(GetDlgItem(wnd, (int)id), buf, ARRAYSIZE(buf));
    if (!wparse_uint(buf, &v) || v < minv || v > maxv) return FALSE;
    *out = v;
    return TRUE;
}

static void set_edit_time(HWND wnd, UINT id, DWORD minutes) {
    WCHAR buf[8]; format_hhmm(minutes, buf, ARRAYSIZE(buf));
    SetWindowTextW(GetDlgItem(wnd, (int)id), buf);
}

static BOOL read_edit_time(HWND wnd, UINT id, DWORD *minutesOut) {
    WCHAR buf[32]; buf[0] = 0;
    GetWindowTextW(GetDlgItem(wnd, (int)id), buf, ARRAYSIZE(buf));
    return parse_hhmm(buf, minutesOut);
}

static void update_night_toggle_text(HWND wnd) {
    HWND b = GetDlgItem(wnd, (int)IDC_NIGHT_TOGGLE);
    if (b) {
        SetWindowTextW(b, g_editCfg.nightEnabled ? L"Включён" : L"Выключен");
        InvalidateRect(b, NULL, TRUE);
    }
}

static void set_color_button(HWND wnd, UINT id, BYTE r, BYTE g, BYTE b) {
    WCHAR text[64]; text[0] = 0;
    wcat(text, ARRAYSIZE(text), L"R="); append_uint_dec(text, ARRAYSIZE(text), r);
    wcat(text, ARRAYSIZE(text), L"  G="); append_uint_dec(text, ARRAYSIZE(text), g);
    wcat(text, ARRAYSIZE(text), L"  B="); append_uint_dec(text, ARRAYSIZE(text), b);
    SetWindowTextW(GetDlgItem(wnd, (int)id), text);
}

static BOOL choose_rgb(HWND owner, BYTE *r, BYTE *g, BYTE *b) {
    CHOOSECOLORW cc;
    lb_memset(&cc, 0, sizeof(cc));
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = owner;
    cc.rgbResult = RGB_(*r, *g, *b);
    cc.lpCustColors = g_customColors;
    cc.Flags = CC_RGBINIT | CC_FULLOPEN;
    if (!ChooseColorW(&cc)) return FALSE;
    *r = GET_R_VALUE(cc.rgbResult);
    *g = GET_G_VALUE(cc.rgbResult);
    *b = GET_B_VALUE(cc.rgbResult);
    return TRUE;
}

static void populate_settings_window(HWND wnd) {
    set_edit_uint(wnd, IDC_ENG_HZ, g_editCfg.engHz);
    set_edit_uint(wnd, IDC_ENG_MS, g_editCfg.engMs);
    set_color_button(wnd, IDC_ENG_COLOR, g_editCfg.engR, g_editCfg.engG, g_editCfg.engB);
    set_edit_uint(wnd, IDC_RUS_HZ, g_editCfg.rusHz);
    set_edit_uint(wnd, IDC_RUS_MS, g_editCfg.rusMs);
    set_color_button(wnd, IDC_RUS_COLOR, g_editCfg.rusR, g_editCfg.rusG, g_editCfg.rusB);
    set_edit_uint(wnd, IDC_FLASH_MS, g_editCfg.flashMs);
    set_color_button(wnd, IDC_FLASH_COLOR, g_editCfg.flashR, g_editCfg.flashG, g_editCfg.flashB);
    set_edit_time(wnd, IDC_NIGHT_FROM, g_editCfg.nightStartMin);
    set_edit_time(wnd, IDC_NIGHT_TO, g_editCfg.nightEndMin);
    set_edit_uint(wnd, IDC_DAY_VOLUME, g_editCfg.dayVolumePct);
    set_edit_uint(wnd, IDC_NIGHT_VOLUME, g_editCfg.nightVolumePct);
    set_edit_uint(wnd, IDC_NIGHT_BRIGHT, g_editCfg.nightBrightnessPct);
    update_night_toggle_text(wnd);
}

static BOOL apply_settings_window(HWND wnd) {
    BOARD_CONFIG n = g_editCfg;
    DWORD v;
    if (!read_edit_uint(wnd, IDC_ENG_HZ, 100u, 10000u, &v)) goto bad; n.engHz = v;
    if (!read_edit_uint(wnd, IDC_ENG_MS, 10u, 2000u, &v)) goto bad; n.engMs = v;
    if (!read_edit_uint(wnd, IDC_RUS_HZ, 100u, 10000u, &v)) goto bad; n.rusHz = v;
    if (!read_edit_uint(wnd, IDC_RUS_MS, 10u, 2000u, &v)) goto bad; n.rusMs = v;
    if (!read_edit_uint(wnd, IDC_FLASH_MS, 10u, 2000u, &v)) goto bad; n.flashMs = v;
    if (!read_edit_time(wnd, IDC_NIGHT_FROM, &n.nightStartMin)) goto bad;
    if (!read_edit_time(wnd, IDC_NIGHT_TO, &n.nightEndMin)) goto bad;
    if (!read_edit_uint(wnd, IDC_DAY_VOLUME, 0u, 100u, &v)) goto bad; n.dayVolumePct = v;
    if (!read_edit_uint(wnd, IDC_NIGHT_VOLUME, 0u, 100u, &v)) goto bad; n.nightVolumePct = v;
    if (!read_edit_uint(wnd, IDC_NIGHT_BRIGHT, 0u, 100u, &v)) goto bad; n.nightBrightnessPct = v;
    n.nightEnabled = g_editCfg.nightEnabled;
    {
        BOARD_CONFIG old = g_boardCfg;
        if (!patch_board_config(&old, &n)) {
            MessageBoxW(wnd,
                L"Не удалось точечно обновить LayoutBeep.ini. Файл не был пересоздан. Проверьте права доступа.",
                L"LayoutBeep — ошибка записи INI", MB_OK | MB_ICONERROR);
            return FALSE;
        }
    }
    g_boardCfg = n;
    g_nightActive = compute_night_active();
    if (!g_boardConnected) board_try_connect();
    if (g_boardConnected) {
        board_send_config();
        board_send_night_mode();
        if (g_lastPrimary) board_send_layout(g_lastPrimary, FALSE);
    }
    update_tray_tooltip();
    return TRUE;
bad:
    MessageBoxW(wnd,
        L"Проверьте значения. Частота: 100..10000 Гц; длительность: 10..2000 мс; время: ЧЧ:ММ; проценты: 0..100.",
        L"LayoutBeep — неверные настройки", MB_OK | MB_ICONERROR);
    return FALSE;
}

static LRESULT CALLBACK settings_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT) { paint_settings_background(h); return 0; }
    if (msg == WM_DRAWITEM) { draw_owner_button((const DRAWITEMSTRUCT*)lp); return TRUE; }
    if (msg == WM_COMMAND) {
        UINT id = LOWORD_(wp);
        if (id == IDC_ENG_COLOR) {
            if (choose_rgb(h, &g_editCfg.engR, &g_editCfg.engG, &g_editCfg.engB)) {
                set_color_button(h, IDC_ENG_COLOR, g_editCfg.engR, g_editCfg.engG, g_editCfg.engB);
                InvalidateRect(GetDlgItem(h, IDC_ENG_COLOR), NULL, TRUE);
            }
            return 0;
        }
        if (id == IDC_RUS_COLOR) {
            if (choose_rgb(h, &g_editCfg.rusR, &g_editCfg.rusG, &g_editCfg.rusB)) {
                set_color_button(h, IDC_RUS_COLOR, g_editCfg.rusR, g_editCfg.rusG, g_editCfg.rusB);
                InvalidateRect(GetDlgItem(h, IDC_RUS_COLOR), NULL, TRUE);
            }
            return 0;
        }
        if (id == IDC_FLASH_COLOR) {
            if (choose_rgb(h, &g_editCfg.flashR, &g_editCfg.flashG, &g_editCfg.flashB)) {
                set_color_button(h, IDC_FLASH_COLOR, g_editCfg.flashR, g_editCfg.flashG, g_editCfg.flashB);
                InvalidateRect(GetDlgItem(h, IDC_FLASH_COLOR), NULL, TRUE);
            }
            return 0;
        }
        if (id == IDC_NIGHT_TOGGLE) {
            g_editCfg.nightEnabled = !g_editCfg.nightEnabled;
            update_night_toggle_text(h);
            return 0;
        }
        if (id == IDC_DEFAULTS) {
            set_default_board_config(&g_editCfg);
            populate_settings_window(h);
            InvalidateRect(h, NULL, TRUE);
            return 0;
        }
        if (id == IDC_SAVE) {
            if (apply_settings_window(h)) DestroyWindow(h);
            return 0;
        }
        if (id == IDC_CANCEL) { DestroyWindow(h); return 0; }
    }
    if (msg == WM_CLOSE) { DestroyWindow(h); return 0; }
    if (msg == WM_DESTROY) { if (h == g_settingsHwnd) g_settingsHwnd = NULL; return 0; }
    return DefWindowProcW(h, msg, wp, lp);
}

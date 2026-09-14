static WCHAR hex_digit(UINT v) {
    v &= 0x0fu;
    return (WCHAR)(v < 10u ? (L'0' + v) : (L'A' + (v - 10u)));
}

static void format_layout_id(DWORD id, LPWSTR out, UINT cap) {
    UINT i;
    if (!out || cap < 9u) return;
    for (i = 0; i < 8u; ++i) {
        UINT shift = (7u - i) * 4u;
        out[i] = hex_digit((id >> shift) & 0x0fu);
    }
    out[8] = 0;
}

static void make_profile_section(DWORD id, LPWSTR out, UINT cap) {
    WCHAR idText[16];
    format_layout_id(id, idText, ARRAYSIZE(idText));
    wcopy(out, cap, L"Layout_");
    wcat(out, cap, idText);
}

static void copy_profile(LAYOUT_PROFILE *dst, const LAYOUT_PROFILE *src) {
    if (!dst || !src) return;
    dst->id = src->id;
    dst->hkl = src->hkl;
    wcopy(dst->section, ARRAYSIZE(dst->section), src->section);
    wcopy(dst->name, ARRAYSIZE(dst->name), src->name);
    dst->hz = src->hz;
    dst->ms = src->ms;
    dst->r = src->r;
    dst->g = src->g;
    dst->b = src->b;
}

static void set_default_profile_cue(UINT slot, LAYOUT_PROFILE *p) {
    static const DWORD hz[MAX_LAYOUT_PROFILES] = {1450u, 850u, 1150u, 1750u, 2050u, 2550u};
    static const BYTE rgb[MAX_LAYOUT_PROFILES][3] = {
        {0u, 0u, 28u},
        {28u, 0u, 0u},
        {0u, 28u, 0u},
        {28u, 18u, 0u},
        {20u, 0u, 28u},
        {0u, 24u, 28u}
    };
    UINT i = slot % MAX_LAYOUT_PROFILES;
    if (!p) return;
    p->hz = hz[i];
    p->ms = 75u;
    p->r = rgb[i][0];
    p->g = rgb[i][1];
    p->b = rgb[i][2];
}

static void load_system_layout_name(DWORD id, LPWSTR out, UINT cap) {
    HKEY h;
    WCHAR keyPath[160], idText[16];
    DWORD type = 0, cb;
    if (!out || !cap) return;
    out[0] = 0;
    format_layout_id(id, idText, ARRAYSIZE(idText));
    wcopy(keyPath, ARRAYSIZE(keyPath), L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\");
    wcat(keyPath, ARRAYSIZE(keyPath), idText);
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_QUERY_VALUE, &h) == ERROR_SUCCESS) {
        cb = cap * (DWORD)sizeof(WCHAR);
        if (RegQueryValueExW(h, L"Layout Text", NULL, &type, (LPBYTE)out, &cb) != ERROR_SUCCESS || type != REG_SZ)
            out[0] = 0;
        out[cap - 1u] = 0;
        RegCloseKey(h);
    }
    if (!out[0]) {
        wcopy(out, cap, L"Раскладка ");
        wcat(out, cap, idText);
    }
}

static int find_profile_by_id(DWORD id) {
    UINT i;
    for (i = 0; i < g_profileCount; ++i) if (g_profiles[i].id == id) return (int)i;
    return -1;
}

static BOOL profile_has_legacy_keys(WORD primary) {
    WCHAR tmp[32];
    tmp[0] = 0;
    if (primary == LANG_ENGLISH)
        GetPrivateProfileStringW(L"Board", L"EnglishHz", L"", tmp, ARRAYSIZE(tmp), g_configPath);
    else if (primary == LANG_RUSSIAN)
        GetPrivateProfileStringW(L"Board", L"RussianHz", L"", tmp, ARRAYSIZE(tmp), g_configPath);
    return tmp[0] != 0;
}

static void load_profile_settings(UINT slot, LAYOUT_PROFILE *p) {
    WORD primary;
    BOOL legacy;
    LPCWSTR legacyWav = L"";
    if (!p) return;
    set_default_profile_cue(slot, p);
    primary = PRIMARYLANGID((WORD)(p->id & 0xffffu));
    legacy = profile_has_legacy_keys(primary);
    if (legacy && primary == LANG_ENGLISH) {
        p->hz = g_boardCfg.engHz; p->ms = g_boardCfg.engMs;
        p->r = g_boardCfg.engR; p->g = g_boardCfg.engG; p->b = g_boardCfg.engB;
        legacyWav = g_englishWav;
    } else if (legacy && primary == LANG_RUSSIAN) {
        p->hz = g_boardCfg.rusHz; p->ms = g_boardCfg.rusMs;
        p->r = g_boardCfg.rusR; p->g = g_boardCfg.rusG; p->b = g_boardCfg.rusB;
        legacyWav = g_russianWav;
    }

    p->hz = ini_uint(p->section, L"Hz", p->hz, 100u, 10000u);
    p->ms = ini_uint(p->section, L"Ms", p->ms, 10u, 2000u);
    p->r = (BYTE)ini_uint(p->section, L"R", p->r, 0u, 255u);
    p->g = (BYTE)ini_uint(p->section, L"G", p->g, 0u, 255u);
    p->b = (BYTE)ini_uint(p->section, L"B", p->b, 0u, 255u);
    GetPrivateProfileStringW(p->section, L"Wav", legacyWav,
                             g_profileWav[slot], ARRAYSIZE(g_profileWav[slot]), g_configPath);
}

static BOOL persist_complete_profile(UINT slot) {
    LAYOUT_PROFILE *p;
    if (slot >= g_profileCount) return FALSE;
    p = &g_profiles[slot];
    if (!patch_ini_uint(p->section, L"Hz", p->hz)) return FALSE;
    if (!patch_ini_uint(p->section, L"Ms", p->ms)) return FALSE;
    if (!patch_ini_uint(p->section, L"R", p->r)) return FALSE;
    if (!patch_ini_uint(p->section, L"G", p->g)) return FALSE;
    if (!patch_ini_uint(p->section, L"B", p->b)) return FALSE;
    if (!patch_ini_key(p->section, L"Wav", g_profileWav[slot])) return FALSE;
    return TRUE;
}

static BOOL refresh_layout_profiles(BOOL createMissingSections) {
    HANDLE hkls[32];
    DWORD ids[MAX_LAYOUT_PROFILES];
    HANDLE selected[MAX_LAYOUT_PROFILES];
    int total, got;
    UINT i, count = 0;

    total = GetKeyboardLayoutList(0, NULL);
    if (total < 0) total = 0;
    g_systemLayoutCount = (UINT)total;
    got = total > 0 ? GetKeyboardLayoutList(total > (int)ARRAYSIZE(hkls) ? (int)ARRAYSIZE(hkls) : total, hkls) : 0;
    if (got < 0) got = 0;

    for (i = 0; i < (UINT)got && count < MAX_LAYOUT_PROFILES; ++i) {
        DWORD id = (DWORD)(ULONG_PTR)hkls[i];
        UINT j;
        BOOL duplicate = FALSE;
        for (j = 0; j < count; ++j) if (ids[j] == id) { duplicate = TRUE; break; }
        if (duplicate) continue;
        ids[count] = id;
        selected[count] = hkls[i];
        ++count;
    }

    if (!count) {
        HWND fg = GetForegroundWindow();
        DWORD tid = fg ? GetWindowThreadProcessId(fg, NULL) : 0u;
        HANDLE hkl = GetKeyboardLayout(tid);
        if (hkl) {
            ids[0] = (DWORD)(ULONG_PTR)hkl;
            selected[0] = hkl;
            count = 1u;
            if (!g_systemLayoutCount) g_systemLayoutCount = 1u;
        }
    }

    g_profileCount = count;
    for (i = 0; i < count; ++i) {
        LAYOUT_PROFILE *p = &g_profiles[i];
        lb_memset(p, 0, sizeof(*p));
        p->id = ids[i];
        p->hkl = selected[i];
        make_profile_section(p->id, p->section, ARRAYSIZE(p->section));
        load_system_layout_name(p->id, p->name, ARRAYSIZE(p->name));
        g_profileWav[i][0] = 0;
        load_profile_settings(i, p);
    }
    for (; i < MAX_LAYOUT_PROFILES; ++i) {
        lb_memset(&g_profiles[i], 0, sizeof(g_profiles[i]));
        g_profileWav[i][0] = 0;
    }

    if (createMissingSections) {
        for (i = 0; i < g_profileCount; ++i) if (!persist_complete_profile(i)) return FALSE;
    }

    g_activeProfileIndex = INVALID_PROFILE_INDEX;
    if (g_lastLayoutId) {
        int idx = find_profile_by_id(g_lastLayoutId);
        if (idx >= 0) g_activeProfileIndex = (UINT)idx;
    }
    return TRUE;
}

static DWORD foreground_layout_id(void) {
    HWND fg = GetForegroundWindow();
    DWORD tid;
    HANDLE hkl;
    if (!fg) return 0;
    tid = GetWindowThreadProcessId(fg, NULL);
    if (!tid) return 0;
    hkl = GetKeyboardLayout(tid);
    return hkl ? (DWORD)(ULONG_PTR)hkl : 0u;
}

static void update_indicator_diagnostic(void) {
    static const GUID IID_IAccessible = {0x618736e0u,0x3c3du,0x11cfu,{0x81u,0x0cu,0x00u,0xaau,0x00u,0x38u,0x9bu,0x71u}};
    LPVOID acc = NULL;
    WCHAR text[256], value[256];
    HRESULT hr;
    if (!g_comInitialized) return;
    if (!g_trayIndicatorButton || !IsWindow(g_trayIndicatorButton))
        g_trayIndicatorButton = find_input_indicator_button();
    if (!g_trayIndicatorButton) { g_trayReadFailures++; return; }
    hr = AccessibleObjectFromWindow(g_trayIndicatorButton, (DWORD)OBJID_CLIENT, &IID_IAccessible, &acc);
    if (!SUCCEEDED(hr) || !acc) { g_trayReadFailures++; return; }
    text[0] = 0; value[0] = 0;
    if (accessible_string(acc, 10u, text, ARRAYSIZE(text)) && text[0])
        wcopy(g_lastTrayText, ARRAYSIZE(g_lastTrayText), text);
    else if (accessible_string(acc, 11u, value, ARRAYSIZE(value)) && value[0])
        wcopy(g_lastTrayText, ARRAYSIZE(g_lastTrayText), value);
    else g_trayReadFailures++;
    release_accessible(acc);
}

static void play_profile_sound(UINT index) {
    if (index >= g_profileCount) return;
    if (g_profileWav[index][0] &&
        PlaySoundW(g_profileWav[index], NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT)) return;
    Beep(g_profiles[index].hz, g_profiles[index].ms);
}

static void check_current_layout(BOOL play, BOOL secureDesktop) {
    DWORD id;
    DWORD *last;
    DWORD *candidate;
    UINT *candidateCount;
    int idx;

    if (!secureDesktop) update_indicator_diagnostic();
    id = foreground_layout_id();
    if (!id) return;

    idx = find_profile_by_id(id);
    if (idx < 0 && !g_settingsHwnd) {
        refresh_layout_profiles(FALSE);
        idx = find_profile_by_id(id);
    }
    if (idx < 0) return; /* More than six layouts or transient system state. */

    if (secureDesktop) {
        last = &g_secureLastLayoutId;
        candidate = &g_secureLayoutCandidateId;
        candidateCount = &g_secureLayoutCandidateCount;
    } else {
        last = &g_lastLayoutId;
        candidate = &g_layoutCandidateId;
        candidateCount = &g_layoutCandidateCount;
    }

    if (!*last) {
        *last = id;
        *candidate = 0;
        *candidateCount = 0;
        g_lastLayoutId = id;
        g_lastPrimary = PRIMARYLANGID((WORD)(id & 0xffffu));
        g_activeProfileIndex = (UINT)idx;
        update_tray_tooltip();
        if (!g_boardConnected) board_try_connect();
        if (g_boardConnected) board_send_profile_index((UINT)idx, FALSE);
        return;
    }

    if (id == *last) {
        *candidate = 0;
        *candidateCount = 0;
        g_activeProfileIndex = (UINT)idx;
        return;
    }

    if (id != *candidate) {
        *candidate = id;
        *candidateCount = 1u;
        return;
    }
    if (*candidateCount < 0xffffffffu) (*candidateCount)++;
    if (*candidateCount < 2u) return;

    *last = id;
    *candidate = 0;
    *candidateCount = 0;
    g_lastLayoutId = id;
    g_lastPrimary = PRIMARYLANGID((WORD)(id & 0xffffu));
    g_activeProfileIndex = (UINT)idx;
    if (!secureDesktop) g_trayChanges++;
    update_tray_tooltip();

    if (!play) return;
    if (!g_boardConnected) board_try_connect();
    if (!board_send_profile_index((UINT)idx, TRUE) && g_pcFallbackEnabled)
        play_profile_sound((UINT)idx);
    update_tray_tooltip();
}

static BOOL board_send_profiles(void) {
    char msg[180];
    UINT i;
    if (!g_boardConnected) return FALSE;

    if (g_boardProtocol >= 6u) {
        msg[0] = 'G'; msg[1] = 0;
        acat_value(msg, ARRAYSIZE(msg), g_boardCfg.flashMs, TRUE);
        acat_value(msg, ARRAYSIZE(msg), g_boardCfg.flashR, TRUE);
        acat_value(msg, ARRAYSIZE(msg), g_boardCfg.flashG, TRUE);
        acat_value(msg, ARRAYSIZE(msg), g_boardCfg.flashB, TRUE);
        acat_char(msg, ARRAYSIZE(msg), '\n');
        if (!board_write_ascii(msg)) return FALSE;

        for (i = 0; i < g_profileCount; ++i) {
            LAYOUT_PROFILE *p = &g_profiles[i];
            msg[0] = 'P'; msg[1] = 0;
            acat_value(msg, ARRAYSIZE(msg), i, TRUE);
            acat_value(msg, ARRAYSIZE(msg), p->hz, TRUE);
            acat_value(msg, ARRAYSIZE(msg), p->ms, TRUE);
            acat_value(msg, ARRAYSIZE(msg), p->r, TRUE);
            acat_value(msg, ARRAYSIZE(msg), p->g, TRUE);
            acat_value(msg, ARRAYSIZE(msg), p->b, TRUE);
            acat_char(msg, ARRAYSIZE(msg), '\n');
            if (!board_write_ascii(msg)) return FALSE;
        }
        return TRUE;
    }

    /* LB5 compatibility: map the first two generic profiles to E/R slots. */
    {
        LAYOUT_PROFILE fallbackA, fallbackB;
        LAYOUT_PROFILE *a, *b;
        lb_memset(&fallbackA, 0, sizeof(fallbackA));
        lb_memset(&fallbackB, 0, sizeof(fallbackB));
        set_default_profile_cue(0u, &fallbackA);
        set_default_profile_cue(1u, &fallbackB);
        a = g_profileCount ? &g_profiles[0] : &fallbackA;
        b = g_profileCount > 1u ? &g_profiles[1] : (g_profileCount ? &g_profiles[0] : &fallbackB);
        msg[0] = 'C'; msg[1] = 0;
        acat_value(msg, ARRAYSIZE(msg), a->hz, TRUE);
        acat_value(msg, ARRAYSIZE(msg), a->ms, TRUE);
        acat_value(msg, ARRAYSIZE(msg), a->r, TRUE);
        acat_value(msg, ARRAYSIZE(msg), a->g, TRUE);
        acat_value(msg, ARRAYSIZE(msg), a->b, TRUE);
        acat_value(msg, ARRAYSIZE(msg), b->hz, TRUE);
        acat_value(msg, ARRAYSIZE(msg), b->ms, TRUE);
        acat_value(msg, ARRAYSIZE(msg), b->r, TRUE);
        acat_value(msg, ARRAYSIZE(msg), b->g, TRUE);
        acat_value(msg, ARRAYSIZE(msg), b->b, TRUE);
        acat_value(msg, ARRAYSIZE(msg), g_boardCfg.flashMs, TRUE);
        acat_value(msg, ARRAYSIZE(msg), g_boardCfg.flashR, TRUE);
        acat_value(msg, ARRAYSIZE(msg), g_boardCfg.flashG, TRUE);
        acat_value(msg, ARRAYSIZE(msg), g_boardCfg.flashB, TRUE);
        acat_char(msg, ARRAYSIZE(msg), '\n');
        return board_write_ascii(msg);
    }
}

static BOOL board_send_profile_index(UINT index, BOOL switched) {
    char msg[32];
    if (index >= g_profileCount || !g_boardConnected) return FALSE;
    if (g_boardProtocol >= 6u) {
        msg[0] = 'L'; msg[1] = 0;
        acat_value(msg, ARRAYSIZE(msg), index, TRUE);
        acat_value(msg, ARRAYSIZE(msg), switched ? 1u : 0u, TRUE);
        acat_char(msg, ARRAYSIZE(msg), '\n');
        return board_write_ascii(msg);
    }
    if (index > 1u) return FALSE;
    msg[0] = index == 0u ? 'E' : 'R';
    msg[1] = switched ? '1' : '0';
    msg[2] = '\n'; msg[3] = 0;
    return board_write_ascii(msg);
}

static BOOL board_send_profile_test(UINT index) {
    char msg[24];
    if (index >= g_profileCount || !g_boardConnected) return FALSE;
    if (g_boardProtocol >= 6u) {
        msg[0] = 'T'; msg[1] = 0;
        acat_value(msg, ARRAYSIZE(msg), index, TRUE);
        acat_char(msg, ARRAYSIZE(msg), '\n');
        return board_write_ascii(msg);
    }
    if (index > 1u) return FALSE;
    msg[0] = 'T'; msg[1] = index == 0u ? 'E' : 'R'; msg[2] = '\n'; msg[3] = 0;
    return board_write_ascii(msg);
}

static BOOL patch_profile_config(const LAYOUT_PROFILE *oldp, const LAYOUT_PROFILE *newp) {
    BOOL ok = TRUE;
    if (!oldp || !newp || oldp->id != newp->id) return FALSE;
    if (oldp->hz != newp->hz && !patch_ini_uint(newp->section, L"Hz", newp->hz)) ok = FALSE;
    if (oldp->ms != newp->ms && !patch_ini_uint(newp->section, L"Ms", newp->ms)) ok = FALSE;
    if (oldp->r != newp->r && !patch_ini_uint(newp->section, L"R", newp->r)) ok = FALSE;
    if (oldp->g != newp->g && !patch_ini_uint(newp->section, L"G", newp->g)) ok = FALSE;
    if (oldp->b != newp->b && !patch_ini_uint(newp->section, L"B", newp->b)) ok = FALSE;
    return ok;
}

static void profile_color_text(HWND wnd, UINT id, const LAYOUT_PROFILE *p) {
    WCHAR text[64];
    if (!p) return;
    text[0] = 0;
    wcat(text, ARRAYSIZE(text), L"R="); append_uint_dec(text, ARRAYSIZE(text), p->r);
    wcat(text, ARRAYSIZE(text), L"  G="); append_uint_dec(text, ARRAYSIZE(text), p->g);
    wcat(text, ARRAYSIZE(text), L"  B="); append_uint_dec(text, ARRAYSIZE(text), p->b);
    SetWindowTextW(GetDlgItem(wnd, (int)id), text);
}

static void flash_color_text(HWND wnd) {
    WCHAR text[64]; text[0] = 0;
    wcat(text, ARRAYSIZE(text), L"R="); append_uint_dec(text, ARRAYSIZE(text), g_editCfg.flashR);
    wcat(text, ARRAYSIZE(text), L"  G="); append_uint_dec(text, ARRAYSIZE(text), g_editCfg.flashG);
    wcat(text, ARRAYSIZE(text), L"  B="); append_uint_dec(text, ARRAYSIZE(text), g_editCfg.flashB);
    SetWindowTextW(GetDlgItem(wnd, (int)IDC_FLASH_COLOR), text);
}

static void populate_profile_editor(HWND wnd) {
    if (g_editProfileIndex >= g_profileCount) return;
    set_edit_uint(wnd, IDC_PROFILE_HZ, g_editProfiles[g_editProfileIndex].hz);
    set_edit_uint(wnd, IDC_PROFILE_MS, g_editProfiles[g_editProfileIndex].ms);
    profile_color_text(wnd, IDC_PROFILE_COLOR, &g_editProfiles[g_editProfileIndex]);
}

static BOOL capture_profile_editor(HWND wnd) {
    DWORD v;
    LAYOUT_PROFILE *p;
    if (g_editProfileIndex >= g_profileCount) return TRUE;
    p = &g_editProfiles[g_editProfileIndex];
    if (!read_edit_uint(wnd, IDC_PROFILE_HZ, 100u, 10000u, &v)) return FALSE;
    p->hz = v;
    if (!read_edit_uint(wnd, IDC_PROFILE_MS, 10u, 2000u, &v)) return FALSE;
    p->ms = v;
    return TRUE;
}

static void update_profiles_night_text(HWND wnd) {
    HWND b = GetDlgItem(wnd, (int)IDC_NIGHT_TOGGLE);
    if (b) SetWindowTextW(b, g_editCfg.nightEnabled ? L"Включён" : L"Выключен");
}

static void paint_profiles_settings(HWND h) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    RECT client;
    COLORREF bg = RGB_(246,248,251), card = RGB_(255,255,255), border = RGB_(224,229,236);
    GetClientRect(h, &client);
    { HBRUSH br=CreateSolidBrush(bg); FillRect(dc,&client,br); DeleteObject(br); }
    paint_text(dc, L"Настройки раскладок", rect_xywh(24,18,360,30), g_titleFont, RGB_(25,32,44), DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc, L"Профили берутся из установленных раскладок Windows", rect_xywh(24,48,500,22), g_smallFont, RGB_(105,113,126), DT_LEFT|DT_SINGLELINE|DT_VCENTER);

    paint_round_box(dc,20,82,584,162,12,card,border);
    paint_text(dc,L"Раскладка",rect_xywh(38,94,160,24),g_sectionFont,RGB_(29,36,48),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Частота",rect_xywh(38,165,90,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Гц",rect_xywh(153,192,30,24),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Длительность",rect_xywh(197,165,105,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"мс",rect_xywh(312,192,30,24),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Цвет",rect_xywh(356,165,80,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);

    paint_round_box(dc,20,254,584,94,12,card,border);
    paint_text(dc,L"Вспышка при переключении",rect_xywh(38,266,260,24),g_sectionFont,RGB_(29,36,48),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Длительность",rect_xywh(38,300,105,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"мс",rect_xywh(153,327,30,20),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Цвет",rect_xywh(197,300,80,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);

    paint_round_box(dc,20,358,584,174,12,card,border);
    paint_text(dc,L"Ночной режим",rect_xywh(38,370,180,24),g_sectionFont,RGB_(29,36,48),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"С",rect_xywh(38,416,20,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"до",rect_xywh(132,416,24,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Громкость днём",rect_xywh(238,416,118,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Громкость ночью",rect_xywh(370,416,122,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Яркость ночью",rect_xywh(38,472,110,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Оранжевый offline-индикатор не приглушается",rect_xywh(166,496,330,22),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    EndPaint(h,&ps);
}

static BOOL apply_profiles_settings(HWND wnd) {
    BOARD_CONFIG n = g_editCfg;
    DWORD v;
    UINT i;
    if (!capture_profile_editor(wnd)) goto bad;
    if (!read_edit_uint(wnd, IDC_FLASH_MS, 10u, 2000u, &v)) goto bad; n.flashMs = v;
    if (!read_edit_time(wnd, IDC_NIGHT_FROM, &n.nightStartMin)) goto bad;
    if (!read_edit_time(wnd, IDC_NIGHT_TO, &n.nightEndMin)) goto bad;
    if (!read_edit_uint(wnd, IDC_DAY_VOLUME, 0u, 100u, &v)) goto bad; n.dayVolumePct = v;
    if (!read_edit_uint(wnd, IDC_NIGHT_VOLUME, 0u, 100u, &v)) goto bad; n.nightVolumePct = v;
    if (!read_edit_uint(wnd, IDC_NIGHT_BRIGHT, 0u, 100u, &v)) goto bad; n.nightBrightnessPct = v;
    n.nightEnabled = g_editCfg.nightEnabled;

    for (i = 0; i < g_profileCount; ++i) {
        if (!patch_profile_config(&g_profiles[i], &g_editProfiles[i])) {
            MessageBoxW(wnd, L"Не удалось точечно сохранить настройки одной из раскладок.",
                        L"LayoutBeep — ошибка записи INI", MB_OK | MB_ICONERROR);
            return FALSE;
        }
    }
    if (!patch_board_config(&g_boardCfg, &n)) {
        MessageBoxW(wnd, L"Не удалось точечно сохранить общие настройки.",
                    L"LayoutBeep — ошибка записи INI", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    for (i = 0; i < g_profileCount; ++i) copy_profile(&g_profiles[i], &g_editProfiles[i]);
    g_boardCfg = n;
    g_nightActive = compute_night_active();
    g_activeProfileIndex = INVALID_PROFILE_INDEX;
    if (g_lastLayoutId) {
        int idx = find_profile_by_id(g_lastLayoutId);
        if (idx >= 0) g_activeProfileIndex = (UINT)idx;
    }
    if (!g_boardConnected) board_try_connect();
    if (g_boardConnected) {
        board_send_config();
        board_send_night_mode();
        if (g_activeProfileIndex != INVALID_PROFILE_INDEX)
            board_send_profile_index(g_activeProfileIndex, FALSE);
    }
    update_tray_tooltip();
    return TRUE;

bad:
    MessageBoxW(wnd,
        L"Проверьте значения. Частота: 100..10000 Гц; длительность: 10..2000 мс; время: ЧЧ:ММ; проценты: 0..100.",
        L"LayoutBeep — неверные настройки", MB_OK | MB_ICONERROR);
    return FALSE;
}

static LRESULT CALLBACK profiles_settings_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT) { paint_profiles_settings(h); return 0; }
    if (msg == WM_COMMAND) {
        UINT id = LOWORD_(wp);
        UINT code = HIWORD_(wp);
        if (id == IDC_PROFILE_COMBO && code == CBN_SELCHANGE) {
            LRESULT sel;
            if (!capture_profile_editor(h)) {
                SendMessageW(GetDlgItem(h, (int)IDC_PROFILE_COMBO), CB_SETCURSEL, g_editProfileIndex, 0);
                MessageBoxW(h, L"Сначала исправьте частоту или длительность текущего профиля.", L"LayoutBeep", MB_OK | MB_ICONERROR);
                return 0;
            }
            sel = SendMessageW(GetDlgItem(h, (int)IDC_PROFILE_COMBO), CB_GETCURSEL, 0, 0);
            if (sel != CB_ERR && (UINT)sel < g_profileCount) {
                g_editProfileIndex = (UINT)sel;
                populate_profile_editor(h);
            }
            return 0;
        }
        if (id == IDC_PROFILE_COLOR && g_editProfileIndex < g_profileCount) {
            LAYOUT_PROFILE *p = &g_editProfiles[g_editProfileIndex];
            if (choose_rgb(h, &p->r, &p->g, &p->b)) profile_color_text(h, IDC_PROFILE_COLOR, p);
            return 0;
        }
        if (id == IDC_PROFILE_DEFAULT && g_editProfileIndex < g_profileCount) {
            set_default_profile_cue(g_editProfileIndex, &g_editProfiles[g_editProfileIndex]);
            populate_profile_editor(h);
            return 0;
        }
        if (id == IDC_FLASH_COLOR) {
            if (choose_rgb(h, &g_editCfg.flashR, &g_editCfg.flashG, &g_editCfg.flashB)) flash_color_text(h);
            return 0;
        }
        if (id == IDC_NIGHT_TOGGLE) {
            g_editCfg.nightEnabled = !g_editCfg.nightEnabled;
            update_profiles_night_text(h);
            return 0;
        }
        if (id == IDC_SAVE) {
            if (apply_profiles_settings(h)) DestroyWindow(h);
            return 0;
        }
        if (id == IDC_CANCEL) { DestroyWindow(h); return 0; }
    }
    if (msg == WM_CLOSE) { DestroyWindow(h); return 0; }
    if (msg == WM_DESTROY) { if (h == g_settingsHwnd) g_settingsHwnd = NULL; return 0; }
    return DefWindowProcW(h, msg, wp, lp);
}

static void open_profiles_settings(HWND owner) {
    HWND combo;
    UINT i;
    if (g_settingsHwnd && IsWindow(g_settingsHwnd)) { SetForegroundWindow(g_settingsHwnd); return; }
    refresh_layout_profiles(FALSE);
    g_editCfg = g_boardCfg;
    for (i = 0; i < g_profileCount; ++i) copy_profile(&g_editProfiles[i], &g_profiles[i]);
    g_editProfileIndex = g_activeProfileIndex < g_profileCount ? g_activeProfileIndex : 0u;

    g_settingsHwnd = CreateWindowExW(0, L"LayoutBeepProfilesSettingsWindow", L"LayoutBeep — настройки",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, 220, 70, 640, 650, owner, NULL, g_instance, NULL);
    if (!g_settingsHwnd) {
        MessageBoxW(owner, L"Не удалось открыть окно настроек.", L"LayoutBeep", MB_OK | MB_ICONERROR);
        return;
    }

    combo = create_child(g_settingsHwnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL,
                         38, 122, 538, 220, IDC_PROFILE_COMBO);
    for (i = 0; i < g_profileCount; ++i) {
        WCHAR label[160], idText[16];
        label[0] = 0; format_layout_id(g_profiles[i].id, idText, ARRAYSIZE(idText));
        wcat(label, ARRAYSIZE(label), g_profiles[i].name);
        wcat(label, ARRAYSIZE(label), L"  ["); wcat(label, ARRAYSIZE(label), idText); wcat(label, ARRAYSIZE(label), L"]");
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)label);
    }
    if (g_profileCount) SendMessageW(combo, CB_SETCURSEL, g_editProfileIndex, 0);

    create_child(g_settingsHwnd, L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 38, 186, 105, 30, IDC_PROFILE_HZ);
    create_child(g_settingsHwnd, L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 197, 186, 105, 30, IDC_PROFILE_MS);
    create_child(g_settingsHwnd, L"BUTTON", L"", 0, 356, 184, 220, 34, IDC_PROFILE_COLOR);
    create_child(g_settingsHwnd, L"BUTTON", L"Сбросить профиль", 0, 438, 94, 138, 28, IDC_PROFILE_DEFAULT);

    create_child(g_settingsHwnd, L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 38, 320, 105, 30, IDC_FLASH_MS);
    create_child(g_settingsHwnd, L"BUTTON", L"", 0, 197, 318, 260, 34, IDC_FLASH_COLOR);

    create_child(g_settingsHwnd, L"BUTTON", L"", 0, 458, 373, 118, 32, IDC_NIGHT_TOGGLE);
    create_child(g_settingsHwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 58, 434, 64, 30, IDC_NIGHT_FROM);
    create_child(g_settingsHwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 158, 434, 64, 30, IDC_NIGHT_TO);
    create_child(g_settingsHwnd, L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 238, 434, 78, 30, IDC_DAY_VOLUME);
    create_child(g_settingsHwnd, L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 370, 434, 78, 30, IDC_NIGHT_VOLUME);
    create_child(g_settingsHwnd, L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 38, 490, 78, 30, IDC_NIGHT_BRIGHT);

    create_child(g_settingsHwnd, L"BUTTON", L"Отмена", 0, 382, 552, 96, 36, IDC_CANCEL);
    create_child(g_settingsHwnd, L"BUTTON", L"Сохранить", 0, 488, 552, 116, 36, IDC_SAVE);

    if (g_profileCount) populate_profile_editor(g_settingsHwnd);
    set_edit_uint(g_settingsHwnd, IDC_FLASH_MS, g_editCfg.flashMs);
    flash_color_text(g_settingsHwnd);
    set_edit_time(g_settingsHwnd, IDC_NIGHT_FROM, g_editCfg.nightStartMin);
    set_edit_time(g_settingsHwnd, IDC_NIGHT_TO, g_editCfg.nightEndMin);
    set_edit_uint(g_settingsHwnd, IDC_DAY_VOLUME, g_editCfg.dayVolumePct);
    set_edit_uint(g_settingsHwnd, IDC_NIGHT_VOLUME, g_editCfg.nightVolumePct);
    set_edit_uint(g_settingsHwnd, IDC_NIGHT_BRIGHT, g_editCfg.nightBrightnessPct);
    update_profiles_night_text(g_settingsHwnd);
    ShowWindow(g_settingsHwnd, SW_SHOW);
    SetForegroundWindow(g_settingsHwnd);
}

static BOOL choose_profile_wav(HWND owner) {
    static const WCHAR filter[] = L"WAV files (*.wav)\0*.wav\0All files (*.*)\0*.*\0\0";
    OPENFILENAMEW ofn;
    UINT idx = g_activeProfileIndex;
    if (idx >= g_profileCount) {
        MessageBoxW(owner, L"Текущая раскладка не определена.", L"LayoutBeep", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    lb_memset(&ofn, 0, sizeof(ofn));
    wcopy(g_fileBuffer, ARRAYSIZE(g_fileBuffer), g_profileWav[idx]);
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = g_fileBuffer;
    ofn.nMaxFile = ARRAYSIZE(g_fileBuffer);
    ofn.lpstrTitle = L"Выберите WAV для текущей раскладки";
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"wav";
    if (!GetOpenFileNameW(&ofn)) return FALSE;
    if (!patch_ini_key(g_profiles[idx].section, L"Wav", g_fileBuffer)) {
        MessageBoxW(owner, L"Не удалось точечно изменить путь WAV в LayoutBeep.ini.",
                    L"LayoutBeep — ошибка записи INI", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    wcopy(g_profileWav[idx], ARRAYSIZE(g_profileWav[idx]), g_fileBuffer);
    play_profile_sound(idx);
    return TRUE;
}

static BOOL reset_profile_wav(HWND owner) {
    UINT idx = g_activeProfileIndex;
    if (idx >= g_profileCount) return FALSE;
    if (!g_profileWav[idx][0]) return TRUE;
    if (!patch_ini_key(g_profiles[idx].section, L"Wav", L"")) {
        MessageBoxW(owner, L"Не удалось точечно сбросить WAV текущей раскладки.",
                    L"LayoutBeep — ошибка записи INI", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    g_profileWav[idx][0] = 0;
    return TRUE;
}

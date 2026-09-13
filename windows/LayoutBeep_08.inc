    /* Preserve the existing newline convention. */
    {
        LPCWSTR nl = L"\r\n";
        DWORD scan = utf16 ? 2u : 0u, used = 0;
        while (scan < size) {
            if (ini_byte_char(g_iniScan, scan, utf16, size) == L'\n') {
                if (scan < step || ini_byte_char(g_iniScan, scan - step, utf16, size) != L'\r') nl = L"\n";
                break;
            }
            scan += step;
        }
        if (sectionFound) {
            if (!ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, key) ||
                !ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, L"=") ||
                !ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, value) ||
                !ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, nl)) {
                CloseHandle(f); return FALSE;
            }
            if (!ini_replace_range(f, size, sectionInsert, 0, g_iniEncoded, used)) { CloseHandle(f); return FALSE; }
        } else {
            BOOL needsNl = FALSE;
            if (size > (utf16 ? 2u : 0u)) {
                DWORD last = size - step;
                WCHAR lc = ini_byte_char(g_iniScan, last, utf16, size);
                needsNl = (lc != L'\n' && lc != L'\r');
            }
            if (needsNl && !ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, nl)) { CloseHandle(f); return FALSE; }
            if (!ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, L"[") ||
                !ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, section) ||
                !ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, L"]") ||
                !ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, nl) ||
                !ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, key) ||
                !ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, L"=") ||
                !ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, value) ||
                !ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, nl)) {
                CloseHandle(f); return FALSE;
            }
            if (!ini_replace_range(f, size, size, 0, g_iniEncoded, used)) { CloseHandle(f); return FALSE; }
        }
    }
    CloseHandle(f);
    return TRUE;
}

static DWORD ini_uint(LPCWSTR section, LPCWSTR key, DWORD def, DWORD minv, DWORD maxv) {
    WCHAR buf[32], dbuf[32];
    DWORD v;
    dbuf[0] = 0; append_uint_dec(dbuf, ARRAYSIZE(dbuf), def);
    GetPrivateProfileStringW(section, key, dbuf, buf, ARRAYSIZE(buf), g_configPath);
    if (!wparse_uint(buf, &v) || v < minv || v > maxv) return def;
    return v;
}

static BOOL patch_ini_uint(LPCWSTR section, LPCWSTR key, DWORD v) {
    WCHAR buf[32]; buf[0] = 0; append_uint_dec(buf, ARRAYSIZE(buf), v);
    return patch_ini_key(section, key, buf);
}

static void format_hhmm(DWORD minutes, LPWSTR out, UINT cap) {
    DWORD h = (minutes / 60u) % 24u;
    DWORD m = minutes % 60u;
    WCHAR buf[8];
    if (!out || !cap) return;
    buf[0] = (WCHAR)(L'0' + (h / 10u));
    buf[1] = (WCHAR)(L'0' + (h % 10u));
    buf[2] = L':';
    buf[3] = (WCHAR)(L'0' + (m / 10u));
    buf[4] = (WCHAR)(L'0' + (m % 10u));
    buf[5] = 0;
    wcopy(out, cap, buf);
}

static BOOL parse_hhmm(LPCWSTR s, DWORD *minutesOut) {
    DWORD h = 0, m = 0;
    UINT i = 0, digits = 0;
    if (!s || !minutesOut) return FALSE;
    while (s[i] == L' ' || s[i] == L'\t') ++i;
    while (s[i] >= L'0' && s[i] <= L'9' && digits < 2u) {
        h = h * 10u + (DWORD)(s[i] - L'0'); ++i; ++digits;
    }
    if (!digits || s[i] != L':') return FALSE;
    ++i; digits = 0;
    while (s[i] >= L'0' && s[i] <= L'9' && digits < 2u) {
        m = m * 10u + (DWORD)(s[i] - L'0'); ++i; ++digits;
    }
    if (digits != 2u) return FALSE;
    while (s[i] == L' ' || s[i] == L'\t') ++i;
    if (s[i] || h > 23u || m > 59u) return FALSE;
    *minutesOut = h * 60u + m;
    return TRUE;
}

static DWORD ini_time(LPCWSTR section, LPCWSTR key, DWORD def) {
    WCHAR defBuf[8], buf[32]; DWORD v;
    format_hhmm(def, defBuf, ARRAYSIZE(defBuf));
    GetPrivateProfileStringW(section, key, defBuf, buf, ARRAYSIZE(buf), g_configPath);
    if (!parse_hhmm(buf, &v)) return def;
    return v;
}

static BOOL patch_ini_time(LPCWSTR section, LPCWSTR key, DWORD minutes) {
    WCHAR buf[8]; format_hhmm(minutes, buf, ARRAYSIZE(buf));
    return patch_ini_key(section, key, buf);
}

static BOOL compute_night_active(void) {
    SYSTEMTIME st; DWORD nowMin, start, end;
    if (!g_boardCfg.nightEnabled) return FALSE;
    GetLocalTime(&st);
    nowMin = (DWORD)st.wHour * 60u + (DWORD)st.wMinute;
    start = g_boardCfg.nightStartMin % 1440u;
    end = g_boardCfg.nightEndMin % 1440u;
    if (start == end) return TRUE; /* enabled + equal endpoints means 24 h */
    if (start < end) return nowMin >= start && nowMin < end;
    return nowMin >= start || nowMin < end; /* crosses midnight */
}

static WCHAR wlower_ascii(WCHAR c) {
    if (c >= L'A' && c <= L'Z') return (WCHAR)(c + (L'a' - L'A'));
    return c;
}

static UINT wfind_ascii_ci(LPCWSTR text, LPCWSTR needle) {
    UINT i, j;
    if (!text || !needle || !needle[0]) return 0xffffffffu;
    for (i = 0; text[i]; ++i) {
        for (j = 0; needle[j] && text[i+j] && wlower_ascii(text[i+j]) == wlower_ascii(needle[j]); ++j) {}
        if (!needle[j]) return i;
    }
    return 0xffffffffu;
}

static UINT wfind_exact(LPCWSTR text, LPCWSTR needle) {
    UINT i, j;
    if (!text || !needle || !needle[0]) return 0xffffffffu;
    for (i = 0; text[i]; ++i) {
        for (j = 0; needle[j] && text[i+j] == needle[j]; ++j) {}
        if (!needle[j]) return i;
    }
    return 0xffffffffu;
}

static UINT minpos(UINT a, UINT b) { return a < b ? a : b; }

static WORD primary_from_indicator_text(LPCWSTR text) {
    UINT en = 0xffffffffu, ru = 0xffffffffu;
    if (!text || !text[0]) return 0;

    en = minpos(en, wfind_ascii_ci(text, L"english"));
    en = minpos(en, wfind_ascii_ci(text, L"eng"));
    en = minpos(en, wfind_exact(text, L"Англий"));
    en = minpos(en, wfind_exact(text, L"англий"));

    ru = minpos(ru, wfind_ascii_ci(text, L"russian"));
    ru = minpos(ru, wfind_ascii_ci(text, L"rus"));
    ru = minpos(ru, wfind_exact(text, L"Русск"));
    ru = minpos(ru, wfind_exact(text, L"русск"));
    ru = minpos(ru, wfind_exact(text, L"РУС"));

    if (en == 0xffffffffu && ru == 0xffffffffu) return 0;
    if (ru < en) return LANG_RUSSIAN;
    return LANG_ENGLISH;
}


static BOOL bytes_contains(const BYTE *buf, DWORD len, const char *needle) {
    DWORD i, j, n = 0;
    if (!buf || !needle) return FALSE;
    while (needle[n]) ++n;
    if (!n || n > len) return FALSE;
    for (i = 0; i + n <= len; ++i) {
        for (j = 0; j < n && buf[i+j] == (BYTE)needle[j]; ++j) {}
        if (j == n) return TRUE;
    }
    return FALSE;
}

static void board_close(void) {
    if (g_boardHandle != INVALID_HANDLE_VALUE) CloseHandle(g_boardHandle);
    g_boardHandle = INVALID_HANDLE_VALUE;
    g_boardConnected = FALSE;
    g_boardPort[0] = 0;
}

static BOOL board_write_ascii(const char *text) {
    DWORD len = 0, written = 0;
    if (!g_boardConnected || g_boardHandle == INVALID_HANDLE_VALUE || !text) return FALSE;
    while (text[len]) ++len;
    if (!WriteFile(g_boardHandle, text, len, &written, NULL) || written != len) {
        g_boardTxErrors++;
        board_close();
        return FALSE;
    }
    return TRUE;
}


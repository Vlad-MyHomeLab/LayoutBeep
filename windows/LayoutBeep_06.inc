static void request_secure_support_toggle(HWND owner, BOOL enable) {
    DWORD ec = 0xffffffffu;
    if (enable) {
        /* v2.4 appended itself to Configuration but did not notify Ease of Access that
           the already-running normal instance was an AT. Clean that stale mechanism. */
        remove_at_configuration_item(AT_OLD_KEY_NAME);
        remove_at_configuration_item(AT_KEY_NAME);
        if (!run_elevated_and_wait(owner, L"/secure-install", &ec)) {
            MessageBoxW(owner,
                L"Windows не запустила установку с правами администратора. Если запрос UAC был отменён, повторите действие.",
                L"LayoutBeep — экран блокировки", MB_OK | MB_ICONERROR);
            return;
        }
        if (ec != 0u || !secure_support_installed()) {
            MessageBoxW(owner, L"Не удалось зарегистрировать помощник экрана блокировки.",
                        L"LayoutBeep — экран блокировки", MB_OK | MB_ICONERROR);
            return;
        }
        g_lockSupportReady = notify_ease_of_access(TRUE);
        if (g_lockSupportReady) {
            MessageBoxW(owner,
                L"Поддержка экрана блокировки включена. Windows получила уведомление о запущенном LayoutBeep. Теперь можно проверить Win+L.",
                L"LayoutBeep", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxW(owner,
                L"Регистрация выполнена, но уведомить Ease of Access Center не удалось. USB на блокировке отдаваться не будет, поэтому плата не должна уходить в offline.",
                L"LayoutBeep — экран блокировки", MB_OK | MB_ICONERROR);
        }
    } else {
        notify_ease_of_access(FALSE);
        if (!run_elevated_and_wait(owner, L"/secure-remove", &ec)) {
            MessageBoxW(owner, L"Не удалось запросить права администратора для удаления поддержки.",
                        L"LayoutBeep — экран блокировки", MB_OK | MB_ICONERROR);
            return;
        }
        remove_at_configuration_item(AT_OLD_KEY_NAME);
        remove_at_configuration_item(AT_KEY_NAME);
        g_lockSupportReady = FALSE;
        MessageBoxW(owner, ec == 0u ? L"Поддержка экрана блокировки отключена." : L"Удалить регистрацию полностью не удалось.",
                    L"LayoutBeep", MB_OK | (ec == 0u ? MB_ICONINFORMATION : MB_ICONERROR));
    }
}

static WORD secure_foreground_primary(void) {
    HWND fg = GetForegroundWindow();
    DWORD tid;
    ULONG_PTR hkl;
    if (!fg) return 0;
    tid = GetWindowThreadProcessId(fg, NULL);
    if (!tid) return 0;
    hkl = (ULONG_PTR)GetKeyboardLayout(tid);
    if (!hkl) return 0;
    return PRIMARYLANGID((WORD)(hkl & 0xffffu));
}

static void check_secure_layout(BOOL play) {
    WORD primary = secure_foreground_primary();
    if (primary != LANG_ENGLISH && primary != LANG_RUSSIAN) {
        g_secureCandidatePrimary = 0; g_secureCandidateCount = 0; return;
    }
    if (!g_secureLastPrimary) {
        g_secureLastPrimary = primary;
        g_lastPrimary = primary;
        if (!g_boardConnected) board_try_connect();
        if (g_boardConnected) board_send_layout(primary, FALSE);
        return;
    }
    if (primary == g_secureLastPrimary) {
        g_secureCandidatePrimary = 0; g_secureCandidateCount = 0; return;
    }
    if (primary != g_secureCandidatePrimary) {
        g_secureCandidatePrimary = primary; g_secureCandidateCount = 1u; return;
    }
    if (++g_secureCandidateCount < 2u) return;
    g_secureLastPrimary = primary;
    g_secureCandidatePrimary = 0; g_secureCandidateCount = 0;
    accept_language_primary(primary, play, 9u);
}

static void set_default_board_config(BOARD_CONFIG *c) {
    if (!c) return;
    c->engHz = 1450u; c->engMs = 75u; c->engR = 0u; c->engG = 0u; c->engB = 28u;
    c->rusHz = 850u;  c->rusMs = 75u; c->rusR = 28u; c->rusG = 0u; c->rusB = 0u;
    c->flashMs = 80u; c->flashR = 90u; c->flashG = 90u; c->flashB = 90u;
    c->nightEnabled = FALSE;
    c->nightStartMin = 23u * 60u;
    c->nightEndMin = 7u * 60u;
    c->dayVolumePct = 100u;
    c->nightVolumePct = 25u;
    c->nightBrightnessPct = 20u;
}

static WCHAR ini_byte_char(const BYTE *buf, DWORD pos, BOOL utf16, DWORD size) {
    if (utf16) {
        if (pos + 1u >= size) return 0;
        return (WCHAR)((WORD)buf[pos] | ((WORD)buf[pos + 1u] << 8));
    }
    if (pos >= size) return 0;
    return (WCHAR)buf[pos];
}

static BOOL ini_token_eq(const BYTE *buf, DWORD start, DWORD end, BOOL utf16, LPCWSTR token) {
    DWORD step = utf16 ? 2u : 1u;
    UINT ti = 0;
    WCHAR c;
    while (start < end) {
        c = ini_byte_char(buf, start, utf16, end);
        if (c != L' ' && c != L'\t') break;
        start += step;
    }
    while (end > start) {
        DWORD p = end - step;
        c = ini_byte_char(buf, p, utf16, end);
        if (c != L' ' && c != L'\t') break;
        end -= step;
    }
    while (start < end && token[ti]) {
        c = ini_byte_char(buf, start, utf16, end);
        if (wlower_ascii(c) != wlower_ascii(token[ti])) return FALSE;
        start += step; ++ti;
    }
    return start == end && token[ti] == 0;
}

static BOOL ini_encode_wide(LPCWSTR text, BOOL utf16, BYTE *out, DWORD cap, DWORD *bytesOut) {
    DWORD i, n;
    if (!text || !out || !bytesOut) return FALSE;
    n = wlen(text);
    if (utf16) {
        if (n > cap / 2u) return FALSE;
        for (i = 0; i < n; ++i) {
            WORD c = (WORD)text[i];
            out[i * 2u] = (BYTE)(c & 0xffu);
            out[i * 2u + 1u] = (BYTE)((c >> 8) & 0xffu);
        }
        *bytesOut = n * 2u;
        return TRUE;
    }
    if (!n) { *bytesOut = 0; return TRUE; }
    {
        int got = WideCharToMultiByte(CP_ACP, 0, text, (int)n, (char*)out, (int)cap, NULL, NULL);
        if (got <= 0) return FALSE;
        *bytesOut = (DWORD)got;
        return TRUE;
    }
}

static BOOL ini_append_wide(BYTE *dst, DWORD cap, DWORD *used, BOOL utf16, LPCWSTR text) {
    DWORD n = 0;
    if (*used > cap) return FALSE;
    if (!ini_encode_wide(text, utf16, dst + *used, cap - *used, &n)) return FALSE;
    *used += n;
    return TRUE;
}

static BOOL ini_seek(HANDLE f, DWORD pos) {
    DWORD r = SetFilePointer(f, (LONG)pos, NULL, FILE_BEGIN);
    return r != INVALID_SET_FILE_POINTER || GetLastError() == ERROR_SUCCESS;
}

static BOOL ini_resize(HANDLE f, DWORD size) {
    if (!ini_seek(f, size)) return FALSE;
    return SetEndOfFile(f);
}

/* Replace one byte range IN PLACE. The file object itself is never replaced.
   Bytes outside [start,start+oldLen) remain byte-for-byte identical. */
static BOOL ini_replace_range(HANDLE f, DWORD oldSize, DWORD start, DWORD oldLen,
                              const BYTE *newData, DWORD newLen) {
    DWORD tailStart, tailLen, done, chunk, rd, wr;
    LONG delta = (LONG)newLen - (LONG)oldLen;
    if (start > oldSize || oldLen > oldSize - start) return FALSE;
    tailStart = start + oldLen;
    tailLen = oldSize - tailStart;


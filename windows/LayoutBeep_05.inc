static BOOL remove_at_configuration_item(LPCWSTR item) {
    HKEY h;
    DWORD disp = 0, type = 0, cb;
    WCHAR oldv[512], newv[512];
    UINT i = 0, start, len, j;
    BOOL first = TRUE;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, AT_CONFIG_PATH, 0, NULL, 0,
                        KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &h, &disp) != ERROR_SUCCESS) return FALSE;
    oldv[0] = 0; cb = sizeof(oldv);
    if (RegQueryValueExW(h, L"Configuration", NULL, &type, (LPBYTE)oldv, &cb) != ERROR_SUCCESS) {
        RegCloseKey(h); return TRUE;
    }
    oldv[ARRAYSIZE(oldv)-1u] = 0;
    newv[0] = 0;
    while (oldv[i]) {
        while (oldv[i] == L' ' || oldv[i] == L',' || oldv[i] == L'\t') ++i;
        start = i;
        while (oldv[i] && oldv[i] != L',') ++i;
        len = i - start;
        while (len && (oldv[start+len-1u] == L' ' || oldv[start+len-1u] == L'\t')) --len;
        if (len) {
            BOOL same = (len == wlen(item));
            if (same) {
                for (j=0; j<len; ++j) if (wlower_ascii(oldv[start+j]) != wlower_ascii(item[j])) { same=FALSE; break; }
            }
            if (!same) {
                WCHAR token[256];
                if (!first) wcat(newv, ARRAYSIZE(newv), L",");
                wcopy_n(token, ARRAYSIZE(token), oldv+start, len);
                wcat(newv, ARRAYSIZE(newv), token);
                first = FALSE;
            }
        }
        if (oldv[i] == L',') ++i;
    }
    type = RegSetValueExW(h, L"Configuration", 0, REG_SZ, (const BYTE*)newv,
                          (wlen(newv)+1u)*(DWORD)sizeof(WCHAR));
    RegCloseKey(h);
    return type == ERROR_SUCCESS;
}

static BOOL secure_support_installed(void) {
    HKEY h;
    DWORD type=0, cb;
    WCHAR params[64];
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, AT_KEY_PATH, 0, KEY_QUERY_VALUE, &h) != ERROR_SUCCESS) return FALSE;
    g_regValue[0]=0; cb=sizeof(g_regValue);
    if (RegQueryValueExW(h, L"StartExe", NULL, &type, (LPBYTE)g_regValue, &cb) != ERROR_SUCCESS || !weq(g_regValue, g_exePath)) {
        RegCloseKey(h); return FALSE;
    }
    params[0]=0; cb=sizeof(params);
    if (RegQueryValueExW(h, L"StartParams", NULL, &type, (LPBYTE)params, &cb) != ERROR_SUCCESS || !weq(params, L"/secure")) {
        RegCloseKey(h); return FALSE;
    }
    RegCloseKey(h);
    return TRUE;
}

static BOOL install_secure_support_elevated(void) {
    HKEY h;
    DWORD disp=0;
    static const WCHAR profile[] = L"<HCIModel><Accommodation type=\"mild vision\"/><Accommodation type=\"mild hearing\"/></HCIModel>";
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, AT_OLD_KEY_PATH);
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, AT_KEY_PATH, 0, NULL, 0,
                        KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &h, &disp) != ERROR_SUCCESS) return FALSE;
    if (!reg_set_sz(h, L"ApplicationName", L"LayoutBeep") ||
        !reg_set_sz(h, L"Description", L"Keyboard layout sound and color indicator") ||
        !reg_set_sz(h, L"Profile", profile) ||
        !reg_set_sz(h, L"SimpleProfile", L"Keyboard layout indicator") ||
        !reg_set_sz(h, L"ATExe", L"LayoutBeep.exe") ||
        !reg_set_sz(h, L"StartExe", g_exePath) ||
        !reg_set_sz(h, L"StartParams", L"/secure") ||
        !reg_set_dword(h, L"TerminateOnDesktopSwitch", 0u) ||
        !reg_set_dword(h, L"CopySettingsToLockedDesktop", 0u)) {
        RegCloseKey(h); return FALSE;
    }
    RegCloseKey(h);
    return TRUE;
}

static BOOL remove_secure_support_elevated(void) {
    BOOL ok = TRUE;
    LSTATUS st = RegDeleteKeyW(HKEY_LOCAL_MACHINE, AT_KEY_PATH);
    if (st != ERROR_SUCCESS && st != ERROR_FILE_NOT_FOUND) ok = FALSE;
    st = RegDeleteKeyW(HKEY_LOCAL_MACHINE, AT_OLD_KEY_PATH);
    if (st != ERROR_SUCCESS && st != ERROR_FILE_NOT_FOUND) ok = FALSE;
    return ok;
}

static BOOL mutex_exists(LPCWSTR name) {
    HANDLE h = OpenMutexW(SYNCHRONIZE_, FALSE, name);
    if (!h) return FALSE;
    CloseHandle(h);
    return TRUE;
}

static BOOL secure_helper_running(void) {
    return mutex_exists(L"Local\\LayoutBeep.Singleton.Secure");
}

static BOOL notify_ease_of_access(BOOL starting) {
    HKEY h;
    DWORD disp=0, value = starting ? 0x0003u : 0x0002u;
    INPUT in[4];
    BOOL regOk;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, AT_TEMP_PATH, 0, NULL, 0,
                        KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &h, &disp) != ERROR_SUCCESS) return FALSE;
    regOk = RegSetValueExW(h, AT_KEY_NAME, 0, REG_DWORD, (const BYTE*)&value, sizeof(value)) == ERROR_SUCCESS;
    RegCloseKey(h);
    if (!regOk) return FALSE;

    lb_memset(in, 0, sizeof(in));
    in[0].type = INPUT_KEYBOARD; in[0].DUMMYUNIONNAME.ki.wVk = VK_LWIN;
    in[1].type = INPUT_KEYBOARD; in[1].DUMMYUNIONNAME.ki.wVk = (WORD)L'U';
    in[2].type = INPUT_KEYBOARD; in[2].DUMMYUNIONNAME.ki.wVk = (WORD)L'U'; in[2].DUMMYUNIONNAME.ki.dwFlags = KEYEVENTF_KEYUP;
    in[3].type = INPUT_KEYBOARD; in[3].DUMMYUNIONNAME.ki.wVk = VK_LWIN; in[3].DUMMYUNIONNAME.ki.dwFlags = KEYEVENTF_KEYUP;
    if (SendInput(4u, in, sizeof(INPUT)) != 4u) return FALSE;
    Sleep(100u);
    return TRUE;
}

static BOOL run_elevated_and_wait(HWND owner, LPCWSTR params, DWORD *exitCode) {
    SHELLEXECUTEINFOW se;
    DWORD ec = 0xffffffffu;
    lb_memset(&se, 0, sizeof(se));
    se.cbSize = sizeof(se);
    se.fMask = SEE_MASK_NOCLOSEPROCESS;
    se.hwnd = owner;
    se.lpVerb = L"runas";
    se.lpFile = g_exePath;
    se.lpParameters = params;
    se.lpDirectory = g_exeDir;
    se.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&se)) return FALSE;
    if (!se.hProcess) return FALSE;
    WaitForSingleObject(se.hProcess, INFINITE);
    GetExitCodeProcess(se.hProcess, &ec);
    CloseHandle(se.hProcess);
    if (exitCode) *exitCode = ec;
    return TRUE;
}


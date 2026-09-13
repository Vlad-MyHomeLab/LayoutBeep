    if (delta > 0) {
        if (!ini_resize(f, oldSize + (DWORD)delta)) return FALSE;
        done = tailLen;
        while (done) {
            chunk = done > INI_MOVE_CHUNK ? INI_MOVE_CHUNK : done;
            done -= chunk;
            if (!ini_seek(f, tailStart + done)) return FALSE;
            rd = 0; if (!ReadFile(f, g_iniMove, chunk, &rd, NULL) || rd != chunk) return FALSE;
            if (!ini_seek(f, tailStart + done + (DWORD)delta)) return FALSE;
            wr = 0; if (!WriteFile(f, g_iniMove, chunk, &wr, NULL) || wr != chunk) return FALSE;
        }
    } else if (delta < 0) {
        DWORD shrink = (DWORD)(-delta);
        done = 0;
        while (done < tailLen) {
            chunk = (tailLen - done) > INI_MOVE_CHUNK ? INI_MOVE_CHUNK : (tailLen - done);
            if (!ini_seek(f, tailStart + done)) return FALSE;
            rd = 0; if (!ReadFile(f, g_iniMove, chunk, &rd, NULL) || rd != chunk) return FALSE;
            if (!ini_seek(f, tailStart - shrink + done)) return FALSE;
            wr = 0; if (!WriteFile(f, g_iniMove, chunk, &wr, NULL) || wr != chunk) return FALSE;
            done += chunk;
        }
        if (!ini_resize(f, oldSize - shrink)) return FALSE;
    }

    if (newLen) {
        if (!ini_seek(f, start)) return FALSE;
        wr = 0; if (!WriteFile(f, newData, newLen, &wr, NULL) || wr != newLen) return FALSE;
    }
    return TRUE;
}

static BOOL ini_create_minimal(LPCWSTR section, LPCWSTR key, LPCWSTR value) {
    HANDLE f;
    DWORD used = 0, wr = 0;
    BOOL utf16 = TRUE; /* New files are Unicode; existing files keep their encoding. */
    f = CreateFileW(g_configPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    g_iniEncoded[used++] = 0xffu; g_iniEncoded[used++] = 0xfeu;
    if (!ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, L"[") ||
        !ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, section) ||
        !ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, L"]\r\n") ||
        !ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, key) ||
        !ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, L"=") ||
        !ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, value) ||
        !ini_append_wide(g_iniEncoded, sizeof(g_iniEncoded), &used, utf16, L"\r\n")) {
        CloseHandle(f); return FALSE;
    }
    if (!WriteFile(f, g_iniEncoded, used, &wr, NULL) || wr != used) { CloseHandle(f); return FALSE; }
    CloseHandle(f);
    return TRUE;
}

/* Surgical key writer:
   - checks that the INI exists before opening it for update;
   - never regenerates/replaces an existing file;
   - if key exists, replaces only its VALUE bytes;
   - if key/section is absent, inserts only the minimal missing line/section;
   - all unrelated bytes/comments/order are preserved exactly. */
static BOOL patch_ini_key(LPCWSTR section, LPCWSTR key, LPCWSTR value) {
    HANDLE f;
    DWORD size, rd = 0, pos, lineStart, lineEnd, contentEnd, nextLine;
    DWORD step, keyValueStart = 0, keyValueEnd = 0, sectionInsert = 0;
    BOOL utf16, inSection = FALSE, sectionFound = FALSE, keyFound = FALSE;
    WCHAR c;

    if (GetFileAttributesW(g_configPath) == INVALID_FILE_ATTRIBUTES)
        return ini_create_minimal(section, key, value);

    f = CreateFileW(g_configPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    size = GetFileSize(f, NULL);
    if (size == 0xffffffffu || size > INI_SCAN_CAP) { CloseHandle(f); return FALSE; }
    if (size) {
        if (!ini_seek(f, 0)) { CloseHandle(f); return FALSE; }
        if (!ReadFile(f, g_iniScan, size, &rd, NULL) || rd != size) { CloseHandle(f); return FALSE; }
    }

    utf16 = size >= 2u && g_iniScan[0] == 0xffu && g_iniScan[1] == 0xfeu;
    step = utf16 ? 2u : 1u;
    pos = utf16 ? 2u : 0u;
    sectionInsert = size;

    while (pos < size) {
        DWORD a, b, eq = 0xffffffffu;
        lineStart = pos;
        lineEnd = size;
        while (pos < size) {
            c = ini_byte_char(g_iniScan, pos, utf16, size);
            if (c == L'\n') { lineEnd = pos; pos += step; break; }
            pos += step;
        }
        nextLine = pos;
        contentEnd = lineEnd;
        if (contentEnd >= lineStart + step && ini_byte_char(g_iniScan, contentEnd - step, utf16, size) == L'\r')
            contentEnd -= step;
        a = lineStart; b = contentEnd;
        while (a < b) { c = ini_byte_char(g_iniScan, a, utf16, size); if (c != L' ' && c != L'\t') break; a += step; }
        while (b > a) { c = ini_byte_char(g_iniScan, b - step, utf16, size); if (c != L' ' && c != L'\t') break; b -= step; }

        if (a < b && ini_byte_char(g_iniScan, a, utf16, size) == L'[' &&
            b >= a + 2u * step && ini_byte_char(g_iniScan, b - step, utf16, size) == L']') {
            if (inSection) { sectionInsert = lineStart; break; }
            inSection = ini_token_eq(g_iniScan, a + step, b - step, utf16, section);
            if (inSection) { sectionFound = TRUE; sectionInsert = nextLine; }
            continue;
        }

        if (!inSection) continue;
        sectionInsert = nextLine;
        {
            DWORD q = a;
            while (q < b) {
                if (ini_byte_char(g_iniScan, q, utf16, size) == L'=') { eq = q; break; }
                q += step;
            }
        }
        if (eq == 0xffffffffu) continue;
        if (ini_token_eq(g_iniScan, a, eq, utf16, key)) {
            DWORD vs = eq + step, ve = b;
            while (vs < ve) { c = ini_byte_char(g_iniScan, vs, utf16, size); if (c != L' ' && c != L'\t') break; vs += step; }
            while (ve > vs) { c = ini_byte_char(g_iniScan, ve - step, utf16, size); if (c != L' ' && c != L'\t') break; ve -= step; }
            keyValueStart = vs; keyValueEnd = ve; keyFound = TRUE; break;
        }
    }

    if (keyFound) {
        DWORD n = 0;
        if (!ini_encode_wide(value, utf16, g_iniEncoded, sizeof(g_iniEncoded), &n)) { CloseHandle(f); return FALSE; }
        if (!ini_replace_range(f, size, keyValueStart, keyValueEnd - keyValueStart, g_iniEncoded, n)) { CloseHandle(f); return FALSE; }
        CloseHandle(f); return TRUE;
    }


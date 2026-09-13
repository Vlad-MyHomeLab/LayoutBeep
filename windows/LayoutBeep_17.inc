    inst = (HINSTANCE)GetModuleHandleW(NULL);
    g_instance = inst;
    g_icon = create_app_icon();
    if (!g_icon) g_icon = LoadIconW(NULL, IDI_APPLICATION);
    g_uiFont = CreateFontW(-16, 0, 0, 0, 400, 0, 0, 0, 1, 0, 0, 5, 0, L"Segoe UI");
    g_titleFont = CreateFontW(-24, 0, 0, 0, 600, 0, 0, 0, 1, 0, 0, 5, 0, L"Segoe UI");
    g_sectionFont = CreateFontW(-18, 0, 0, 0, 600, 0, 0, 0, 1, 0, 0, 5, 0, L"Segoe UI");
    g_smallFont = CreateFontW(-14, 0, 0, 0, 400, 0, 0, 0, 1, 0, 0, 5, 0, L"Segoe UI");
    lb_memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = window_proc;
    wc.hInstance = inst;
    wc.hIcon = g_icon;
    wc.lpszClassName = L"LayoutBeepHiddenWindow";
    if (!RegisterClassW(&wc)) {
        MessageBoxW(NULL, L"Не удалось зарегистрировать окно LayoutBeep.", L"LayoutBeep", MB_OK | MB_ICONERROR);
        ExitProcess(2);
    }
    lb_memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = settings_proc;
    wc.hInstance = inst;
    wc.hIcon = g_icon;
    wc.lpszClassName = L"LayoutBeepSettingsWindow";
    RegisterClassW(&wc);

    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    g_hwnd = CreateWindowExW(0, L"LayoutBeepHiddenWindow", L"LayoutBeep", 0,
                             0, 0, 0, 0, NULL, NULL, inst, NULL);
    if (!g_hwnd) {
        MessageBoxW(NULL, L"Не удалось создать окно LayoutBeep.", L"LayoutBeep", MB_OK | MB_ICONERROR);
        ExitProcess(3);
    }

    { HRESULT chr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED); g_comInitialized = SUCCEEDED(chr); }
    g_desktopActive = current_desktop_is_input();
    if (g_secureMode) {
        /* Runtime/secure diagnostics must never modify the user's INI file. */
        check_secure_layout(FALSE);
        if (g_desktopActive) board_try_connect();
    } else {
        /* Clean the v2.4 Configuration entry. For TerminateOnDesktopSwitch=0 Microsoft
           requires an externally-started AT to notify Ease of Access Center explicitly. */
        remove_at_configuration_item(AT_OLD_KEY_NAME);
        remove_at_configuration_item(AT_KEY_NAME);
        check_tray_indicator(FALSE);
        if (g_desktopActive) board_try_connect();
        add_tray_icon();
        if (secure_support_installed()) g_lockSupportReady = notify_ease_of_access(TRUE);
    }
    /* 50 ms layout polling; night/day schedule is evaluated once per minute. */
    SetTimer(g_hwnd, 1, 50, NULL);

    lb_memset(&msg, 0, sizeof(msg));
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    board_close();
    remove_tray_icon();
    if (g_comInitialized) CoUninitialize();
    if (g_uiFont) DeleteObject(g_uiFont);
    if (g_titleFont) DeleteObject(g_titleFont);
    if (g_sectionFont) DeleteObject(g_sectionFont);
    if (g_smallFont) DeleteObject(g_smallFont);
    if (g_icon) DestroyIcon(g_icon);
    ExitProcess(0);
}

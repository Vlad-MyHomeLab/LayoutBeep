static void paint_round_box(HDC dc, int x, int y, int w, int h, int radius, COLORREF fill, COLORREF border) {
    HBRUSH br = CreateSolidBrush(fill);
    HPEN pen = CreatePen(0, 1, border);
    HANDLE oldBr = SelectObject(dc, br);
    HANDLE oldPen = SelectObject(dc, pen);
    RoundRect(dc, x, y, x+w, y+h, radius, radius);
    SelectObject(dc, oldBr);
    SelectObject(dc, oldPen);
    DeleteObject(br); DeleteObject(pen);
}

static void paint_text(HDC dc, LPCWSTR text, RECT r, HFONT font, COLORREF color, UINT flags) {
    HANDLE old = NULL;
    if (font) old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &r, flags | DT_NOPREFIX);
    if (old) SelectObject(dc, old);
}

static void paint_settings_background(HWND h) {
    PAINTSTRUCT ps; HDC dc; RECT client;
    COLORREF bg = RGB_(246,248,251), card = RGB_(255,255,255), border = RGB_(224,229,236);
    dc = BeginPaint(h, &ps);
    GetClientRect(h, &client);
    { HBRUSH br=CreateSolidBrush(bg); FillRect(dc,&client,br); DeleteObject(br); }

    paint_text(dc, L"Настройки RP2040", rect_xywh(24,18,300,30), g_titleFont, RGB_(25,32,44), DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc, L"Звук, цвет и ночной профиль", rect_xywh(24,48,360,22), g_smallFont, RGB_(105,113,126), DT_LEFT|DT_SINGLELINE|DT_VCENTER);

    paint_round_box(dc,20,82,584,112,12,card,border);
    paint_round_box(dc,20,204,584,112,12,card,border);
    paint_round_box(dc,20,326,584,94,12,card,border);
    paint_round_box(dc,20,430,584,174,12,card,border);

    { HBRUSH b=CreateSolidBrush(RGB_(43,112,238)); RECT d=rect_xywh(36,99,10,10); FillRect(dc,&d,b); DeleteObject(b); }
    paint_text(dc,L"English",rect_xywh(54,92,120,24),g_sectionFont,RGB_(29,36,48),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    { HBRUSH b=CreateSolidBrush(RGB_(226,58,66)); RECT d=rect_xywh(36,221,10,10); FillRect(dc,&d,b); DeleteObject(b); }
    paint_text(dc,L"Русский",rect_xywh(54,214,120,24),g_sectionFont,RGB_(29,36,48),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    { HBRUSH b=CreateSolidBrush(RGB_(238,169,36)); RECT d=rect_xywh(36,343,10,10); FillRect(dc,&d,b); DeleteObject(b); }
    paint_text(dc,L"Вспышка при переключении",rect_xywh(54,336,250,24),g_sectionFont,RGB_(29,36,48),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    { HBRUSH b=CreateSolidBrush(RGB_(126,87,194)); RECT d=rect_xywh(36,447,10,10); FillRect(dc,&d,b); DeleteObject(b); }
    paint_text(dc,L"Ночной режим",rect_xywh(54,440,180,24),g_sectionFont,RGB_(29,36,48),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"В заданное время применяет отдельные громкость и яркость",rect_xywh(38,468,420,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);

    paint_text(dc,L"Частота",rect_xywh(38,124,90,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Гц",rect_xywh(153,151,30,24),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Длительность",rect_xywh(197,124,105,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"мс",rect_xywh(312,151,30,24),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Цвет",rect_xywh(356,124,80,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);

    paint_text(dc,L"Частота",rect_xywh(38,246,90,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Гц",rect_xywh(153,273,30,24),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Длительность",rect_xywh(197,246,105,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"мс",rect_xywh(312,273,30,24),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Цвет",rect_xywh(356,246,80,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);

    paint_text(dc,L"Длительность",rect_xywh(38,368,105,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"мс",rect_xywh(153,395,30,20),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Цвет",rect_xywh(197,368,80,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);

    paint_text(dc,L"С",rect_xywh(38,492,20,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"до",rect_xywh(132,492,24,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Громкость днём",rect_xywh(238,492,118,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"%",rect_xywh(324,520,20,20),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Громкость ночью",rect_xywh(370,492,122,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"%",rect_xywh(456,520,20,20),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Яркость ночью",rect_xywh(38,548,110,18),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"%",rect_xywh(124,576,20,20),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    paint_text(dc,L"Оранжевый offline-индикатор не приглушается",rect_xywh(166,572,330,22),g_smallFont,RGB_(100,109,123),DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    EndPaint(h,&ps);
}

static COLORREF button_rgb_for_id(UINT id) {
    if (id==IDC_ENG_COLOR) return RGB_(g_editCfg.engR,g_editCfg.engG,g_editCfg.engB);
    if (id==IDC_RUS_COLOR) return RGB_(g_editCfg.rusR,g_editCfg.rusG,g_editCfg.rusB);
    return RGB_(g_editCfg.flashR,g_editCfg.flashG,g_editCfg.flashB);
}

static void draw_owner_button(const DRAWITEMSTRUCT *di) {
    RECT r=di->rcItem, tr; COLORREF fill=RGB_(255,255,255), border=RGB_(211,217,225), text=RGB_(38,45,56);
    WCHAR label[128]; label[0]=0;
    if (di->CtlID==IDC_SAVE) { fill=RGB_(37,99,235); border=RGB_(37,99,235); text=RGB_(255,255,255); }
    else if (di->CtlID==IDC_NIGHT_TOGGLE) {
        if (g_editCfg.nightEnabled) { fill=RGB_(126,87,194); border=RGB_(126,87,194); text=RGB_(255,255,255); }
        else { fill=RGB_(244,246,249); border=RGB_(211,217,225); text=RGB_(82,91,105); }
    }
    else if (di->CtlID==IDC_ENG_COLOR || di->CtlID==IDC_RUS_COLOR || di->CtlID==IDC_FLASH_COLOR) { fill=RGB_(249,250,252); }
    if (di->itemState & ODS_SELECTED) {
        if (di->CtlID==IDC_SAVE) fill=RGB_(29,78,216);
        else if (di->CtlID==IDC_NIGHT_TOGGLE && g_editCfg.nightEnabled) fill=RGB_(104,72,168);
        else fill=RGB_(238,241,245);
    }
    paint_round_box(di->hDC,r.left,r.top,r.right-r.left,r.bottom-r.top,8,fill,border);
    GetWindowTextW(di->hwndItem,label,ARRAYSIZE(label));
    if (di->CtlID==IDC_ENG_COLOR || di->CtlID==IDC_RUS_COLOR || di->CtlID==IDC_FLASH_COLOR) {
        COLORREF c=button_rgb_for_id(di->CtlID);
        paint_round_box(di->hDC,r.left+8,r.top+7,26,(r.bottom-r.top)-14,6,c,RGB_(210,215,222));
        tr=r; tr.left+=44; tr.right-=8;
        paint_text(di->hDC,label,tr,g_uiFont,text,DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    } else {
        tr=r;
        paint_text(di->hDC,label,tr,g_uiFont,text,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
    }
}

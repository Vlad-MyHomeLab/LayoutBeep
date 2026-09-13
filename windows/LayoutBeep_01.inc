#define WIN32_LEAN_AND_MEAN

/*
 * LayoutBeep - tiny native Win32 layout-change sound notifier.
 * No CRT, no .NET, no installer. Windows 10/11 x64.
 * v2.7.3: configurable RP2040 cues plus scheduled night mode. Night mode
 *        reduces passive-piezo volume and LED/flash brightness during a
 *        user-defined local-time interval. Settings live next to the EXE
 *        in LayoutBeep.ini and are re-sent whenever the RP2040 reconnects.
 */

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int UINT;
typedef unsigned int DWORD;
typedef int BOOL;
typedef int LONG;
typedef long long LONG_PTR;
typedef unsigned long long ULONG_PTR;
typedef ULONG_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;
typedef void *HANDLE;
typedef HANDLE HWND;
typedef HANDLE HINSTANCE;
typedef HANDLE HICON;
typedef HANDLE HMENU;
typedef HANDLE HBRUSH;
typedef HANDLE HFONT;
typedef HANDLE HDC;
typedef HANDLE HPEN;
typedef HANDLE HCURSOR;
typedef HANDLE HKEY;
typedef HANDLE HMODULE;
typedef HANDLE HHOOK;
typedef void *FARPROC;
typedef void *LPVOID;
typedef const void *LPCVOID;
typedef unsigned short WCHAR;
typedef WCHAR *LPWSTR;
typedef const WCHAR *LPCWSTR;
typedef BYTE *LPBYTE;
typedef DWORD *LPDWORD;
typedef DWORD COLORREF;
typedef LONG LSTATUS;
typedef LONG HRESULT;
typedef WCHAR *BSTR;
typedef LRESULT (__stdcall *WNDPROC)(HWND, UINT, WPARAM, LPARAM);

#define WINAPI __stdcall
#define CALLBACK __stdcall
#define TRUE 1
#define FALSE 0
#define NULL ((void*)0)

#define WM_NULL          0x0000u
#define WM_DESTROY       0x0002u
#define WM_CLOSE         0x0010u
#define WM_PAINT         0x000Fu
#define WM_ERASEBKGND    0x0014u
#define WM_DRAWITEM      0x002Bu
#define WM_SETFONT       0x0030u
#define WM_COMMAND       0x0111u
#define WM_TIMER         0x0113u
#define WM_RBUTTONUP     0x0205u
#define WM_LBUTTONDBLCLK 0x0203u
#define WM_CONTEXTMENU   0x007Bu
#define WM_APP           0x8000u

#define WS_OVERLAPPED    0x00000000u
#define WS_CAPTION       0x00C00000u
#define WS_SYSMENU       0x00080000u
#define WS_CHILD         0x40000000u
#define WS_VISIBLE       0x10000000u
#define WS_BORDER        0x00800000u
#define BS_GROUPBOX      0x00000007u
#define BS_OWNERDRAW     0x0000000Bu
#define ES_NUMBER        0x00002000u
#define ES_AUTOHSCROLL   0x00000080u
#define SW_SHOWNORMAL    1
#define SW_SHOW          5
#define TRANSPARENT      1
#define DT_LEFT          0x00000000u
#define DT_CENTER        0x00000001u
#define DT_VCENTER       0x00000004u
#define DT_SINGLELINE    0x00000020u
#define DT_NOPREFIX      0x00000800u
#define ODS_SELECTED     0x0001u
#define CC_RGBINIT       0x00000001u
#define CC_FULLOPEN      0x00000002u
#define RGB_(r,g,b) ((COLORREF)(((BYTE)(r)) | ((WORD)((BYTE)(g)) << 8) | (((DWORD)(BYTE)(b)) << 16)))
#define GET_R_VALUE(rgb) ((BYTE)((rgb) & 0xffu))
#define GET_G_VALUE(rgb) ((BYTE)(((rgb) >> 8) & 0xffu))
#define GET_B_VALUE(rgb) ((BYTE)(((rgb) >> 16) & 0xffu))
#define LOWORD_(v) ((WORD)((ULONG_PTR)(v) & 0xffffu))
#define WM_TRAY          (WM_APP + 1u)

#define NIM_ADD    0x00000000u
#define NIM_MODIFY 0x00000001u
#define NIM_DELETE 0x00000002u
#define NIF_MESSAGE 0x00000001u
#define NIF_ICON    0x00000002u
#define NIF_TIP     0x00000004u

#define MF_STRING    0x00000000u
#define MF_GRAYED    0x00000001u
#define MF_CHECKED   0x00000008u
#define MF_SEPARATOR 0x00000800u

#define TPM_RIGHTBUTTON 0x0002u
#define TPM_RETURNCMD   0x0100u

#define OFN_READONLY       0x00000001u
#define OFN_NOCHANGEDIR    0x00000008u
#define OFN_PATHMUSTEXIST  0x00000800u
#define OFN_FILEMUSTEXIST  0x00001000u
#define OFN_EXPLORER       0x00080000u

#define SND_ASYNC     0x0001u
#define SND_NODEFAULT 0x0002u
#define SND_FILENAME  0x00020000u

#define MB_OK              0x00000000u
#define MB_ICONINFORMATION 0x00000040u
#define MB_ICONERROR       0x00000010u

#define LANG_ENGLISH 0x09u
#define LANG_RUSSIAN 0x19u
#define TH32CS_SNAPPROCESS 0x00000002u
#define PRIMARYLANGID(lgid) ((WORD)((lgid) & 0x03ffu))

#define KEY_QUERY_VALUE 0x0001u
#define KEY_SET_VALUE   0x0002u
#define REG_SZ 1u
#define REG_DWORD 4u
#define ERROR_SUCCESS 0u
#define ERROR_FILE_NOT_FOUND 2u
#define ERROR_ALREADY_EXISTS 183u

#define GENERIC_READ  0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 0x00000001u
#define FILE_SHARE_WRITE 0x00000002u
#define CREATE_NEW 1u
#define OPEN_EXISTING 3u
#define FILE_ATTRIBUTE_NORMAL 0x00000080u
#define FILE_BEGIN 0u
#define FILE_END 2u
#define CP_ACP 0u
#define INVALID_FILE_ATTRIBUTES 0xffffffffu
#define INVALID_SET_FILE_POINTER 0xffffffffu
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#define PURGE_TXCLEAR 0x0004u
#define PURGE_RXCLEAR 0x0008u
#define DIGCF_PRESENT 0x00000002u
#define SPDRP_HARDWAREID 0x00000001u
#define DICS_FLAG_GLOBAL 0x00000001u
#define DIREG_DEV 0x00000001u
#define OBJID_CLIENT ((LONG)-4)
#define CHILDID_SELF 0
#define COINIT_APARTMENTTHREADED 0x2u
#define S_OK ((HRESULT)0L)
#define S_FALSE ((HRESULT)1L)
#define SUCCEEDED(hr) ((HRESULT)(hr) >= 0)

#define IDI_APPLICATION ((LPCWSTR)(ULONG_PTR)32512u)
#define HKEY_CURRENT_USER ((HKEY)(LONG_PTR)-2147483647LL)
#define HKEY_LOCAL_MACHINE ((HKEY)(LONG_PTR)-2147483646LL)

#define CMD_SOUNDS_ENABLED 1001u
#define CMD_ENGLISH_WAV    1002u
#define CMD_RUSSIAN_WAV    1003u
#define CMD_TEST_ENGLISH   1004u
#define CMD_TEST_RUSSIAN   1005u
#define CMD_RESET_SOUNDS   1006u
#define CMD_AUTOSTART      1007u
#define CMD_ABOUT          1008u
#define CMD_EXIT           1009u
#define CMD_BOARD_RECONNECT 1010u
#define CMD_BOARD_SETTINGS  1011u
#define CMD_LOCKSCREEN       1012u

#define IDC_ENG_HZ       2001u
#define IDC_ENG_MS       2002u
#define IDC_ENG_COLOR    2003u
#define IDC_RUS_HZ       2004u
#define IDC_RUS_MS       2005u
#define IDC_RUS_COLOR    2006u
#define IDC_FLASH_MS     2007u
#define IDC_FLASH_COLOR  2008u
#define IDC_SAVE         2009u
#define IDC_DEFAULTS     2010u
#define IDC_CANCEL       2011u
#define IDC_NIGHT_TOGGLE 2012u
#define IDC_NIGHT_FROM   2013u
#define IDC_NIGHT_TO     2014u
#define IDC_NIGHT_VOLUME 2015u
#define IDC_NIGHT_BRIGHT 2016u
#define IDC_DAY_VOLUME   2017u

#define ARRAYSIZE(a) ((UINT)(sizeof(a)/sizeof((a)[0])))
#define UOI_NAME 2
#define UOI_IO 6
#define DESKTOP_READOBJECTS 0x0001u
#define AT_KEY_NAME L"LayoutBeep_LockScreen_v2"
#define AT_KEY_PATH L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Accessibility\\ATs\\LayoutBeep_LockScreen_v2"
#define AT_OLD_KEY_NAME L"LayoutBeep_LockScreen_v1"
#define AT_OLD_KEY_PATH L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Accessibility\\ATs\\LayoutBeep_LockScreen_v1"
#define AT_CONFIG_PATH L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Accessibility"
#define AT_TEMP_PATH L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AccessibilityTemp"
#define SEE_MASK_NOCLOSEPROCESS 0x00000040u
#define INFINITE 0xffffffffu
#define SYNCHRONIZE_ 0x00100000u
#define INPUT_KEYBOARD 1u
#define KEYEVENTF_KEYUP 0x0002u
#define VK_LWIN 0x5Bu


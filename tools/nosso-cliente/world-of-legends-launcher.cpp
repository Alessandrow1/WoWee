// World of Legends Launcher v0.1 - self-contained Win32 GUI (no Electron/runtime).
// The existing wowee.exe and game data remain unchanged.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>

namespace {
constexpr int ID_PLAY = 1001;
constexpr int ID_SETTINGS = 1002;
constexpr int ID_REFRESH = 1003;
constexpr int ID_CLOSE = 1004;
constexpr UINT_PTR STATUS_TIMER = 2001;
constexpr COLORREF BG = RGB(12, 21, 37);
constexpr COLORREF PANEL = RGB(23, 37, 57);
constexpr COLORREF GOLD = RGB(225, 183, 96);
constexpr COLORREF WHITE = RGB(235, 240, 250);
constexpr COLORREF MUTED = RGB(166, 184, 207);
constexpr COLORREF GREEN = RGB(115, 222, 151);
constexpr COLORREF RED = RGB(255, 134, 131);

std::filesystem::path base;
std::wstring ini;
HWND mainWindow = nullptr;
HWND playButton = nullptr;
HWND refreshButton = nullptr;
HWND settingsButton = nullptr;
HWND statusLabel = nullptr;
HWND pathLabel = nullptr;
HFONT fontBody = nullptr;
HFONT fontSmall = nullptr;
HFONT fontButton = nullptr;
HFONT fontTitle = nullptr;
HFONT fontSubtitle = nullptr;
HBRUSH bgBrush = nullptr;
HBRUSH panelBrush = nullptr;
bool realmOnline = false;

std::wstring iniValue(const wchar_t* key, const wchar_t* fallback) {
    wchar_t value[2048]{};
    GetPrivateProfileStringW(L"WorldOfLegends", key, fallback, value, 2048, ini.c_str());
    return value;
}

void setText(HWND control, const std::wstring& text) {
    SetWindowTextW(control, text.c_str());
}

bool tcpReachable(const std::wstring& host, int port) {
    // A short non-blocking TCP connection probe. This checks reachability, not
    // game authentication or the completeness of world initialization.
    addrinfoW hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfoW* results = nullptr;
    std::wstring portText = std::to_wstring(port);
    if (GetAddrInfoW(host.c_str(), portText.c_str(), &hints, &results) != 0)
        return false;
    bool reachable = false;
    for (auto* addr = results; addr != nullptr && !reachable; addr = addr->ai_next) {
        SOCKET sock = socket(addr->ai_family, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) continue;
        u_long nonblock = 1;
        ioctlsocket(sock, FIONBIO, &nonblock);
        int rc = connect(sock, addr->ai_addr, static_cast<int>(addr->ai_addrlen));
        if (rc == 0) {
            reachable = true;
        } else if (WSAGetLastError() == WSAEWOULDBLOCK) {
            fd_set writeSet, errorSet;
            FD_ZERO(&writeSet);
            FD_ZERO(&errorSet);
            FD_SET(sock, &writeSet);
            FD_SET(sock, &errorSet);
            timeval timeout{0, 150000};  // 150 ms maximum per candidate
            rc = select(0, nullptr, &writeSet, &errorSet, &timeout);
            if (rc > 0 && FD_ISSET(sock, &writeSet)) {
                int errorCode = 1;
                int errorSize = sizeof(errorCode);
                reachable = getsockopt(sock, SOL_SOCKET, SO_ERROR,
                                       reinterpret_cast<char*>(&errorCode), &errorSize) == 0
                            && errorCode == 0;
            }
        }
        closesocket(sock);
    }
    FreeAddrInfoW(results);
    return reachable;
}

void refreshStatus() {
    const auto host = iniValue(L"ServerHost", L"127.0.0.1");
    const bool auth = tcpReachable(host, 3724);
    const bool world = tcpReachable(host, 8085);
    realmOnline = auth && world;
    std::wstring message = L"REINO: ";
    if (realmOnline) message += L"ONLINE  (autenticacao + mundo acessiveis)";
    else if (auth) message += L"AUTH ONLINE  |  MUNDO OFFLINE";
    else if (world) message += L"MUNDO ONLINE  |  AUTH OFFLINE";
    else message += L"OFFLINE";
    setText(statusLabel, message);
    InvalidateRect(mainWindow, nullptr, FALSE);
}

void openSettings() {
    if (!std::filesystem::exists(ini)) {
        WritePrivateProfileStringW(L"WorldOfLegends", L"DataPath", L"E:\\", ini.c_str());
        WritePrivateProfileStringW(L"WorldOfLegends", L"ServerHost", L"127.0.0.1", ini.c_str());
    }
    HINSTANCE status = ShellExecuteW(mainWindow, L"open", L"notepad.exe",
                                    (L"\"" + ini + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(status) <= 32)
        MessageBoxW(mainWindow, L"Nao consegui abrir as configuracoes.", L"World of Legends", MB_ICONERROR);
}

void launchGame() {
    const auto client = base / L"wowee.exe";
    if (!std::filesystem::exists(client)) {
        MessageBoxW(mainWindow, L"wowee.exe nao encontrado ao lado do launcher.",
                    L"World of Legends", MB_ICONERROR);
        return;
    }
    const auto dataPath = iniValue(L"DataPath", L"E:\\");
    if (!std::filesystem::exists(std::filesystem::path(dataPath) / L"expansions")) {
        MessageBoxW(mainWindow,
            L"Dados nao encontrados. Abra Configuracoes e corrija DataPath.\n"
            L"A pasta indicada precisa conter expansions\\wotlk.",
            L"World of Legends", MB_ICONERROR);
        return;
    }
    SetEnvironmentVariableW(L"WOW_DATA_PATH", dataPath.c_str());
    auto defaultEnv = [](const wchar_t* key, const wchar_t* val) {
        if (GetEnvironmentVariableW(key, nullptr, 0) == 0)
            SetEnvironmentVariableW(key, val);
    };
    defaultEnv(L"WOWEE_PREVIEW_MSAA", L"1");
    defaultEnv(L"WOWEE_FILE_CACHE_MB", L"256");
    defaultEnv(L"WOWEE_INITIAL_TERRAIN_RADIUS", L"2");

    std::wstring command = L"\"" + client.wstring() + L"\"";
    std::vector<wchar_t> writable(command.begin(), command.end());
    writable.push_back(L'\0');
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(client.c_str(), writable.data(), nullptr, nullptr,
                        FALSE, 0, nullptr, base.c_str(), &si, &pi)) {
        MessageBoxW(mainWindow, L"Nao foi possivel abrir o jogo. Confira as DLLs do pacote.",
                    L"World of Legends", MB_ICONERROR);
        return;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    ShowWindow(mainWindow, SW_MINIMIZE);
}

void paintBackground(HDC hdc, const RECT& rect) {
    FillRect(hdc, &rect, bgBrush);
    RECT header{0, 0, rect.right, 143};
    FillRect(hdc, &header, panelBrush);
    HBRUSH goldBrush = CreateSolidBrush(GOLD);
    RECT accent{0, 140, rect.right, 143};
    FillRect(hdc, &accent, goldBrush);
    DeleteObject(goldBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, GOLD);
    SelectObject(hdc, fontTitle);
    RECT title{42, 26, rect.right - 25, 83};
    DrawTextW(hdc, L"WORLD OF LEGENDS", -1, &title, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SetTextColor(hdc, MUTED);
    SelectObject(hdc, fontSubtitle);
    RECT subtitle{44, 87, rect.right - 25, 120};
    DrawTextW(hdc, L"Seu reino. Sua aventura. Seu mundo.", -1, &subtitle, DT_LEFT | DT_SINGLELINE);
    SelectObject(hdc, fontSmall);
    RECT foot{42, rect.bottom - 45, rect.right - 20, rect.bottom - 12};
    DrawTextW(hdc, L"Launcher v0.1  |  Cliente WotLK 3.3.5a  |  Status: verificacao TCP", -1,
              &foot, DT_LEFT | DT_SINGLELINE);
}

LRESULT CALLBACK windowProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        mainWindow = hWnd;  // WM_CREATE runs before CreateWindowExW returns.
        HINSTANCE inst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hWnd, GWLP_HINSTANCE));
        playButton = CreateWindowW(L"BUTTON", L"JOGAR", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                   42, 250, 255, 68, hWnd,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PLAY)), inst, nullptr);
        settingsButton = CreateWindowW(L"BUTTON", L"CONFIGURACOES", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                      315, 250, 194, 68, hWnd,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SETTINGS)), inst, nullptr);
        refreshButton = CreateWindowW(L"BUTTON", L"ATUALIZAR STATUS", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                     527, 250, 200, 68, hWnd,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_REFRESH)), inst, nullptr);
        statusLabel = CreateWindowW(L"STATIC", L"REINO: verificando...", WS_CHILD | WS_VISIBLE,
                                    44, 179, 690, 35, hWnd, nullptr, inst, nullptr);
        pathLabel = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                  44, 217, 690, 25, hWnd, nullptr, inst, nullptr);
        SendMessageW(statusLabel, WM_SETFONT, reinterpret_cast<WPARAM>(fontBody), TRUE);
        SendMessageW(pathLabel, WM_SETFONT, reinterpret_cast<WPARAM>(fontSmall), TRUE);
        SetTimer(hWnd, STATUS_TIMER, 10000, nullptr);
        setText(pathLabel, L"Dados do jogo: " + iniValue(L"DataPath", L"E:\\"));
        refreshStatus();
        return 0;
    }
    case WM_TIMER:
        if (wp == STATUS_TIMER) refreshStatus();
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_PLAY: launchGame(); break;
        case ID_SETTINGS: openSettings(); break;
        case ID_REFRESH:
            setText(pathLabel, L"Dados do jogo: " + iniValue(L"DataPath", L"E:\\"));
            refreshStatus();
            break;
        default: break;
        }
        return 0;
    case WM_DRAWITEM: {
        auto* info = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if (info->CtlType != ODT_BUTTON) break;
        bool play = info->CtlID == ID_PLAY;
        COLORREF fill = play ? RGB(146, 106, 40) : RGB(38, 58, 83);
        if (info->itemState & ODS_SELECTED) fill = play ? RGB(115, 81, 30) : RGB(27, 43, 66);
        HBRUSH brush = CreateSolidBrush(fill);
        FillRect(info->hDC, &info->rcItem, brush);
        DeleteObject(brush);
        HBRUSH border = CreateSolidBrush(GOLD);
        RECT line{info->rcItem.left, info->rcItem.bottom - 3,
                  info->rcItem.right, info->rcItem.bottom};
        FillRect(info->hDC, &line, border);
        DeleteObject(border);
        SetBkMode(info->hDC, TRANSPARENT);
        SetTextColor(info->hDC, WHITE);
        SelectObject(info->hDC, fontButton);
        wchar_t label[80]{};
        GetWindowTextW(info->hwndItem, label, 80);
        DrawTextW(info->hDC, label, -1, &info->rcItem,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return TRUE;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wp);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, reinterpret_cast<HWND>(lp) == statusLabel
                             ? (realmOnline ? GREEN : RED) : MUTED);
        return reinterpret_cast<LRESULT>(bgBrush);
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hWnd, &ps);
        RECT rect;
        GetClientRect(hWnd, &rect);
        paintBackground(dc, rect);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hWnd, STATUS_TIMER);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    wchar_t filename[32768]{};
    DWORD count = GetModuleFileNameW(nullptr, filename, 32768);
    if (!count || count >= 32768) return 1;
    base = std::filesystem::path(filename).parent_path();
    ini = (base / L"World-of-Legends.ini").wstring();

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        MessageBoxW(nullptr, L"Falha ao inicializar rede do Windows.",
                    L"World of Legends", MB_ICONERROR);
        return 1;
    }

    bgBrush = CreateSolidBrush(BG);
    panelBrush = CreateSolidBrush(PANEL);
    fontTitle = CreateFontW(-37, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Georgia");
    fontSubtitle = CreateFontW(-19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    fontBody = CreateFontW(-23, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    fontSmall = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    fontButton = CreateFontW(-19, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    WNDCLASSW cls{};
    cls.lpfnWndProc = windowProc;
    cls.hInstance = instance;
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    cls.hbrBackground = bgBrush;
    cls.lpszClassName = L"WorldOfLegendsLauncherV01";
    if (!RegisterClassW(&cls)) return 1;

    mainWindow = CreateWindowExW(0, cls.lpszClassName, L"World of Legends - Launcher",
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, 790, 420,
                                nullptr, nullptr, instance, nullptr);
    if (!mainWindow) return 1;
    SendMessageW(mainWindow, WM_SETICON, ICON_BIG,
                 reinterpret_cast<LPARAM>(LoadIconW(instance, MAKEINTRESOURCEW(1))));
    ShowWindow(mainWindow, showCommand);
    UpdateWindow(mainWindow);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    DeleteObject(fontTitle);
    DeleteObject(fontSubtitle);
    DeleteObject(fontBody);
    DeleteObject(fontSmall);
    DeleteObject(fontButton);
    DeleteObject(bgBrush);
    DeleteObject(panelBrush);
    WSACleanup();
    return static_cast<int>(message.wParam);
}

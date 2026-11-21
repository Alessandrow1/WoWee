// World of Legends Launcher v0.3 - self-contained Win32 GUI (no Electron/runtime).
// The existing wowee.exe and game data remain unchanged.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <propidl.h>
#include <gdiplus.h>
#include <memory>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>
#include <utility>

namespace {
using namespace Gdiplus;
constexpr int ID_PLAY = 1001;
constexpr int ID_SETTINGS = 1002;
constexpr int ID_REFRESH = 1003;
constexpr int ID_UPDATE = 1005;
constexpr int ID_CLOSE = 1004;
constexpr int ID_SAVE_SETTINGS = 1101;
constexpr int ID_CANCEL_SETTINGS = 1102;
constexpr int ID_PATH = 1103;
constexpr int ID_HOST = 1104;
constexpr int ID_CACHE = 1105;
constexpr int ID_TERRAIN = 1106;
constexpr int ID_MSAA = 1107;
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
HWND updateButton = nullptr;
HWND statusLabel = nullptr;
HWND pathLabel = nullptr;
HWND settingsWindow = nullptr;
HWND pathEdit = nullptr;
HWND hostEdit = nullptr;
HWND cacheCombo = nullptr;
HWND terrainCombo = nullptr;
HWND msaaCombo = nullptr;
HFONT fontBody = nullptr;
HFONT fontSmall = nullptr;
HFONT fontButton = nullptr;
HFONT fontTitle = nullptr;
HFONT fontSubtitle = nullptr;
HBRUSH bgBrush = nullptr;
HBRUSH panelBrush = nullptr;
bool realmOnline = false;
std::wstring realmStatus = L"VERIFICANDO SERVIDOR...";
std::unique_ptr<Gdiplus::Image> artwork;
ULONG_PTR gdiplusToken = 0;

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
    realmStatus = message;
    if (mainWindow) InvalidateRect(mainWindow, nullptr, FALSE);
}


std::wstring controlText(HWND control) {
    const int n = GetWindowTextLengthW(control);
    std::wstring text(static_cast<size_t>(n) + 1, L'\0');
    GetWindowTextW(control, text.data(), n + 1);
    text.resize(static_cast<size_t>(n));
    return text;
}

HWND settingsControl(HWND parent, const wchar_t* kind, const wchar_t* caption,
                     DWORD style, int x, int y, int w, int h, int id = 0) {
    HWND control = CreateWindowExW(0, kind, caption, WS_CHILD | WS_VISIBLE | style,
                        x, y, w, h, parent,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE)), nullptr);
    if (control) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(fontSmall), TRUE);
    return control;
}

void setupCombo(HWND combo, const std::vector<std::wstring>& values, const std::wstring& selected) {
    int target = 0;
    for (int i = 0; i < static_cast<int>(values.size()); ++i) {
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(values[i].c_str()));
        if (values[i] == selected) target = i;
    }
    SendMessageW(combo, CB_SETCURSEL, target, 0);
}

std::wstring selectedCombo(HWND combo) {
    int i = static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
    if (i < 0) return {};
    wchar_t buffer[128]{};
    SendMessageW(combo, CB_GETLBTEXT, i, reinterpret_cast<LPARAM>(buffer));
    return buffer;
}

bool saveSettings(HWND dialog) {
    const std::wstring path = controlText(pathEdit);
    const std::wstring host = controlText(hostEdit);
    if (path.empty() || !std::filesystem::exists(std::filesystem::path(path) / L"expansions")) {
        MessageBoxW(dialog, L"DataPath deve apontar para uma pasta que contem expansions.",
                    L"Caminho invalido", MB_ICONWARNING);
        return false;
    }
    if (host.empty() || host.find_first_of(L" \t\r\n") != std::wstring::npos) {
        MessageBoxW(dialog, L"Informe um IP ou hostname sem espacos.",
                    L"Servidor invalido", MB_ICONWARNING);
        return false;
    }
    const std::pair<const wchar_t*, std::wstring> entries[] = {
        {L"DataPath", path},
        {L"ServerHost", host},
        {L"FileCacheMB", selectedCombo(cacheCombo)},
        {L"InitialTerrainRadius", selectedCombo(terrainCombo)},
        {L"PreviewMSAA", selectedCombo(msaaCombo)}
    };
    for (const auto& [key, value] : entries) {
        if (!WritePrivateProfileStringW(L"WorldOfLegends", key, value.c_str(), ini.c_str())) {
            MessageBoxW(dialog, L"Nao foi possivel salvar o arquivo INI.",
                        L"Erro ao salvar", MB_ICONERROR);
            return false;
        }
    }
    refreshStatus();
    InvalidateRect(mainWindow, nullptr, FALSE);
    return true;
}

LRESULT CALLBACK settingsProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    switch (message) {
    case WM_CREATE: {
        settingsWindow = window;
        settingsControl(window, L"STATIC", L"CAMINHO DOS DADOS (ex.: E:\\)",
                        0, 24, 24, 460, 24);
        pathEdit = settingsControl(window, L"EDIT", iniValue(L"DataPath", L"E:\\").c_str(),
                        WS_BORDER | ES_AUTOHSCROLL, 24, 51, 455, 30, ID_PATH);
        settingsControl(window, L"STATIC", L"HOST DO SERVIDOR (IP ou nome do PC)",
                        0, 24, 98, 460, 24);
        hostEdit = settingsControl(window, L"EDIT", iniValue(L"ServerHost", L"127.0.0.1").c_str(),
                        WS_BORDER | ES_AUTOHSCROLL, 24, 124, 455, 30, ID_HOST);

        settingsControl(window, L"STATIC", L"CACHE DE ARQUIVOS (MB)", 0, 24, 171, 460, 22);
        cacheCombo = settingsControl(window, L"COMBOBOX", L"",
                          CBS_DROPDOWNLIST | WS_VSCROLL, 24, 196, 455, 160, ID_CACHE);
        setupCombo(cacheCombo, {L"128", L"256", L"512", L"1024"},
                   iniValue(L"FileCacheMB", L"256"));

        settingsControl(window, L"STATIC", L"RAIO INICIAL DO TERRENO (1 rapido, 4 completo)",
                        0, 24, 239, 460, 22);
        terrainCombo = settingsControl(window, L"COMBOBOX", L"",
                          CBS_DROPDOWNLIST | WS_VSCROLL, 24, 265, 455, 145, ID_TERRAIN);
        setupCombo(terrainCombo, {L"1", L"2", L"3", L"4"},
                   iniValue(L"InitialTerrainRadius", L"2"));

        settingsControl(window, L"STATIC", L"MSAA DA PREVIA 3D", 0, 24, 307, 460, 22);
        msaaCombo = settingsControl(window, L"COMBOBOX", L"",
                          CBS_DROPDOWNLIST | WS_VSCROLL, 24, 333, 455, 130, ID_MSAA);
        setupCombo(msaaCombo, {L"1", L"2", L"4"},
                   iniValue(L"PreviewMSAA", L"1"));

        settingsControl(window, L"STATIC", L"Resolucao e tela cheia: ajustar pelo menu de video do jogo.",
                        0, 24, 387, 465, 24);
        settingsControl(window, L"BUTTON", L"SALVAR CONFIGURACOES",
                        BS_PUSHBUTTON, 24, 427, 274, 40, ID_SAVE_SETTINGS);
        settingsControl(window, L"BUTTON", L"CANCELAR",
                        BS_PUSHBUTTON, 310, 427, 169, 40, ID_CANCEL_SETTINGS);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == ID_SAVE_SETTINGS) {
            if (saveSettings(window)) DestroyWindow(window);
            return 0;
        }
        if (LOWORD(wp) == ID_CANCEL_SETTINGS) {
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_CLOSE: DestroyWindow(window); return 0;
    case WM_DESTROY:
        settingsWindow = nullptr;
        pathEdit = hostEdit = cacheCombo = terrainCombo = msaaCombo = nullptr;
        return 0;
    }
    return DefWindowProcW(window, message, wp, lp);
}

void openSettings() {
    if (settingsWindow) {
        ShowWindow(settingsWindow, SW_RESTORE);
        SetForegroundWindow(settingsWindow);
        return;
    }
    static bool registered = false;
    const HINSTANCE instance = reinterpret_cast<HINSTANCE>(
        GetWindowLongPtrW(mainWindow, GWLP_HINSTANCE));
    if (!registered) {
        WNDCLASSW cls{};
        cls.lpfnWndProc = settingsProc;
        cls.hInstance = instance;
        cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        cls.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
        cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        cls.lpszClassName = L"WorldOfLegendsSettingsV02";
        if (!RegisterClassW(&cls)) {
            MessageBoxW(mainWindow, L"Nao consegui registrar a janela de configuracoes.",
                        L"World of Legends", MB_ICONERROR);
            return;
        }
        registered = true;
    }
    HWND win = CreateWindowExW(WS_EX_DLGMODALFRAME, L"WorldOfLegendsSettingsV02",
                L"World of Legends - Configuracoes",
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                CW_USEDEFAULT, CW_USEDEFAULT, 522, 515,
                mainWindow, nullptr, instance, nullptr);
    if (!win) {
        MessageBoxW(mainWindow, L"Nao foi possivel abrir as configuracoes.",
                    L"World of Legends", MB_ICONERROR);
        return;
    }
    ShowWindow(win, SW_SHOW);
    UpdateWindow(win);
}

void checkForUpdates() {
    const auto script = base / L"world-of-legends-update.ps1";
    if (!std::filesystem::exists(script)) {
        MessageBoxW(mainWindow, L"Arquivo de atualizacao ausente. Recrie o pacote Windows.",
                    L"World of Legends", MB_ICONERROR);
        return;
    }
    // This packaged, locally maintained script only stages verified downloads.
    // It never executes downloaded code or replaces a running installation.
    const std::wstring params = L"-NoProfile -ExecutionPolicy Bypass -File \"" +
        script.wstring() + L"\" -InstalledVersion \"0.3.0\"";
    HINSTANCE started = ShellExecuteW(mainWindow, L"open", L"powershell.exe",
                                      params.c_str(), base.c_str(), SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(started) <= 32) {
        MessageBoxW(mainWindow, L"Windows PowerShell nao encontrado.",
                    L"World of Legends", MB_ICONERROR);
    }
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
    // Apply only settings that the game actually supports. They are stored in
    // the launcher INI, not in the global Windows environment.
    SetEnvironmentVariableW(L"WOWEE_PREVIEW_MSAA",
                            iniValue(L"PreviewMSAA", L"1").c_str());
    SetEnvironmentVariableW(L"WOWEE_FILE_CACHE_MB",
                            iniValue(L"FileCacheMB", L"256").c_str());
    SetEnvironmentVariableW(L"WOWEE_INITIAL_TERRAIN_RADIUS",
                            iniValue(L"InitialTerrainRadius", L"2").c_str());

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


void roundedBox(Graphics& g, const RectF& r, float radius, const Color& color,
                const Color& outline, float borderWidth = 1.0f) {
    GraphicsPath p;
    float d = radius * 2;
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
    p.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90);
    p.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90);
    p.CloseFigure();
    SolidBrush b(color);
    g.FillPath(&b, &p);
    Pen border(outline, borderWidth);
    g.DrawPath(&border, &p);
}

void label(Graphics& g, const wchar_t* content, float x, float y, float size,
           Color color, bool bold = false) {
    FontFamily family(L"Segoe UI");
    Font font(&family, size, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    SolidBrush brush(color);
    PointF origin(x, y);
    g.DrawString(content, -1, &font, origin, &brush);
}

void paintBackground(HDC dc, const RECT& rect) {
    Graphics g(dc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    const int width = rect.right, height = rect.bottom;

    SolidBrush solid(Color(255, 8, 17, 33));
    g.FillRectangle(&solid, 0, 0, width, height);
    if (artwork && artwork->GetLastStatus() == Ok) {
        const UINT aw = artwork->GetWidth();
        const UINT ah = artwork->GetHeight();
        if (aw && ah) {
            // Fill the hero area while keeping the original artwork unaltered.
            const float imageAreaH = static_cast<float>(height) * 0.86f;
            const float scale = std::max(static_cast<float>(width) / aw, imageAreaH / ah);
            const float srcW = width / scale;
            const float srcH = imageAreaH / scale;
            g.DrawImage(artwork.get(), RectF(0, 0, static_cast<REAL>(width), imageAreaH),
                        (aw - srcW) * 0.5f, (ah - srcH) * 0.34f,
                        srcW, srcH, UnitPixel);
        }
    }

    // Cinematic grading, legible controls, no edited or regenerated artwork.
    LinearGradientBrush shade(Point(0, 0), Point(0, height),
        Color(40, 4, 9, 18), Color(255, 5, 13, 25));
    g.FillRectangle(&shade, 0, 0, width, height);
    SolidBrush bottom(Color(224, 8, 20, 37));
    g.FillRectangle(&bottom, 0, height - 185, width, 185);
    SolidBrush line(Color(210, 200, 152, 73));
    g.FillRectangle(&line, 0, height - 185, width, 2);

    roundedBox(g, RectF(30, 24, 160, 36), 10,
               Color(180, 8, 20, 37), Color(170, 214, 168, 82));
    label(g, L"WOL  /  LAUNCHER", 44, 30, 16, Color(255, 239, 206, 142), true);
    label(g, L"SUA LENDA COMEÇA AQUI", 35, height - 314, 34,
          Color(255, 249, 224, 165), true);
    label(g, L"Explore um mundo de aventuras.", 37, height - 266, 19,
          Color(255, 226, 233, 247));
    label(g, L"WORLD OF LEGENDS", 36, height - 164, 18,
          Color(255, 241, 210, 146), true);

    Color statusColor = realmOnline ? Color(255, 128, 237, 167) : Color(255, 255, 151, 142);
    label(g, realmStatus.c_str(), 35, height - 130, 17, statusColor, true);
    auto dataPath = iniValue(L"DataPath", L"E:\\");
    std::wstring dataInfo = L"ARQUIVOS DO JOGO  " + dataPath;
    label(g, dataInfo.c_str(), 35, height - 96, 15, Color(255, 170, 191, 213));
    label(g, L"v0.3  |  WotLK 3.3.5a", 36, height - 44, 14, Color(255, 135, 156, 185));
    label(g, L"Status indica portas TCP acessiveis, nao autentica o jogo.",
          285, height - 43, 13, Color(255, 130, 152, 179));
}

LRESULT CALLBACK windowProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        mainWindow = hWnd;  // WM_CREATE runs before CreateWindowExW returns.
        HINSTANCE inst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hWnd, GWLP_HINSTANCE));
        playButton = CreateWindowW(L"BUTTON", L"JOGAR AGORA", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                   0, 0, 0, 0, hWnd,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PLAY)), inst, nullptr);
        settingsButton = CreateWindowW(L"BUTTON", L"CONFIGURACOES", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                      0, 0, 0, 0, hWnd,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SETTINGS)), inst, nullptr);
        refreshButton = CreateWindowW(L"BUTTON", L"VERIFICAR STATUS", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                     0, 0, 0, 0, hWnd,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_REFRESH)), inst, nullptr);
        updateButton = CreateWindowW(L"BUTTON", L"VERIFICAR ATUALIZACOES",
                                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                0, 0, 0, 0, hWnd,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_UPDATE)), inst, nullptr);
        SetTimer(hWnd, STATUS_TIMER, 10000, nullptr);
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
        case ID_UPDATE: checkForUpdates(); break;
        case ID_REFRESH:
            refreshStatus();
            break;
        default: break;
        }
        return 0;
    case WM_SIZE: {
        const int w = LOWORD(lp), h = HIWORD(lp);
        if (playButton) {
            const int side = w - 35;
            MoveWindow(playButton, side - 302, h - 143, 302, 58, TRUE);
            MoveWindow(settingsButton, side - 302, h - 72, 145, 40, TRUE);
            MoveWindow(refreshButton, side - 145, h - 72, 145, 40, TRUE);
            MoveWindow(updateButton, w - 261, 25, 226, 42, TRUE);
        }
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }
    case WM_DRAWITEM: {
        auto* info = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if (info->CtlType != ODT_BUTTON) break;
        Graphics g(info->hDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        bool play = info->CtlID == ID_PLAY;
        bool pressed = (info->itemState & ODS_SELECTED) != 0;
        RECT rr = info->rcItem;
        RectF area(static_cast<REAL>(rr.left + 2), static_cast<REAL>(rr.top + 2),
                   static_cast<REAL>(rr.right - rr.left - 4),
                   static_cast<REAL>(rr.bottom - rr.top - 4));
        roundedBox(g, area, 9,
            play ? (pressed ? Color(255, 131, 89, 27) : Color(255, 181, 128, 49))
                 : (pressed ? Color(255, 24, 39, 60) : Color(255, 32, 52, 76)),
            play ? Color(255, 245, 205, 119) : Color(180, 167, 190, 212),
            play ? 2.0f : 1.0f);
        wchar_t text[80]{};
        GetWindowTextW(info->hwndItem, text, 80);
        FontFamily family(L"Segoe UI");
        Font font(&family, play ? 22.0f : 14.0f, FontStyleBold, UnitPixel);
        RectF bounds;
        g.MeasureString(text, -1, &font, PointF(0, 0), &bounds);
        SolidBrush fg(Color(255, 250, 243, 227));
        g.DrawString(text, -1, &font,
            PointF(area.X + (area.Width - bounds.Width) / 2,
                   area.Y + (area.Height - bounds.Height) / 2 - 1), &fg);
        return TRUE;
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

    GdiplusStartupInput startup;
    if (GdiplusStartup(&gdiplusToken, &startup, nullptr) != Ok) {
        WSACleanup();
        return 1;
    }
    artwork.reset(Image::FromFile((base / L"assets" / L"world-of-legends-login.png").c_str()));
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
                                CW_USEDEFAULT, CW_USEDEFAULT, 1020, 670,
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
    artwork.reset();
    GdiplusShutdown(gdiplusToken);
    WSACleanup();
    return static_cast<int>(message.wParam);
}

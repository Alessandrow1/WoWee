// World of Legends lightweight Windows launcher.
// Keeps the game data outside the distributable client and avoids requiring MSYS2 to launch.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <string>
#include <vector>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t filename[32768] = {};
    DWORD n = GetModuleFileNameW(nullptr, filename, 32768);
    if (n == 0 || n >= 32768) {
        MessageBoxW(nullptr, L"Nao foi possivel localizar o launcher.", L"World of Legends", MB_ICONERROR);
        return 1;
    }
    const std::filesystem::path base = std::filesystem::path(filename).parent_path();
    const std::filesystem::path client = base / L"wowee.exe";
    if (!std::filesystem::exists(client)) {
        MessageBoxW(nullptr, L"wowee.exe nao encontrado na pasta do World of Legends.", L"World of Legends", MB_ICONERROR);
        return 1;
    }
    const std::wstring ini = (base / L"World-of-Legends.ini").wstring();
    wchar_t dataPath[32768] = {};
    GetPrivateProfileStringW(L"WorldOfLegends", L"DataPath", L"E:\\", dataPath, 32768, ini.c_str());
    if (!std::filesystem::exists(std::filesystem::path(dataPath) / L"expansions")) {
        MessageBoxW(nullptr,
            L"Nao encontrei a pasta expansions no caminho de dados configurado.\n"
            L"Edite DataPath no arquivo World-of-Legends.ini.\n"
            L"Os dados originais do jogo nao acompanham este pacote.",
            L"World of Legends - Dados ausentes", MB_ICONERROR);
        return 1;
    }
    SetEnvironmentVariableW(L"WOW_DATA_PATH", dataPath);
    // Allow power users to override each option through their environment.
    auto setDefault = [](const wchar_t* name, const wchar_t* value) {
        if (GetEnvironmentVariableW(name, nullptr, 0) == 0)
            SetEnvironmentVariableW(name, value);
    };
    setDefault(L"WOWEE_PREVIEW_MSAA", L"1");
    setDefault(L"WOWEE_FILE_CACHE_MB", L"256");
    setDefault(L"WOWEE_INITIAL_TERRAIN_RADIUS", L"2");

    std::wstring command = L"\"" + client.wstring() + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    // CommandLine must be writable.
    std::vector<wchar_t> writable(command.begin(), command.end());
    writable.push_back(L'\0');
    BOOL ok = CreateProcessW(client.c_str(), writable.data(), nullptr, nullptr, FALSE,
                             0, nullptr, base.c_str(), &si, &pi);
    if (!ok) {
        MessageBoxW(nullptr,
            L"Nao foi possivel iniciar wowee.exe. Verifique as DLLs do pacote e o log.",
            L"World of Legends", MB_ICONERROR);
        return 1;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}

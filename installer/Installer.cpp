// G-Major Reborn — self-contained Windows installer / uninstaller.
//
// The payload (VST3, Standalone, tools, presets, docs) is embedded as RCDATA resources
// listed in the generated payload.h. Usage:
//   GMajorReborn-Setup.exe              interactive install
//   GMajorReborn-Setup.exe /S [/D=dir]  silent install (all default components)
//   Uninstall.exe /uninstall [/S]       (Uninstall.exe = copy of the installer in the app folder)
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <objbase.h>
#include <string>
#include <vector>
#include "payload.h"   // kPayload[], kPayloadCount, kVersion, kTotalBytes

namespace {

enum Root { RootApp = 0, RootVst3 = 1, RootUserPresets = 2 };
enum Comp { CompVst3 = 0, CompRacks, CompApp, CompPresets, CompStartMenu, CompDesktop, CompCount };

const wchar_t* kProduct = L"G-Major Reborn";
const wchar_t* kUninstKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\GMajorReborn";
const wchar_t* kLogName = L"install.log";

HINSTANCE gInst;
HWND gWnd, gPath, gProgress, gStatus, gInstallBtn, gCancelBtn, gBrowseBtn, gLaunch;
HWND gComp[CompCount];
bool gSilent = false, gDone = false;
HFONT gFont;

std::wstring knownFolder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    std::wstring s;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) s = p;
    CoTaskMemFree(p);
    return s;
}

std::wstring selfPath() {
    wchar_t buf[MAX_PATH * 4];
    const DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof buf / sizeof buf[0]));
    return std::wstring(buf, n);
}

std::wstring parentOf(const std::wstring& p) {
    const size_t k = p.find_last_of(L"\\/");
    return k == std::wstring::npos ? std::wstring() : p.substr(0, k);
}

bool ensureDir(const std::wstring& d) {
    if (d.empty()) return false;
    const int r = SHCreateDirectoryExW(nullptr, d.c_str(), nullptr);
    return r == ERROR_SUCCESS || r == ERROR_ALREADY_EXISTS || r == ERROR_FILE_EXISTS;
}

std::wstring readRegString(HKEY root, const wchar_t* key, const wchar_t* name) {
    wchar_t buf[2048]; DWORD sz = sizeof buf; DWORD type = 0;
    if (RegGetValueW(root, key, name, RRF_RT_REG_SZ, &type, buf, &sz) == ERROR_SUCCESS) return buf;
    return {};
}

void setStatus(const std::wstring& s) { if (gStatus) SetWindowTextW(gStatus, s.c_str()); }

void pump() {
    MSG m;
    while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
}

void message(const std::wstring& text, UINT flags = MB_ICONINFORMATION) {
    if (!gSilent) MessageBoxW(gWnd, text.c_str(), kProduct, flags | MB_OK);
}

std::wstring rootDir(int root, const std::wstring& appDir) {
    switch (root) {
        case RootVst3: return knownFolder(FOLDERID_ProgramFilesCommon) + L"\\VST3";
        case RootUserPresets: return knownFolder(FOLDERID_Documents) + L"\\G-Major Reborn";
        default: return appDir;
    }
}

bool compEnabled(int root, const std::wstring& rel, const bool* comps) {
    if (root == RootVst3) return rel.rfind(L"G-Major Reborn.vst3", 0) == 0 ? comps[CompVst3] : comps[CompRacks];
    if (root == RootUserPresets) return comps[CompPresets];
    // the app folder always receives docs/presets; the Standalone + tools follow the component
    const bool isBinary = rel.rfind(L"Standalone", 0) == 0 || rel.rfind(L"tools", 0) == 0;
    return !isBinary || comps[CompApp];
}

bool writeResource(int id, const std::wstring& dest) {
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10) /* RT_RCDATA */);
    if (!r) return false;
    HGLOBAL h = LoadResource(nullptr, r);
    const void* data = h ? LockResource(h) : nullptr;
    const DWORD size = SizeofResource(nullptr, r);
    if (!data) return false;
    if (!ensureDir(parentOf(dest))) return false;
    HANDLE f = CreateFileW(dest.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const BOOL ok = WriteFile(f, data, size, &written, nullptr);
    CloseHandle(f);
    return ok && written == size;
}

bool makeShortcut(const std::wstring& lnk, const std::wstring& target, const std::wstring& workDir) {
    IShellLinkW* sl = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&sl))) return false;
    sl->SetPath(target.c_str());
    sl->SetWorkingDirectory(workDir.c_str());
    sl->SetDescription(L"G-Major Reborn - multi-effect (Standalone)");
    IPersistFile* pf = nullptr;
    bool ok = false;
    if (SUCCEEDED(sl->QueryInterface(IID_IPersistFile, (void**)&pf))) {
        ensureDir(parentOf(lnk));
        ok = SUCCEEDED(pf->Save(lnk.c_str(), TRUE));
        pf->Release();
    }
    sl->Release();
    return ok;
}

void writeLog(const std::wstring& appDir, const std::vector<std::wstring>& files) {
    std::wstring all;
    for (const auto& f : files) { all += f; all += L"\r\n"; }
    const std::wstring path = appDir + L"\\" + kLogName;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    const WORD bom = 0xFEFF; DWORD w;
    WriteFile(h, &bom, 2, &w, nullptr);
    WriteFile(h, all.data(), (DWORD)(all.size() * sizeof(wchar_t)), &w, nullptr);
    CloseHandle(h);
}

std::vector<std::wstring> readLog(const std::wstring& appDir) {
    std::vector<std::wstring> out;
    HANDLE h = CreateFileW((appDir + L"\\" + kLogName).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return out;
    const DWORD size = GetFileSize(h, nullptr);
    std::wstring s(size / sizeof(wchar_t), L'\0');
    DWORD r = 0;
    ReadFile(h, s.data(), size, &r, nullptr);
    CloseHandle(h);
    if (!s.empty() && s[0] == 0xFEFF) s.erase(0, 1);
    size_t pos = 0;
    while (pos < s.size()) {
        size_t e = s.find(L"\r\n", pos);
        if (e == std::wstring::npos) e = s.size();
        if (e > pos) out.push_back(s.substr(pos, e - pos));
        pos = e + 2;
    }
    return out;
}

void registerUninstaller(const std::wstring& appDir, DWORD totalKB) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, kUninstKey, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    auto setS = [&](const wchar_t* n, const std::wstring& v) {
        RegSetValueExW(k, n, 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
    };
    auto setD = [&](const wchar_t* n, DWORD v) { RegSetValueExW(k, n, 0, REG_DWORD, (const BYTE*)&v, sizeof v); };
    const std::wstring un = appDir + L"\\Uninstall.exe";
    setS(L"DisplayName", kProduct);
    setS(L"DisplayVersion", kVersion);
    setS(L"Publisher", L"GMR Project");
    setS(L"InstallLocation", appDir);
    setS(L"UninstallString", L"\"" + un + L"\" /uninstall");
    setS(L"QuietUninstallString", L"\"" + un + L"\" /uninstall /S");
    setS(L"DisplayIcon", appDir + L"\\Standalone\\G-Major Reborn.exe");
    setD(L"EstimatedSize", totalKB);
    setD(L"NoModify", 1);
    setD(L"NoRepair", 1);
    RegCloseKey(k);
}

// ------------------------------------------------------------------------------ install
bool doInstall(const std::wstring& appDirIn, const bool* comps) {
    std::wstring appDir = appDirIn;
    while (!appDir.empty() && (appDir.back() == L'\\' || appDir.back() == L'/')) appDir.pop_back();
    if (appDir.empty() || !ensureDir(appDir)) {
        message(L"Não foi possível criar a pasta de instalação:\n" + appDirIn, MB_ICONERROR);
        return false;
    }
    std::vector<std::wstring> installed;
    // keep entries of a previous install (e.g. presets in Documents) so uninstall removes them too
    for (const auto& f : readLog(appDir)) installed.push_back(f);
    unsigned long long done = 0;
    for (int i = 0; i < kPayloadCount; ++i) {
        const auto& p = kPayload[i];
        if (!compEnabled(p.root, p.rel, comps)) { done += p.size; continue; }
        const std::wstring dest = rootDir(p.root, appDir) + L"\\" + p.rel;
        setStatus(L"Copiando " + std::wstring(p.rel));
        if (!writeResource(p.id, dest)) {
            message(L"Falha ao gravar:\n" + dest + L"\n\nSe o G-Major Reborn ou o seu DAW estiver aberto, feche-o e tente novamente.", MB_ICONERROR);
            writeLog(appDir, installed);
            return false;
        }
        installed.push_back(dest);
        // VST3 bundle folder icon (what JUCE's post-build `attrib +s` does on a Windows build machine)
        const std::wstring rel = p.rel;
        if (p.root == RootVst3 && rel.size() > 11 && _wcsicmp(rel.c_str() + rel.size() - 11, L"desktop.ini") == 0) {
            SetFileAttributesW(dest.c_str(), FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN);
            const std::wstring bundle = parentOf(dest);
            SetFileAttributesW(bundle.c_str(), GetFileAttributesW(bundle.c_str()) | FILE_ATTRIBUTE_SYSTEM);
        }
        done += p.size;
        if (gProgress) SendMessageW(gProgress, PBM_SETPOS, (WPARAM)(done * 1000ull / (kTotalBytes ? kTotalBytes : 1)), 0);
        pump();
    }
    // uninstaller = copy of this executable
    const std::wstring un = appDir + L"\\Uninstall.exe";
    if (_wcsicmp(selfPath().c_str(), un.c_str()) != 0) CopyFileW(selfPath().c_str(), un.c_str(), FALSE);
    installed.push_back(un);

    const std::wstring exe = appDir + L"\\Standalone\\G-Major Reborn.exe";
    if (comps[CompApp] && comps[CompStartMenu]) {
        const std::wstring dir = knownFolder(FOLDERID_CommonPrograms) + L"\\G-Major Reborn";
        const std::wstring l1 = dir + L"\\G-Major Reborn.lnk", l2 = dir + L"\\Desinstalar G-Major Reborn.lnk";
        if (makeShortcut(l1, exe, appDir + L"\\Standalone")) installed.push_back(l1);
        if (makeShortcut(l2, un, appDir)) installed.push_back(l2);
    }
    if (comps[CompApp] && comps[CompDesktop]) {
        const std::wstring l = knownFolder(FOLDERID_PublicDesktop) + L"\\G-Major Reborn.lnk";
        if (makeShortcut(l, exe, appDir + L"\\Standalone")) installed.push_back(l);
    }
    writeLog(appDir, installed);
    registerUninstaller(appDir, (DWORD)(kTotalBytes / 1024));
    if (gProgress) SendMessageW(gProgress, PBM_SETPOS, 1000, 0);
    setStatus(L"Instalação concluída.");
    return true;
}

// ------------------------------------------------------------------------------ uninstall
void removeEmptyParents(std::wstring dir, const std::vector<std::wstring>& stopAt) {
    for (int guard = 0; guard < 8 && !dir.empty(); ++guard) {
        for (const auto& s : stopAt) if (_wcsicmp(dir.c_str(), s.c_str()) == 0) return;
        if (!RemoveDirectoryW(dir.c_str())) return;   // fails when not empty: stop
        dir = parentOf(dir);
    }
}

int doUninstall() {
    const std::wstring appDir = parentOf(selfPath());
    if (!gSilent && MessageBoxW(nullptr, L"Remover o G-Major Reborn (plugin VST3, aplicativo e atalhos)?\n\n"
                                         L"Presets que você salvou em outras pastas não serão apagados.",
                                kProduct, MB_ICONQUESTION | MB_YESNO) != IDYES) return 1;
    const auto files = readLog(appDir);
    const std::wstring self = selfPath();
    const std::vector<std::wstring> stop = { knownFolder(FOLDERID_ProgramFilesCommon) + L"\\VST3", knownFolder(FOLDERID_ProgramFiles),
                                             knownFolder(FOLDERID_Documents), knownFolder(FOLDERID_CommonPrograms),
                                             knownFolder(FOLDERID_PublicDesktop) };
    int failed = 0;
    for (const auto& f : files) {
        if (_wcsicmp(f.c_str(), self.c_str()) == 0) continue;
        if (!DeleteFileW(f.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND) ++failed;
        removeEmptyParents(parentOf(f), stop);
    }
    DeleteFileW((appDir + L"\\" + kLogName).c_str());
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, kUninstKey);
    // delete ourselves and the (now empty) folder once this process has exited
    std::wstring cmd = L"cmd.exe /c ping -n 3 127.0.0.1 >nul & del /f /q \"" + self + L"\" & rmdir \"" + appDir + L"\"";
    STARTUPINFOW si {}; si.cb = sizeof si; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi {};
    if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    } else {
        MoveFileExW(self.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    }
    if (failed) message(L"Alguns arquivos não puderam ser removidos (estão em uso?). Feche o DAW e remova-os manualmente.", MB_ICONWARNING);
    else message(L"G-Major Reborn foi removido.");
    return failed ? 2 : 0;
}

// ------------------------------------------------------------------------------ UI
enum Ids { IdInstall = 100, IdCancel, IdBrowse, IdPath, IdComp0 = 200 };

HWND child(const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, gWnd, (HMENU)(INT_PTR)id, gInst, nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)gFont, TRUE);
    return c;
}

void browse() {
    BROWSEINFOW bi {};
    bi.hwndOwner = gWnd;
    bi.lpszTitle = L"Pasta de instalação do G-Major Reborn";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;
    wchar_t buf[MAX_PATH];
    if (SHGetPathFromIDListW(pidl, buf)) {
        std::wstring p = buf;
        if (p.find(L"G-Major Reborn") == std::wstring::npos) p += L"\\G-Major Reborn";
        SetWindowTextW(gPath, p.c_str());
    }
    CoTaskMemFree(pidl);
}

void onInstall() {
    if (gDone) {
        if (SendMessageW(gLaunch, BM_GETCHECK, 0, 0) == BST_CHECKED) {
            wchar_t buf[MAX_PATH * 2]; GetWindowTextW(gPath, buf, MAX_PATH * 2);
            // via explorer.exe so the app starts with the user's normal (non-elevated) rights
            const std::wstring exe = L"\"" + std::wstring(buf) + L"\\Standalone\\G-Major Reborn.exe\"";
            ShellExecuteW(nullptr, L"open", L"explorer.exe", exe.c_str(), nullptr, SW_SHOWNORMAL);
        }
        DestroyWindow(gWnd);
        return;
    }
    bool comps[CompCount];
    for (int i = 0; i < CompCount; ++i) comps[i] = SendMessageW(gComp[i], BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (!comps[CompVst3] && !comps[CompRacks] && !comps[CompApp]) { message(L"Selecione ao menos um plugin ou o aplicativo.", MB_ICONWARNING); return; }
    wchar_t buf[MAX_PATH * 2];
    GetWindowTextW(gPath, buf, MAX_PATH * 2);
    for (HWND h : { gInstallBtn, gCancelBtn, gBrowseBtn, gPath }) EnableWindow(h, FALSE);
    for (HWND h : gComp) EnableWindow(h, FALSE);
    const bool ok = doInstall(buf, comps);
    if (ok) {
        gDone = true;
        SetWindowTextW(gInstallBtn, L"Concluir");
        EnableWindow(gInstallBtn, TRUE);
        if (comps[CompApp]) ShowWindow(gLaunch, SW_SHOW);
        std::wstring info = L"Instalado.";
        if (comps[CompVst3] || comps[CompRacks]) info += L" No seu DAW, faça uma nova varredura de plugins VST3.";
        setStatus(info);
    } else {
        for (HWND h : { gInstallBtn, gCancelBtn, gBrowseBtn, gPath }) EnableWindow(h, TRUE);
        for (HWND h : gComp) EnableWindow(h, TRUE);
    }
}

LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IdInstall: onInstall(); return 0;
                case IdCancel: DestroyWindow(h); return 0;
                case IdBrowse: browse(); return 0;
                default: break;
            }
            break;
        case WM_CTLCOLORSTATIC: {
            HDC dc = (HDC)wp; SetBkMode(dc, TRANSPARENT);
            return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
        }
        case WM_CLOSE: DestroyWindow(h); return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        default: break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

int runUi(const std::wstring& defaultDir) {
    INITCOMMONCONTROLSEX icc { sizeof icc, ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    NONCLIENTMETRICSW ncm {}; ncm.cbSize = sizeof ncm;
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
    gFont = CreateFontIndirectW(&ncm.lfMessageFont);
    HFONT title;
    { LOGFONTW lf = ncm.lfMessageFont; lf.lfHeight = lf.lfHeight * 3 / 2; lf.lfWeight = FW_BOLD; title = CreateFontIndirectW(&lf); }

    WNDCLASSW wc {};
    wc.lpfnWndProc = wndProc; wc.hInstance = gInst; wc.lpszClassName = L"GMRSetup";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.hIcon = LoadIconW(gInst, MAKEINTRESOURCEW(1));
    RegisterClassW(&wc);
    const int W = 560, H = 456;
    RECT r { 0, 0, W, H };
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE);
    const int ww = r.right - r.left, wh = r.bottom - r.top;
    gWnd = CreateWindowExW(0, L"GMRSetup", (std::wstring(L"Instalar ") + kProduct + L" " + kVersion).c_str(),
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                           (GetSystemMetrics(SM_CXSCREEN) - ww) / 2, (GetSystemMetrics(SM_CYSCREEN) - wh) / 2, ww, wh,
                           nullptr, nullptr, gInst, nullptr);
    HWND t = child(L"STATIC", (std::wstring(kProduct) + L" " + kVersion).c_str(), 0, 20, 16, 520, 30, 0);
    SendMessageW(t, WM_SETFONT, (WPARAM)title, TRUE);
    child(L"STATIC", L"Multi-efeito para guitarra no estilo TC Electronic G\x2022Major: plugin VST3 e aplicativo "
                     L"independente para Windows 11 (64 bits). Escolha o que instalar:", 0, 20, 52, 520, 36, 0);
    const wchar_t* labels[CompCount] = {
        L"Plugin VST3 completo  (Arquivos Comuns\\VST3 \x2014 aparece no seu DAW)",
        L"Racks VST3 individuais  (Gate, EQ, Compressor, Filter, Pitch, Chorus, Delay, Reverb)",
        L"Aplicativo Standalone + ferramentas (gmr_render, gmr_tests)",
        L"Presets em Documentos\\G-Major Reborn\\presets",
        L"Atalho no Menu Iniciar",
        L"Atalho na \x00C1rea de Trabalho" };
    const bool defaults[CompCount] = { true, true, true, true, true, false };
    for (int i = 0; i < CompCount; ++i) {
        gComp[i] = child(L"BUTTON", labels[i], BS_AUTOCHECKBOX | WS_TABSTOP, 30, 96 + i * 26, 500, 22, IdComp0 + i);
        SendMessageW(gComp[i], BM_SETCHECK, defaults[i] ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    child(L"STATIC", L"Pasta do aplicativo:", 0, 20, 264, 300, 18, 0);
    gPath = child(L"EDIT", defaultDir.c_str(), WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 20, 284, 420, 24, IdPath);
    gBrowseBtn = child(L"BUTTON", L"Procurar...", BS_PUSHBUTTON | WS_TABSTOP, 448, 283, 92, 26, IdBrowse);
    gProgress = child(PROGRESS_CLASSW, L"", 0, 20, 326, 520, 18, 0);
    SendMessageW(gProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 1000));
    gStatus = child(L"STATIC", L"Pronto para instalar.", SS_ENDELLIPSIS, 20, 350, 520, 20, 0);
    gLaunch = child(L"BUTTON", L"Abrir o G-Major Reborn agora", BS_AUTOCHECKBOX, 20, 412, 260, 22, 0);
    SendMessageW(gLaunch, BM_SETCHECK, BST_CHECKED, 0);
    ShowWindow(gLaunch, SW_HIDE);
    gInstallBtn = child(L"BUTTON", L"Instalar", BS_DEFPUSHBUTTON | WS_TABSTOP, 340, 410, 96, 28, IdInstall);
    gCancelBtn = child(L"BUTTON", L"Cancelar", BS_PUSHBUTTON | WS_TABSTOP, 444, 410, 96, 28, IdCancel);
    ShowWindow(gWnd, SW_SHOW);
    UpdateWindow(gWnd);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(gWnd, &m)) { TranslateMessage(&m); DispatchMessageW(&m); }
    }
    DeleteObject(gFont); DeleteObject(title);
    return gDone ? 0 : 1;
}

} // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    gInst = inst;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool uninstall = false;
    std::wstring dir;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        if (_wcsicmp(a.c_str(), L"/S") == 0) gSilent = true;
        else if (_wcsicmp(a.c_str(), L"/uninstall") == 0) uninstall = true;
        else if (a.size() > 3 && _wcsnicmp(a.c_str(), L"/D=", 3) == 0) dir = a.substr(3);
    }
    LocalFree(argv);
    int rc;
    if (uninstall) rc = doUninstall();
    else {
        if (dir.empty()) dir = readRegString(HKEY_LOCAL_MACHINE, kUninstKey, L"InstallLocation");
        if (dir.empty()) dir = knownFolder(FOLDERID_ProgramFiles) + L"\\G-Major Reborn";
        if (gSilent) {
            const bool comps[CompCount] = { true, true, true, true, true, false };
            rc = doInstall(dir, comps) ? 0 : 1;
        } else rc = runUi(dir);
    }
    CoUninitialize();
    return rc;
}

// SolarData.cpp
// A lightweight Windows utility that displays real-time solar data banners.
// The application is controlled via a system tray icon, allows users to show or
// hide specific banner windows, and saves/restores user-defined layouts.

#include <string>
#include <vector>
#include <chrono>
#include <windows.h>
#include <gdiplus.h>
#include <winhttp.h>    // Use the modern and robust WinHTTP library
#include <shellapi.h>
#include <ShlObj.h>
#include "resource.h"

#pragma comment (lib, "Gdiplus.lib")
#pragma comment (lib, "winhttp.lib") // Link WinHTTP
#pragma comment (lib, "Shell32.lib")
#pragma comment (lib, "Ole32.lib")
#pragma comment (lib, "Advapi32.lib")

#define WM_TRAYICON (WM_APP + 1)
constexpr UINT_PTR ID_TIMER_REFRESH = 1;
constexpr UINT_PTR ID_TIMER_MOUSE_POLL = 2;

// --- Data Structures ---
struct WindowData {
    std::wstring url;
    Gdiplus::Image* image = nullptr;
    bool isFirstPositioning = true;
    bool isMouseOver = false;
    int naturalWidth = 0;
    int naturalHeight = 0;
};

struct BannerInfo {
    std::wstring title;
    std::wstring url;
    UINT menuId;
    bool showOnStartup;
    HWND hWnd = nullptr;
    bool isVisible = false;
    bool hasSavedPosition = false;
    int savedX = 0;
    int savedY = 0;

    BannerInfo(std::wstring p_title, std::wstring p_url, UINT p_menuId, bool p_showOnStartup)
        : title(std::move(p_title)), url(std::move(p_url)), menuId(p_menuId), showOnStartup(p_showOnStartup) {}
};

// --- Global Application State ---
HINSTANCE hInst;
ULONG_PTR gdiplusToken;
const wchar_t* szAppClass = L"SolarAppController";
const wchar_t* szBannerClass = L"SolarBannerClass";
std::vector<BannerInfo> g_banners;
const wchar_t* g_registryKey = L"Software\\SolarData";
bool g_userHasMovedWindows = false;

// --- Function Prototypes ---
LRESULT CALLBACK    AppWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    BannerWndProc(HWND, UINT, WPARAM, LPARAM);
HWND                CreateBannerWindow(BannerInfo* pBanner);
void                LoadAndDisplayImage(HWND hWnd);
void                PositionAndResizeWindow(HWND hWnd);
void                CreateTrayIcon(HWND hWnd);
void                ShowTrayMenu(HWND hWnd);
HRESULT             DownloadImageWithWinHttp(const std::wstring& url, const std::wstring& filePath);
void                SaveWindowsState();
void                LoadWindowsState();

// --- Entry Point ---
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    hInst = hInstance;
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    // Final, stable list of 100% verified working banners.
    g_banners.emplace_back( L"General Solar Data", L"https://www.hamqsl.com/solarn0nbh.php", 101, true );
    g_banners.emplace_back( L"VHF Conditions", L"https://www.hamqsl.com/solar101vhf.php", 102, true );
    g_banners.emplace_back( L"Sun Map", L"https://www.hamqsl.com/solarmap.php", 105, true );
    g_banners.emplace_back( L"Auroral Oval", L"https://services.swpc.noaa.gov/images/animations/ovation/north/latest.jpg", 108, true );
    
    LoadWindowsState();

    WNDCLASSEXW wcexApp = { sizeof(WNDCLASSEX) };
    wcexApp.lpfnWndProc = AppWndProc;
    wcexApp.hInstance = hInstance;
    wcexApp.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcexApp.lpszClassName = szAppClass;
    RegisterClassExW(&wcexApp);

    WNDCLASSEXW wcexBanner = { sizeof(WNDCLASSEX) };
    wcexBanner.style = CS_HREDRAW | CS_VREDRAW;
    wcexBanner.lpfnWndProc = BannerWndProc;
    wcexBanner.hInstance = hInstance;
    wcexBanner.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcexBanner.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcexBanner.lpszClassName = szBannerClass;
    RegisterClassExW(&wcexBanner);

    HWND hWndApp = CreateWindowW(szAppClass, L"Solar Data Controller", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
    if (!hWndApp) return FALSE;

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return (int)msg.wParam;
}

// --- Window Procedures ---

LRESULT CALLBACK AppWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        CreateTrayIcon(hWnd);
        for (auto& banner : g_banners) {
            if (banner.isVisible) {
                banner.hWnd = CreateBannerWindow(&banner);
            }
        }
        break;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            ShowTrayMenu(hWnd);
        }
        break;

    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            if (wmId == 200) {
                DestroyWindow(hWnd);
                break;
            }
            for (auto& banner : g_banners) {
                if (banner.menuId == wmId) {
                    if (banner.hWnd == nullptr) { 
                        banner.hWnd = CreateBannerWindow(&banner);
                    } else {
                       ShowWindow(banner.hWnd, IsWindowVisible(banner.hWnd) ? SW_HIDE : SW_SHOW);
                    }
                    break;
                }
            }
        }
        break;

    case WM_DESTROY:
        {
            SaveWindowsState();
            for (const auto& banner : g_banners) {
                if (banner.hWnd) DestroyWindow(banner.hWnd);
            }
            NOTIFYICONDATA nid = {};
            nid.cbSize = sizeof(nid);
            nid.hWnd = hWnd;
            nid.uID = 1;
            Shell_NotifyIcon(NIM_DELETE, &nid);
            PostQuitMessage(0);
        }
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK BannerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowData* pData = (WindowData*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    switch (message)
    {
    case WM_NCCREATE:
        {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pCreate->lpCreateParams);
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    case WM_NCCALCSIZE:
        if (wParam == TRUE && pData && !pData->isMouseOver) return 0;
        break;
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDOWN:
        g_userHasMovedWindows = true;
        ReleaseCapture();
        SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        break;
    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;
    case WM_PAINT:
        {
            if (!pData) break;
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            if (pData->image) {
                Gdiplus::Graphics graphics(hdc);
                graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                RECT clientRect; GetClientRect(hWnd, &clientRect);
                graphics.DrawImage(pData->image, 0, 0, clientRect.right, clientRect.bottom);
            }
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_TIMER:
        switch (wParam) {
        case ID_TIMER_REFRESH: LoadAndDisplayImage(hWnd); break;
        case ID_TIMER_MOUSE_POLL: {
            if (!pData) break;
            POINT cursorPos; GetCursorPos(&cursorPos);
            RECT windowRect; GetWindowRect(hWnd, &windowRect);
            bool currentlyOver = PtInRect(&windowRect, cursorPos);
            if (currentlyOver != pData->isMouseOver) {
                pData->isMouseOver = currentlyOver;
                SetWindowPos(hWnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            }
        } break;
        } break;
    case WM_DESTROY:
        if (pData) {
            if (pData->image) delete pData->image;
            delete pData;
            SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
        }
        for (auto& banner : g_banners) {
            if (banner.hWnd == hWnd) {
                banner.hWnd = nullptr;
                break;
            }
        }
        KillTimer(hWnd, ID_TIMER_REFRESH);
        KillTimer(hWnd, ID_TIMER_MOUSE_POLL);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// --- Helper Functions ---

HWND CreateBannerWindow(BannerInfo* pBanner)
{
    WindowData* pData = new WindowData();
    pData->url = pBanner->url;

    HWND hWnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_COMPOSITED, szBannerClass, pBanner->title.c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInst, pData);

    if (hWnd) {
        const int refreshInterval = 10 * 60 * 1000;
        SetTimer(hWnd, ID_TIMER_REFRESH, refreshInterval, nullptr);
        SetTimer(hWnd, ID_TIMER_MOUSE_POLL, 250, nullptr);
        LoadAndDisplayImage(hWnd);
    }
    return hWnd;
}

static int nextWindowYOffset = 0;
void PositionAndResizeWindow(HWND hWnd)
{
    WindowData* pData = (WindowData*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (!pData || pData->naturalWidth == 0) return;
    
    LONG style = WS_OVERLAPPEDWINDOW;
    RECT windowRect = { 0, 0, pData->naturalWidth, pData->naturalHeight };
    AdjustWindowRect(&windowRect, style, FALSE);
    int winW = windowRect.right - windowRect.left;
    int winH = windowRect.bottom - windowRect.top;

    int x, y;
    BannerInfo* pBannerInfo = nullptr;
    for(auto& banner : g_banners) {
        if (banner.hWnd == hWnd) {
            pBannerInfo = &banner;
            break;
        }
    }
    
    if (pBannerInfo && pBannerInfo->hasSavedPosition) {
        x = pBannerInfo->savedX;
        y = pBannerInfo->savedY;
    } else {
        RECT workArea; SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
        
        if (pBannerInfo && pBannerInfo->title == L"Auroral Oval") {
            x = (workArea.right - workArea.left - winW) / 2 + workArea.left;
            y = (workArea.bottom - workArea.top - winH) / 2 + workArea.top;
        } else {
            x = workArea.right - winW;
            y = workArea.bottom - winH - nextWindowYOffset;
            nextWindowYOffset += winH;
        }
    }
    
    // This is the crucial step: show the window after positioning it.
    SetWindowPos(hWnd, HWND_TOPMOST, x, y, winW, winH, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void LoadAndDisplayImage(HWND hWnd)
{
    WindowData* pData = (WindowData*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (!pData) return;

    if (!pData->isFirstPositioning && !IsWindowVisible(hWnd)) {
        return;
    }

    PWSTR path = nullptr;
    std::wstring finalPath;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path))) {
        finalPath = path;
        finalPath += L"\\SolarData";
        CreateDirectoryW(finalPath.c_str(), nullptr);
        finalPath += L"\\banner_" + std::to_wstring((uintptr_t)hWnd) + L".tmp";
        CoTaskMemFree(path);
    } else {
        finalPath = L"banner_" + std::to_wstring((uintptr_t)hWnd) + L".tmp";
    }

    std::wstring url = pData->url;
    url += L"?t=" + std::to_wstring(std::chrono::system_clock::now().time_since_epoch().count());

    if (SUCCEEDED(DownloadImageWithWinHttp(url, finalPath))) {
        if (pData->image) delete pData->image;
        pData->image = Gdiplus::Image::FromFile(finalPath.c_str());
        
        if (pData->image && pData->image->GetLastStatus() == Gdiplus::Ok) {
            pData->naturalWidth = pData->image->GetWidth();
            pData->naturalHeight = pData->image->GetHeight();
            
            if (pData->isFirstPositioning) {
                pData->isFirstPositioning = false;
                PositionAndResizeWindow(hWnd);
            }
            
            InvalidateRect(hWnd, nullptr, FALSE);
        } else {
            if (pData->image) { delete pData->image; pData->image = nullptr; }
        }
    }
}

HRESULT DownloadImageWithWinHttp(const std::wstring& url, const std::wstring& filePath)
{
    HRESULT hr = E_FAIL;
    URL_COMPONENTS urlComp = { sizeof(urlComp) };
    wchar_t hostName[256];
    wchar_t urlPath[2048];
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 2048;
    urlComp.dwSchemeLength = 1;

    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp)) {
        return hr;
    }

    const wchar_t* userAgent = L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36";
    HINTERNET hSession = WinHttpOpen(userAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return hr;

    HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return hr; }
    
    std::wstring refererHeader = L"Referer: https://www.hamqsl.com/\r\n";
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
    
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return hr; }

    WinHttpAddRequestHeaders(hRequest, refererHeader.c_str(), (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);

    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        if (WinHttpReceiveResponse(hRequest, NULL)) {
            FILE* pFile = nullptr;
            if (_wfopen_s(&pFile, filePath.c_str(), L"wb") == 0 && pFile != nullptr) {
                char buffer[4096];
                DWORD bytesRead = 0;
                while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                    fwrite(buffer, 1, bytesRead, pFile);
                }
                fclose(pFile);
                hr = S_OK;
            }
        }
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return hr;
}

void CreateTrayIcon(HWND hWnd) {
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_SOLARDATA));
    wcscpy_s(nid.szTip, L"Solar Data Banners");
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void ShowTrayMenu(HWND hWnd) {
    POINT pt; GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    for (auto& banner : g_banners) {
        UINT flags = MF_STRING;
        if (banner.hWnd) {
            banner.isVisible = IsWindowVisible(banner.hWnd);
        }
        if (banner.isVisible) {
            flags |= MF_CHECKED;
        }
        AppendMenuW(hMenu, flags, banner.menuId, banner.title.c_str());
    }
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 200, L"Exit");
    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, nullptr);
    PostMessage(hWnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

void SaveWindowsState() {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, g_registryKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD userMoved = g_userHasMovedWindows ? 1 : 0;
        RegSetValueExW(hKey, L"UserHasMoved", 0, REG_DWORD, (const BYTE*)&userMoved, sizeof(userMoved));

        for (const auto& banner : g_banners) {
            HKEY hSubKey;
            if(RegCreateKeyExW(hKey, banner.title.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hSubKey, NULL) == ERROR_SUCCESS) {
                if (banner.hWnd) {
                    RECT rect;
                    GetWindowRect(banner.hWnd, &rect);
                    DWORD isVisible = IsWindowVisible(banner.hWnd) ? 1 : 0;
                    RegSetValueExW(hSubKey, L"Visible", 0, REG_DWORD, (const BYTE*)&isVisible, sizeof(isVisible));
                    RegSetValueExW(hSubKey, L"PosX", 0, REG_DWORD, (const BYTE*)&rect.left, sizeof(rect.left));
                    RegSetValueExW(hSubKey, L"PosY", 0, REG_DWORD, (const BYTE*)&rect.top, sizeof(rect.top));
                }
                RegCloseKey(hSubKey);
            }
        }
        RegCloseKey(hKey);
    }
}

void LoadWindowsState() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, g_registryKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD userMoved = 0;
        DWORD dwSize = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"UserHasMoved", NULL, NULL, (LPBYTE)&userMoved, &dwSize) == ERROR_SUCCESS) {
            g_userHasMovedWindows = (userMoved == 1);
        }

        for (auto& banner : g_banners) {
            HKEY hSubKey;
            if(RegOpenKeyExW(hKey, banner.title.c_str(), 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
                DWORD isVisible = 0, posX = 0, posY = 0;
                dwSize = sizeof(DWORD);
                if (RegQueryValueExW(hSubKey, L"Visible", NULL, NULL, (LPBYTE)&isVisible, &dwSize) == ERROR_SUCCESS &&
                    RegQueryValueExW(hSubKey, L"PosX", NULL, NULL, (LPBYTE)&posX, &dwSize) == ERROR_SUCCESS &&
                    RegQueryValueExW(hSubKey, L"PosY", NULL, NULL, (LPBYTE)&posY, &dwSize) == ERROR_SUCCESS)
                {
                    banner.isVisible = (isVisible == 1);
                    banner.savedX = posX;
                    banner.savedY = posY;
                    banner.hasSavedPosition = true;
                } else {
                    banner.isVisible = banner.showOnStartup;
                }
                RegCloseKey(hSubKey);
            } else {
                banner.isVisible = banner.showOnStartup;
            }
        }
        RegCloseKey(hKey);
    } else {
        for (auto& banner : g_banners) {
            banner.isVisible = banner.showOnStartup;
        }
    }
}

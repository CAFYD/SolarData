// SolarData.cpp
// Final version with Tray Icon Menu. Pre-selects and shows working banners on startup.

#include <windows.h>
#include <gdiplus.h>    // For GDI+
#include <urlmon.h>     // For URLDownloadToFileW
#include <shellapi.h>   // For Shell_NotifyIcon
#include <string>
#include <vector>
#include <chrono>       // For generating a timestamp
#include "resource.h"   // For the Icon ID (e.g., IDI_SOLARDATA)

#pragma comment (lib, "Gdiplus.lib")
#pragma comment (lib, "Urlmon.lib")
#pragma comment (lib, "Shell32.lib")

// --- Custom Message for the Tray Icon ---
#define WM_TRAYICON (WM_APP + 1)

// --- Data Structures ---
struct WindowData {
    std::wstring url;
    Gdiplus::Image* image = nullptr;
    bool isFirstPositioning = true;
    bool isMouseOver = false;
};

struct BannerInfo {
    std::wstring title;
    std::wstring url;
    UINT menuId;
    HWND hWnd = nullptr;
};

// --- Global Application State & Constants ---
HINSTANCE           hInst;
ULONG_PTR           gdiplusToken;
const wchar_t* szAppClass = L"SolarAppController";
const wchar_t* szBannerClass = L"SolarBannerClass";
std::vector<BannerInfo> g_banners;

const int           refreshMinutes = 10;
const int           zoomFactor = 1;
const UINT_PTR      ID_TIMER_REFRESH = 1;
const UINT_PTR      ID_TIMER_MOUSE_POLL = 2;

// --- Function Prototypes ---
LRESULT CALLBACK    AppWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    BannerWndProc(HWND, UINT, WPARAM, LPARAM);
HWND                CreateBannerWindow(BannerInfo* pBanner);
void                LoadAndDisplayImage(HWND hWnd);
void                PositionAndResizeWindow(HWND hWnd, int imgW, int imgH);
void                CreateTrayIcon(HWND hWnd);
void                ShowTrayMenu(HWND hWnd);

// --- Entry Point ---
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    hInst = hInstance;
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    // --- DEFINE BANNERS HERE (ho rimosso quelli non funzionanti) ---
    g_banners = {
        { L"General Solar Data", L"https://www.hamqsl.com/solarn0nbh.php", 101 },
        { L"VHF Conditions",     L"https://www.hamqsl.com/solar101vhf.php", 102 },
        { L"Sun Map",            L"https://www.hamqsl.com/solarmap.php",     105 }
    };

    WNDCLASSEXW wcexApp = { sizeof(WNDCLASSEX), 0, AppWndProc, 0, 0, hInstance, nullptr, nullptr, (HBRUSH)(COLOR_WINDOW + 1), nullptr, szAppClass, nullptr };
    RegisterClassExW(&wcexApp);

    WNDCLASSEXW wcexBanner = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, BannerWndProc, 0, 0, hInstance, nullptr, LoadCursor(nullptr, IDC_ARROW), (HBRUSH)GetStockObject(BLACK_BRUSH), nullptr, szBannerClass, nullptr };
    RegisterClassExW(&wcexBanner);

    HWND hWndApp = CreateWindowW(szAppClass, L"Solar Data Controller", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
    if (!hWndApp) return FALSE;

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return (int)msg.wParam;
}

// --- Window Procedure for the INVISIBLE CONTROLLER WINDOW ---
LRESULT CALLBACK AppWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        CreateTrayIcon(hWnd);
        // --- MODIFICA: Crea e mostra tutti i banner all'avvio ---
        for (auto& banner : g_banners) {
            banner.hWnd = CreateBannerWindow(&banner);
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
        if (wmId == 200) { // Exit command
            DestroyWindow(hWnd);
            break;
        }

        for (auto& banner : g_banners) {
            if (banner.menuId == wmId) {
                if (banner.hWnd == nullptr) { // Should not happen with pre-selection, but good for safety
                    banner.hWnd = CreateBannerWindow(&banner);
                }
                else {
                    ShowWindow(banner.hWnd, IsWindowVisible(banner.hWnd) ? SW_HIDE : SW_SHOW);
                }
                break;
            }
        }
    }
    break;

    case WM_DESTROY:
    {
        for (auto& banner : g_banners) {
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

// --- Window Procedure for the VISIBLE BANNER WINDOWS ---
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

    case WM_ERASEBKGND:
        return 1;

    case WM_LBUTTONDOWN:
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
            RECT clientRect;
            GetClientRect(hWnd, &clientRect);
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
                SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            }
        } break;
        } break;

    case WM_DESTROY:
        if (pData) {
            if (pData->image) delete pData->image;
            delete pData;
            SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
        }
        // When a banner window is truly destroyed, update its handle in the global list
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

    HWND hWnd = CreateWindowExW(WS_EX_APPWINDOW | WS_EX_COMPOSITED, szBannerClass, pBanner->title.c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInst, pData);

    if (hWnd) {
        LoadAndDisplayImage(hWnd);
        SetTimer(hWnd, ID_TIMER_REFRESH, refreshMinutes * 60 * 1000, nullptr);
        SetTimer(hWnd, ID_TIMER_MOUSE_POLL, 250, nullptr);
    }
    return hWnd;
}

static int nextWindowYOffset = 0;
void PositionAndResizeWindow(HWND hWnd, int imgW, int imgH)
{
    WindowData* pData = (WindowData*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (!pData) return;

    LONG style = WS_OVERLAPPEDWINDOW;
    RECT windowRect = { 0, 0, imgW * zoomFactor, imgH * zoomFactor };
    AdjustWindowRect(&windowRect, style, FALSE);
    int winW = windowRect.right - windowRect.left;
    int winH = windowRect.bottom - windowRect.top;

    int x, y;
    if (pData->isFirstPositioning) {
        RECT workArea; SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
        x = workArea.right - winW;
        y = workArea.bottom - winH - nextWindowYOffset;
        nextWindowYOffset += winH;
        pData->isFirstPositioning = false;
    }
    else {
        RECT currentRect; GetWindowRect(hWnd, &currentRect);
        x = currentRect.left; y = currentRect.top;
    }
    SetWindowPos(hWnd, HWND_TOPMOST, x, y, winW, winH, SWP_SHOWWINDOW | SWP_NOACTIVATE);
}

void LoadAndDisplayImage(HWND hWnd)
{
    WindowData* pData = (WindowData*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (!pData) return;
    std::wstring tempFile = L"banner_" + std::to_wstring((uintptr_t)hWnd) + L".png";
    std::wstring url = pData->url;
    url += L"?t=" + std::to_wstring(std::chrono::system_clock::now().time_since_epoch().count());

    if (SUCCEEDED(URLDownloadToFileW(nullptr, url.c_str(), tempFile.c_str(), 0, nullptr))) {
        if (pData->image) delete pData->image;
        pData->image = Gdiplus::Image::FromFile(tempFile.c_str());
        if (pData->image && pData->image->GetLastStatus() == Gdiplus::Ok) {
            PositionAndResizeWindow(hWnd, pData->image->GetWidth(), pData->image->GetHeight());
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        else {
            if (pData->image) { delete pData->image; pData->image = nullptr; }
        }
    }
}

void CreateTrayIcon(HWND hWnd) {
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_SOLARDATA)); // Usa l'ID corretto
    wcscpy_s(nid.szTip, L"Solar Data Banners");
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void ShowTrayMenu(HWND hWnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();

    for (const auto& banner : g_banners) {
        UINT flags = MF_STRING;
        if (banner.hWnd && IsWindowVisible(banner.hWnd)) {
            flags |= MF_CHECKED;
        }
        AppendMenuW(hMenu, flags, banner.menuId, banner.title.c_str());
    }
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 200, L"Esci");

    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
    PostMessage(hWnd, WM_NULL, 0, 0);

    DestroyMenu(hMenu);
}
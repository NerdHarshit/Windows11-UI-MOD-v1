#include <windows.h>
#include <WebView2.h>
#include <wrl.h>
#include <objbase.h>
#include <string>
#include <psapi.h>
#include <stdint.h>
#include <vector>   // 🔹 NEW: for multiple widgets

using namespace Microsoft::WRL;

/* =========================================================
   🔹 NEW: WidgetWindow structure
   Each widget gets its own HWND + WebView + Controller
   ========================================================= */
struct WidgetWindow {
    HWND hwnd;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    std::wstring url;
    bool isSystemWidget; // 🔹 to decide if timer/system stats apply
};

std::vector<WidgetWindow*> widgets;   // 🔹 holds all widgets

#define SYSTEM_TIMER 1
HWND systemWidgetHwnd = nullptr;  // 🔹 NEW

/* ================== SYSTEM STATS (UNCHANGED) ================== */

ULONGLONG prevIdle = 0;
ULONGLONG prevKernel = 0;
ULONGLONG prevUser = 0;

int GetCPUUsage()
{
    FILETIME idle, kernel, user;
    GetSystemTimes(&idle, &kernel, &user);

    ULONGLONG i = ((ULONGLONG)idle.dwHighDateTime << 32) | idle.dwLowDateTime;
    ULONGLONG k = ((ULONGLONG)kernel.dwHighDateTime << 32) | kernel.dwLowDateTime;
    ULONGLONG u = ((ULONGLONG)user.dwHighDateTime << 32) | user.dwLowDateTime;

    if (prevIdle == 0)
    {
        prevIdle = i;
        prevKernel = k;
        prevUser = u;
        return 0;
    }

    ULONGLONG idleDiff = i - prevIdle;
    ULONGLONG kernelDiff = k - prevKernel;
    ULONGLONG userDiff = u - prevUser;

    ULONGLONG total = kernelDiff + userDiff;

    prevIdle = i;
    prevKernel = k;
    prevUser = u;

    if (total == 0) return 0;

    return (int)(100 - (idleDiff * 100 / total));
}

int GetRAMUsage()
{
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    return (int)mem.dwMemoryLoad;
}

int GetDiskUsage()
{
    ULARGE_INTEGER freeBytes, totalBytes;
    GetDiskFreeSpaceExW(L"C:\\", &freeBytes, &totalBytes, nullptr);

    double used = 1.0 - ((double)freeBytes.QuadPart / totalBytes.QuadPart);
    return (int)(used * 100);
}

/* =========================================================
   🔹 MODIFIED: Send data to ONLY system widgets
   ========================================================= */
void SendSystemData()
{
    for (auto widget : widgets)
    {
        if (!widget->isSystemWidget || !widget->webview)
            continue;

        int cpu = GetCPUUsage();
        int ram = GetRAMUsage();
        int disk = GetDiskUsage();

        std::wstring json =
            L"{\"cpu\":" + std::to_wstring(cpu) +
            L",\"ram\":" + std::to_wstring(ram) +
            L",\"disk\":" + std::to_wstring(disk) + L"}";

        widget->webview->PostWebMessageAsJson(json.c_str());
    }
}

/* =========================================================
   🔹 MODIFIED: Shared WndProc for ALL widgets
   Uses GWLP_USERDATA to map HWND → WidgetWindow
   ========================================================= */
LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
    WidgetWindow* widget =
        (WidgetWindow*)GetWindowLongPtr(h, GWLP_USERDATA);

    switch (msg)
    {
    case WM_SIZE:
        if (widget && widget->controller)
        {
            RECT r;
            GetClientRect(h, &r);
            widget->controller->put_Bounds(r);
        }
        return 0;

    case WM_TIMER:
        if (w == SYSTEM_TIMER)
            SendSystemData();
        return 0;

    /*case WM_LBUTTONDOWN:
        ReleaseCapture();
        SendMessage(h, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
        return 0;*/

    case WM_DESTROY:
        PostQuitMessage(0); // (we'll refine this later)
        return 0;
    }

    return DefWindowProc(h, msg, w, l);
}

/* =========================================================
   🔹 NEW: Function to create one widget window
   ========================================================= */
void CreateWidget(
    HINSTANCE hInst,
    ICoreWebView2Environment* env,
    const wchar_t* url,
    int x, int y, int w, int h,
    bool isSystemWidget)
{
    WidgetWindow* widget = new WidgetWindow();
    widget->url = url;
    widget->isSystemWidget = isSystemWidget;

    widget->hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        L"WidgetWindow",
        L"",
        WS_POPUP | WS_VISIBLE,
        x, y, w, h,
        nullptr, nullptr, hInst, nullptr
    );

    SetWindowLongPtr(widget->hwnd, GWLP_USERDATA, (LONG_PTR)widget);

    if (isSystemWidget)
{
    systemWidgetHwnd = widget->hwnd;
}

    env->CreateCoreWebView2Controller(
        widget->hwnd,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [widget](HRESULT, ICoreWebView2Controller* ctrl) -> HRESULT
            {
                widget->controller = ctrl;
                widget->controller->get_CoreWebView2(&widget->webview);

                ComPtr<ICoreWebView2Settings> settings;
                widget->webview->get_Settings(&settings);
                settings->put_IsWebMessageEnabled(TRUE);

                RECT r;
                GetClientRect(widget->hwnd, &r);
                widget->controller->put_Bounds(r);

                widget->webview->Navigate(widget->url.c_str());
                return S_OK;
            }
        ).Get()
    );

    widgets.push_back(widget);
}

/* =========================================================
   🔹 MAIN
   ========================================================= */
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    /* 🔹 MODIFIED: One window class for ALL widgets */
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"WidgetWindow";
    RegisterClassW(&wc);

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hInst](HRESULT, ICoreWebView2Environment* env) -> HRESULT
            {
                // 🔹 SYSTEM WIDGET (needs timer)
                CreateWidget(
                    hInst, env,
                    L"C:\\Users\\HARSHIT\\Desktop\\windows11 mod\\Prism\\widgets\\system\\system.html",
                    100, 100, 300, 250,
                    true
                );

                // 🔹 WEATHER WIDGET (JS-only)
                CreateWidget(
                    hInst, env,
                    L"C:\\Users\\HARSHIT\\Desktop\\windows11 mod\\Prism\\widgets\\weather\\weather.html",
                    450, 100, 250, 250,
                    false
                );

                // 🔹 Start system update timer ONCE
                SetTimer(systemWidgetHwnd, SYSTEM_TIMER, 1000, nullptr);

                return S_OK;
            }
        ).Get()
    );

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

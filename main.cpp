#include <windows.h>
#include <WebView2.h>
#include <wrl.h>
#include <objbase.h>
#include <string>
#include <psapi.h>
#include <stdint.h>

using namespace Microsoft::WRL;

HWND hwnd = nullptr;
ComPtr<ICoreWebView2Controller> controller;
ComPtr<ICoreWebView2> webview;

#define SYSTEM_TIMER 1

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
    return (int)mem.dwMemoryLoad;   // already percent
}

int GetDiskUsage()
{
    ULARGE_INTEGER freeBytes, totalBytes;
    GetDiskFreeSpaceExW(L"C:\\", &freeBytes, &totalBytes, nullptr);

    double used = 1.0 - ((double)freeBytes.QuadPart / totalBytes.QuadPart);
    return (int)(used * 100);
}

// ------------------------------
// Send data into JavaScript
// ------------------------------
void SendFakeSystemData()
{
    if (!webview) return;

    int cpu = GetCPUUsage();
    int ram = GetRAMUsage();
    int disk = GetDiskUsage();

    std::wstring json =
        L"{\"cpu\":" + std::to_wstring(cpu) +
        L",\"ram\":" + std::to_wstring(ram) +
        L",\"disk\":" + std::to_wstring(disk) + L"}";

    webview->PostWebMessageAsJson(json.c_str());
}



LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
    switch (msg)
    {
    case WM_SIZE:
        if (controller)
        {
            RECT r;
            GetClientRect(h, &r);
            controller->put_Bounds(r);
        }
        return 0;

    case WM_TIMER:
        if (w == SYSTEM_TIMER)
            SendFakeSystemData();
        return 0;

    case WM_LBUTTONDOWN:
        ReleaseCapture();
        SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, SYSTEM_TIMER);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(h, msg, w, l);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"WebView2TestWindow";
    RegisterClassW(&wc);

    hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"",
        WS_POPUP | WS_VISIBLE,
        100, 100, 300, 250,
        NULL, NULL, hInst, NULL
    );

    ShowWindow(hwnd, SW_SHOW);

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT
            {
                env->CreateCoreWebView2Controller(
                    hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [](HRESULT hr, ICoreWebView2Controller* ctrl) -> HRESULT
                        {
                            controller = ctrl;
                            controller->get_CoreWebView2(&webview);

                            // Enable JS messaging
                            ComPtr<ICoreWebView2Settings> settings;
                            webview->get_Settings(&settings);
                            settings->put_IsWebMessageEnabled(TRUE);

                            webview->OpenDevToolsWindow();

                            RECT bounds;
                            GetClientRect(hwnd, &bounds);
                            controller->put_Bounds(bounds);

                            // Navigate to widget
                            webview->Navigate(
                                L"C:\\Users\\HARSHIT\\Desktop\\windows11 mod\\Prism\\widgets\\system\\system.html"
                            );

                            // Wait until page is loaded before starting timer
                            webview->add_NavigationCompleted(
                                Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                    [](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT
                                    {
                                        SetTimer(hwnd, SYSTEM_TIMER, 1000, nullptr);
                                        return S_OK;
                                    }
                                ).Get(),
                                nullptr
                            );

                            return S_OK;
                        }
                    ).Get()
                );
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

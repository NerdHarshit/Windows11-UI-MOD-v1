#include <windows.h>
#include <WebView2.h>
#include <wrl.h>
#include <objbase.h>
#include <string>
#include <psapi.h>
#include <stdint.h>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <shellapi.h>
#include "Prism\resource.h"


std::wstring settingsFile = L"settings.txt";
std::wstring currentTheme = L"theme-frost";

using namespace Microsoft::WRL;

/* =========================================================
   Widget structure
   ========================================================= */
struct WidgetWindow
{
    HWND hwnd;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    std::wstring url;
    bool isSystemWidget;
    bool isControlPanel;
    bool isTaskbar;
};

std::vector<WidgetWindow *> widgets;
std::unordered_map<std::wstring, WidgetWindow *> widgetMap;

#define SYSTEM_TIMER 1
HWND systemWidgetHwnd = nullptr;

std::wstring GetExeDir()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring p(path);
    return p.substr(0, p.find_last_of(L"\\/"));
}


void BroadcastTheme()
{
    for (auto &pair : widgetMap)
    {
        WidgetWindow *w = pair.second;
        if (!w || !w->webview)
            continue;
        if (w->isTaskbar)
            continue; // skip taskbar for v1

        std::wstring js =
            L"setTheme('" + currentTheme + L"');";

        w->webview->ExecuteScript(js.c_str(), nullptr);
    }
}

RECT GetRealTaskbarRect()
{
    RECT r = {};
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (taskbar)
    {
        GetWindowRect(taskbar, &r);
    }
    return r;
}

RECT GetPrimaryTaskbarRect()
{
    RECT r = {};
    APPBARDATA abd = {};
    abd.cbSize = sizeof(abd);

    if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd))
    {
        r = abd.rc;
    }
    return r;
}

RECT GetTaskbarRectPrimary()
{
    RECT r = {};
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (taskbar)
        GetWindowRect(taskbar, &r);
    return r;
}

RECT GetTaskbarRect()
{
    RECT rc = {0};
    APPBARDATA abd = {};
    abd.cbSize = sizeof(abd);

    if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd))
    {
        rc = abd.rc;
    }

    return rc;
}
RECT GetPrimaryMonitorRect()
{
    HMONITOR hMon = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);

    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(hMon, &mi);

    return mi.rcMonitor; // 🔹 full monitor (includes taskbar)
}

POINT ClampToMonitor(int x, int y)
{
    POINT p = {x, y};
    HMONITOR mon = MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST);

    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(mon, &mi);

    if (x < mi.rcWork.left)
        x = mi.rcWork.left;
    if (y < mi.rcWork.top)
        y = mi.rcWork.top;
    if (x > mi.rcWork.right - 100)
        x = mi.rcWork.right - 100;
    if (y > mi.rcWork.bottom - 100)
        y = mi.rcWork.bottom - 100;

    return {x, y};
}

void SaveWidgetPositions()
{
    std::wofstream file(settingsFile, std::ios::app); // append
    if (!file.is_open())
        return;

    for (auto &pair : widgetMap)
    {
        WidgetWindow *w = pair.second;
        if (!w || !w->hwnd)
            continue;

        RECT r;
        GetWindowRect(w->hwnd, &r);

        file << pair.first << L"_x=" << r.left << std::endl;
        file << pair.first << L"_y=" << r.top << std::endl;
    }

    file.close();
}

void SaveWidgetState()
{
    std::wofstream file(settingsFile);
    if (!file.is_open())
        return;
    file << L"theme=" << currentTheme << std::endl;

    for (auto &pair : widgetMap)
    {
        WidgetWindow *w = pair.second;
        bool visible = IsWindowVisible(w->hwnd);
        file << pair.first << L"=" << (visible ? 1 : 0) << std::endl;
    }

    file.close();
    SaveWidgetPositions();
}

void LoadWidgetState()
{
    std::wifstream file(settingsFile);
    if (!file.is_open())
        return;

    std::unordered_map<std::wstring, int> values;

    std::wstring line;
    while (std::getline(file, line))
    {
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos)
            continue;

        std::wstring key = line.substr(0, eq);
        std::wstring value = line.substr(eq + 1);

        if (key == L"theme")
        {
            currentTheme = value;
            continue;
        }

        values[key] = std::stoi(value);
    }

    file.close();

    for (auto &pair : widgetMap)
    {
        std::wstring name = pair.first;
        WidgetWindow *w = pair.second;

        if (!w || !w->hwnd)
            continue;

        if (name == L"taskbar")
            continue; // skip taskbar widget

        // visibility
        if (values.count(name))
        {
            bool visible = values[name] == 1;
            ShowWindow(w->hwnd, visible ? SW_SHOW : SW_HIDE);
            if (w->controller)
                w->controller->put_IsVisible(visible ? TRUE : FALSE);
        }

        // position X
        if (values.count(name + L"_x") && values.count(name + L"_y"))
        {
            int x = values[name + L"_x"];
            int y = values[name + L"_y"];

            POINT p = ClampToMonitor(x, y);

            SetWindowPos(
                w->hwnd,
                nullptr,
                p.x, p.y,
                0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
}

/* ================== SYSTEM STATS ================== */

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

    if (total == 0)
        return 0;

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
   Send system data
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
   Window procedure
   ========================================================= */
LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
    WidgetWindow *widget =
        (WidgetWindow *)GetWindowLongPtr(h, GWLP_USERDATA);

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

    // Drag ONLY widgets
    case WM_LBUTTONDOWN:
        if (widget && !widget->isControlPanel && !widget->isTaskbar)
        {
            ReleaseCapture();
            SendMessage(h, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
            SaveWidgetState();
            return 0;
        }
        break;

    case WM_DISPLAYCHANGE:
    {
        for (auto &pair : widgetMap)
        {
            WidgetWindow *w = pair.second;
            if (!w || !w->isTaskbar)
                continue;

            RECT tb = GetTaskbarRect();

            SetWindowPos(
                w->hwnd,
                HWND_TOPMOST,
                tb.left,
                tb.top,
                tb.right - tb.left,
                tb.bottom - tb.top,
                SWP_NOACTIVATE);
        }
        return 0;
    }

    case WM_CLOSE:
        if (widget)
        {
            if (widget->isControlPanel)
            {
                // 🔹 Control panel closed → exit app
                SaveWidgetState(); // save state + positions
                PostQuitMessage(0);
            }
            else
            {
                // 🔹 Widget closed → just hide it
                ShowWindow(h, SW_HIDE);

                // also hide WebView2 controller
                if (widget->controller)
                    widget->controller->put_IsVisible(FALSE);

                SaveWidgetState(); // remember it's hidden
            }
            return 0;
        }
        break;

    case WM_DESTROY:
        return 0; // do nothing here
    }

    return DefWindowProc(h, msg, w, l);
}

/* =========================================================
   Create widget / control panel window
   ========================================================= */
void CreateWidget(
    HINSTANCE hInst,
    ICoreWebView2Environment *env,
    const wchar_t *name, // 🔹 widget name
    const std::wstring& url,
    int x, int y, int w, int h,
    bool isSystemWidget,
    bool isControlPanel,
    bool isTaskbar)
{
    WidgetWindow *widget = new WidgetWindow();
    widget->url = url;
    widget->isSystemWidget = isSystemWidget;
    widget->isControlPanel = isControlPanel;
    widget->isTaskbar = (std::wstring(name) == L"taskbar");

    DWORD style = WS_POPUP | WS_VISIBLE;
    DWORD exStyle = WS_EX_TOOLWINDOW; //| WS_EX_LAYERED;

    HICON hIcon = (HICON)LoadImageW(
    GetModuleHandleW(nullptr),
    MAKEINTRESOURCEW(101),   // 👈 IMPORTANT: W version
    IMAGE_ICON,
    32, 32,
    LR_DEFAULTCOLOR
);

SendMessage(widget->hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
SendMessage(widget->hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);



    if (std::wstring(name) == L"taskbar")
    {
        exStyle |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
        // 🔹 WS_EX_TRANSPARENT = click-through
        // 🔹 WS_EX_LAYERED = required for transparency
    }

    if (isControlPanel)
    {
        style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        exStyle = 0; // show in taskbar
    }

    widget->hwnd = CreateWindowExW(
        exStyle,
        L"WidgetWindow",
        isControlPanel ? L"Prism Control Panel" : L"",
        style,
        x, y, w, h,
        nullptr, nullptr, hInst, nullptr);

    if (std::wstring(name) == L"taskbar")
    {
        SetLayeredWindowAttributes(widget->hwnd, 0, 255, LWA_ALPHA);
        // 🔹 ensures window is visible but mouse passes through
    }

    /*if(!isControlPanel)
    {
        SetLayeredWindowAttributes(widget->hwnd,0,255,LWA_ALPHA);
    }*/

    SetWindowLongPtr(widget->hwnd, GWLP_USERDATA, (LONG_PTR)widget);

    if (isSystemWidget)
        systemWidgetHwnd = widget->hwnd;

    widgetMap[name] = widget;

    if (std::wstring(name) == L"taskbar")
    {
        SetWindowPos(
            widget->hwnd,
            HWND_TOPMOST,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    env->CreateCoreWebView2Controller(
        widget->hwnd,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [widget, name](HRESULT, ICoreWebView2Controller *ctrl) -> HRESULT
            {
                widget->controller = ctrl;
                widget->controller->get_CoreWebView2(&widget->webview);

                ComPtr<ICoreWebView2Settings> settings;
                widget->webview->get_Settings(&settings);
                settings->put_IsWebMessageEnabled(TRUE);

                ComPtr<ICoreWebView2Controller2> controller2;
                if (SUCCEEDED(widget->controller.As(&controller2)))
                {
                    controller2->put_DefaultBackgroundColor({0, 0, 0, 0});
                }

                RECT r;
                GetClientRect(widget->hwnd, &r);
                widget->controller->put_Bounds(r);

                widget->webview->Navigate(widget->url.c_str());

                // 🔹 Apply theme after widget loads
                widget->webview->add_NavigationCompleted(
                    Callback<ICoreWebView2NavigationCompletedEventHandler>(
                        [widget](ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *) -> HRESULT
                        {
                            if (!widget->isControlPanel && !widget->isTaskbar)
                            {
                                std::wstring js =
                                    L"setTheme('" + currentTheme + L"');";

                                widget->webview->ExecuteScript(js.c_str(), nullptr);
                            }
                            return S_OK;
                        })
                        .Get(),
                    nullptr);

                // Disable interaction ONLY for widgets
                if (!widget->isControlPanel)
                {
                    HWND child = GetWindow(widget->hwnd, GW_CHILD);
                    if (child)
                        EnableWindow(child, FALSE);
                }

                // 🔹 Listen for messages from control panel
                if (widget->isControlPanel)
                {

                    // 🔹 Sync checkbox UI AFTER control panel page is loaded
                    // 🔹 Sync checkbox UI AFTER control panel page is loaded
                    widget->webview->add_NavigationCompleted(
                        Callback<ICoreWebView2NavigationCompletedEventHandler>(
                            [widget](ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *) -> HRESULT
                            {
                                bool allVisible = true; // 🔹 track master checkbox

                                for (auto &w : widgetMap)
                                {
                                    if (w.second->isControlPanel)
                                        continue;

                                    bool vis = IsWindowVisible(w.second->hwnd);

                                    if (!vis)
                                        allVisible = false; // 🔹 at least one hidden

                                    std::wstring js =
                                        L"setCheckbox('" + w.first + L"', " +
                                        (vis ? L"true" : L"false") + L");";

                                    widget->webview->ExecuteScript(js.c_str(), nullptr);
                                }

                                // 🔹 sync "Enable widgets" checkbox
                                std::wstring masterJs =
                                    L"setEnableAll(" + std::wstring(allVisible ? L"true" : L"false") + L");";

                                widget->webview->ExecuteScript(masterJs.c_str(), nullptr);

                                return S_OK;
                            })
                            .Get(),
                        nullptr);

                    widget->webview->add_WebMessageReceived(
                        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                            [](ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT
                            {
                                PWSTR msgRaw = nullptr;
                                args->get_WebMessageAsJson(&msgRaw); // 🔹 FIX

                                std::wstring message = msgRaw;
                                CoTaskMemFree(msgRaw);

                                // remove quotes from json string
                                if (message.size() >= 2 && message.front() == L'"' && message.back() == L'"')
                                {
                                    message = message.substr(1, message.size() - 2);
                                }

                                if (message.rfind(L"set:", 0) == 0)
                                {
                                    size_t first = message.find(L":", 4);
                                    if (first == std::wstring::npos)
                                        return S_OK;

                                    std::wstring name = message.substr(4, first - 4);
                                    int value = std::stoi(message.substr(first + 1));

                                    if (widgetMap.count(name) && widgetMap[name]->hwnd)
                                    {
                                        HWND h = widgetMap[name]->hwnd;

                                        WidgetWindow *w = widgetMap[name];
                                        ShowWindow(h, value ? SW_SHOW : SW_HIDE);

                                        if (w->controller)
                                        {
                                            w->controller->put_IsVisible(value ? TRUE : FALSE);
                                        }
                                        SaveWidgetState();
                                    }
                                }

                                if (message.rfind(L"theme:", 0) == 0)
                                {
                                    currentTheme = message.substr(6); // after "theme:"
                                    SaveWidgetState();
                                    BroadcastTheme();
                                }

                                return S_OK;
                            })
                            .Get(),
                        nullptr);
                }

                return S_OK;
            })
            .Get());

    widgets.push_back(widget);
}

/* =========================================================
   MAIN
   ========================================================= */
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    WNDCLASSEXW wc = {};
wc.cbSize = sizeof(WNDCLASSEXW);
wc.lpfnWndProc = WndProc;
wc.hInstance = hInst;
wc.lpszClassName = L"WidgetWindow";
wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_ICON1));
wc.hIconSm = wc.hIcon;

RegisterClassExW(&wc);


    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hInst](HRESULT, ICoreWebView2Environment *env) -> HRESULT
            {
                std::wstring base = GetExeDir();

                CreateWidget(
                    hInst, env,
                    L"system",
                    base + L"\\Prism\\widgets\\system\\system.html",
                    100, 100, 300, 250,
                    true, false, false);

                CreateWidget(
                    hInst, env,
                    L"weather",
                    base + L"\\Prism\\widgets\\weather\\weather.html",
                    450, 100, 250, 250,
                    false, false, false);

                CreateWidget(
                    hInst, env,
                    L"digital",
                    base + L"\\Prism\\widgets\\digital-clock\\index.html",
                    100, 400, 200, 150,
                    false, false, false);

                CreateWidget(
                    hInst, env,
                    L"analog",
                    base + L"\\Prism\\widgets\\analog-clock\\index.html",
                    350, 400, 600, 600,
                    false, false, false);

                CreateWidget(
                    hInst, env,
                    L"control",
                    base + L"\\Prism\\Engine\\index.html",
                    650, 100, 400, 400,
                    false, true, false);

                // 🔹 TASKBAR WIDGET
                RECT tb = GetRealTaskbarRect();

                CreateWidget(
                    hInst, env,
                    L"taskbar",
                    base + L"\\Prism\\Taskbar\\index.html",
                    tb.left,
                    tb.top,
                    tb.right - tb.left,
                    tb.bottom - tb.top,
                    false,
                    false, true);

                LoadWidgetState();
                BroadcastTheme();

                SetTimer(systemWidgetHwnd, SYSTEM_TIMER, 1000, nullptr);
                return S_OK;
            })
            .Get());

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

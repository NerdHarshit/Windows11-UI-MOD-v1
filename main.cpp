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

std::wstring settingsFile = L"settings.txt";


using namespace Microsoft::WRL;

/* =========================================================
   Widget structure
   ========================================================= */
struct WidgetWindow {
    HWND hwnd;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    std::wstring url;
    bool isSystemWidget;
    bool isControlPanel;
};

std::vector<WidgetWindow*> widgets;
std::unordered_map<std::wstring, WidgetWindow*> widgetMap;

#define SYSTEM_TIMER 1
HWND systemWidgetHwnd = nullptr;


void SaveWidgetPositions()
{
    std::wofstream file(settingsFile, std::ios::app); // append
    if (!file.is_open()) return;

    for (auto& pair : widgetMap)
    {
        WidgetWindow* w = pair.second;
        if (!w || !w->hwnd) continue;

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
    if (!file.is_open()) return;

    for (auto& pair : widgetMap)
    {
        WidgetWindow* w = pair.second;
        bool visible = IsWindowVisible(w->hwnd);
        file << pair.first << L"=" << (visible ? 1 : 0) << std::endl;
    }

    file.close();
    SaveWidgetPositions();
}

void LoadWidgetState()
{
    std::wifstream file(settingsFile);
    if (!file.is_open()) return;

    std::unordered_map<std::wstring, int> values;

    std::wstring line;
    while (std::getline(file, line))
    {
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;

        std::wstring key = line.substr(0, eq);
        int val = std::stoi(line.substr(eq + 1));
        values[key] = val;
    }

    file.close();

    for (auto& pair : widgetMap)
    {
        std::wstring name = pair.first;
        WidgetWindow* w = pair.second;

        if (!w || !w->hwnd) continue;

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

            SetWindowPos(
                w->hwnd,
                nullptr,
                x, y,
                0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
            );
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

    // Drag ONLY widgets
    case WM_LBUTTONDOWN:
        if (widget && !widget->isControlPanel)
        {
            ReleaseCapture();
            SendMessage(h, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
            SaveWidgetState();
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(h, msg, w, l);
}

/* =========================================================
   Create widget / control panel window
   ========================================================= */
void CreateWidget(
    HINSTANCE hInst,
    ICoreWebView2Environment* env,
    const wchar_t* name,          // 🔹 widget name
    const wchar_t* url,
    int x, int y, int w, int h,
    bool isSystemWidget,
    bool isControlPanel)
{
    WidgetWindow* widget = new WidgetWindow();
    widget->url = url;
    widget->isSystemWidget = isSystemWidget;
    widget->isControlPanel = isControlPanel;

    DWORD style = WS_POPUP | WS_VISIBLE;
    DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_LAYERED;

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
        nullptr, nullptr, hInst, nullptr
    );

    if(!isControlPanel)
    {
        SetLayeredWindowAttributes(widget->hwnd,0,255,LWA_ALPHA);
    }

    SetWindowLongPtr(widget->hwnd, GWLP_USERDATA, (LONG_PTR)widget);

    if (isSystemWidget)
        systemWidgetHwnd = widget->hwnd;

    widgetMap[name] = widget;

    env->CreateCoreWebView2Controller(
        widget->hwnd,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [widget, name](HRESULT, ICoreWebView2Controller* ctrl) -> HRESULT
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
        [widget](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT
        {
            bool allVisible = true; // 🔹 track master checkbox

            for (auto& w : widgetMap)
            {
                if (w.second->isControlPanel) continue;

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
        }
    ).Get(),
    nullptr
);




                    widget->webview->add_WebMessageReceived(
                        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                            [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT
                            {
                                PWSTR msgRaw = nullptr;
                                args->get_WebMessageAsJson(&msgRaw);   // 🔹 FIX

                                std::wstring message = msgRaw;
                                CoTaskMemFree(msgRaw);


                                // remove quotes from json string
                                if (message.size() >=2 && message.front()==L'"' && message.back() == L'"')
                                {
                                    message = message.substr(1,message.size()-2);
                                }

                                if(message.rfind(L"set:",0)==0)
                                {
                                    size_t first = message.find(L":",4);
                                    if(first == std::wstring::npos) return S_OK;

                                    std::wstring name = message.substr(4,first-4);
                                    int value = std::stoi(message.substr(first+1));

                                    if(widgetMap.count(name)&& widgetMap[name]->hwnd)
                                    {
                                        HWND h = widgetMap[name] ->hwnd;

                                        
                                        WidgetWindow* w = widgetMap[name];
                                        ShowWindow(h,value? SW_SHOW:SW_HIDE);

                                        if(w->controller)
                                        {
                                            w->controller->put_IsVisible(value ?TRUE:FALSE);
                                        }
                                        SaveWidgetState();
                                    }
                                }

                                return S_OK;
                            }
                        ).Get(),
                        nullptr
                    );
                }

                return S_OK;
            }
        ).Get()
    );

    widgets.push_back(widget);
}

/* =========================================================
   MAIN
   ========================================================= */
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

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
                CreateWidget(
                    hInst, env,
                    L"system",
                    L"C:\\Users\\HARSHIT\\Desktop\\windows11 mod\\Prism\\widgets\\system\\system.html",
                    100, 100, 300, 250,
                    true, false
                );

                CreateWidget(
                    hInst, env,
                    L"weather",
                    L"C:\\Users\\HARSHIT\\Desktop\\windows11 mod\\Prism\\widgets\\weather\\weather.html",
                    450, 100, 250, 250,
                    false, false
                );

                CreateWidget(
                    hInst, env,
                    L"digital",
                    L"C:\\Users\\HARSHIT\\Desktop\\windows11 mod\\Prism\\widgets\\digital-clock\\index.html",
                    100, 400, 200, 150,
                    false, false
                );

                CreateWidget(
                    hInst, env,
                    L"analog",
                    L"C:\\Users\\HARSHIT\\Desktop\\windows11 mod\\Prism\\widgets\\analog-clock\\index.html",
                    350, 400, 600, 600,
                    false, false
                );

                CreateWidget(
                    hInst, env,
                    L"control",
                    L"C:\\Users\\HARSHIT\\Desktop\\windows11 mod\\Prism\\Engine\\index.html",
                    650, 100, 400, 400,
                    false, true
                );

                LoadWidgetState();


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

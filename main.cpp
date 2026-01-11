#include <windows.h>
#include <WebView2.h>
#include <wrl.h>
#include <objbase.h>
#include <string>

using namespace Microsoft::WRL;

HWND hwnd = nullptr;
ComPtr<ICoreWebView2Controller> controller;
ComPtr<ICoreWebView2> webview;

//new
#define SYSTEM_TIMER 1

//new
void SendFakeSystemData()
{
    if (!webview) return;

    static int cpu = 30;
    static int ram = 50;
    static int disk = 70;

    // animate values so you see them move
    cpu = (cpu + 1) % 100;
    ram = (ram + 2) % 100;
    disk = (disk + 1) % 100;

    std::wstring js =
        L"window.chrome.webview.postMessage({"
        L"cpu:" + std::to_wstring(cpu) + L","
        L"ram:" + std::to_wstring(ram) + L","
        L"disk:" + std::to_wstring(disk) +
        L"});";

    webview->ExecuteScript(js.c_str(), nullptr);
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

    //new
    case WM_TIMER:
        if (w == SYSTEM_TIMER)
            SendFakeSystemData();
        return 0;


    case WM_LBUTTONDOWN:
   {
    ReleaseCapture();
    SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    return 0;
   }



    case WM_DESTROY:
        //new
        KillTimer(hwnd, SYSTEM_TIMER);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(h, msg, w, l);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int)
{

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return -1;

    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"WebView2TestWindow";

    RegisterClassW(&wc);

    hwnd = CreateWindowExW(
    WS_EX_TOOLWINDOW ,
    wc.lpszClassName,
    L"",
    WS_POPUP | WS_VISIBLE,
    100, 100, 800, 600,
    NULL, NULL, hInst, NULL
   );
   ShowWindow(hwnd, SW_SHOW);
   UpdateWindow(hwnd);

  // SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA)

   CreateCoreWebView2EnvironmentWithOptions(
    nullptr, nullptr, nullptr,
    Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
        [](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT
        {
            if (!env) return E_FAIL;

            env->CreateCoreWebView2Controller(
                hwnd,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [env](HRESULT hr, ICoreWebView2Controller* ctrl) -> HRESULT
                    {
                        if (!ctrl) return E_FAIL;

                        controller = ctrl;
                        controller->get_CoreWebView2(&webview);

                        RECT bounds;
                        GetClientRect(hwnd, &bounds);
                        controller->put_Bounds(bounds);

                        webview->Navigate(L"C:\\Users\\HARSHIT\\Desktop\\windows11 mod\\Prism\\widgets\\system\\system.html");
                        SetTimer(hwnd, SYSTEM_TIMER, 1000, nullptr);
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

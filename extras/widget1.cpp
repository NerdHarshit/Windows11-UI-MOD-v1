// acrylic_widget.cpp
#undef UNICODE
#undef _UNICODE

#include <windows.h>
#include <gdiplus.h>
#include <stdint.h>
#pragma comment (lib,"Gdiplus.lib")



using namespace Gdiplus;

// --- Accent / composition structures (runtime link to SetWindowCompositionAttribute) ---
enum ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4, // Win10 1809+ (and Win11)
    ACCENT_INVALID_STATE = 5
};

struct ACCENT_POLICY {
    int AccentState;
    int AccentFlags;
    uint32_t GradientColor; // ARGB
    int AnimationId;
};

enum WINDOWCOMPOSITIONATTRIB {
    WCA_UNDEFINED = 0,
    /* lots omitted */
    WCA_ACCENT_POLICY = 19
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB Attrib;
    PVOID pvData;
    SIZE_T cbData;
};

typedef BOOL(WINAPI *pSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

// --- Globals ---
ULONG_PTR g_gdiplusToken = 0;
const int WIDGET_W = 360;
const int WIDGET_H = 160;

// --- Helpers: rounded path + fill ---
void BuildRoundedPath(GraphicsPath &path, const RectF &r, float radius)
{
    float d = radius * 2.0f;
    path.Reset();
    path.AddArc(r.X, r.Y, d, d, 180, 90);
    path.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    path.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    path.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    path.CloseFigure();
}

void FillRoundedRect(Graphics &g, const RectF &r, float radius, const Color &c)
{
    GraphicsPath p;
    BuildRoundedPath(p, r, radius);
    SolidBrush brush(c);
    g.FillPath(&brush, &p);
}

// --- Try to enable acrylic/blur; returns true if set ---
bool TryEnableAcrylic(HWND hwnd)
{
    HMODULE hUser = LoadLibraryA("user32.dll");
    if (!hUser) return false;

    pSetWindowCompositionAttribute SetWindowCompositionAttribute =
        (pSetWindowCompositionAttribute)GetProcAddress(hUser, "SetWindowCompositionAttribute");
    if (!SetWindowCompositionAttribute) {
        FreeLibrary(hUser);
        return false;
    }

    ACCENT_POLICY policy = {};
    // Try acrylic first (modern)
    policy.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND; // 4
    // AccentFlags bitfields: there are different conventions; 0x20 or 0x00 often used.
    // We set 0 (safe) and set GradientColor with alpha.
    // GradientColor ARGB: (alpha << 24) | (B << 16) | (G << 8) | R
    // Use semi-transparent white-ish tint (alpha ~160)
    uint8_t alpha = 160;
    uint8_t r = 255, g = 255, b = 255;
    policy.GradientColor = (uint32_t)((alpha << 24) | (b << 16) | (g << 8) | r);

    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attrib = WCA_ACCENT_POLICY;
    data.pvData = &policy;
    data.cbData = sizeof(policy);

    BOOL ok = SetWindowCompositionAttribute(hwnd, &data);
    FreeLibrary(hUser);

    return ok != FALSE;
}

// fallback: enable blur-behind via DWM (older)
bool TryEnableDwmBlur(HWND hwnd)
{
    // DwmEnableBlurBehindWindow is in dwmapi.dll
    HMODULE hDwm = LoadLibraryA("dwmapi.dll");
    if (!hDwm) return false;

    typedef HRESULT(WINAPI *pDwmEnableBlurBehindWindow)(HWND, const void*);
    pDwmEnableBlurBehindWindow DwmEnableBlurBehindWindow =
        (pDwmEnableBlurBehindWindow)GetProcAddress(hDwm, "DwmEnableBlurBehindWindow");
    if (!DwmEnableBlurBehindWindow) {
        FreeLibrary(hDwm);
        return false;
    }

    // Define the DWM_BLURBEHIND struct locally (avoid missing headers)
    struct DWM_BLURBEHIND {
        DWORD dwFlags;
        BOOL  fEnable;
        HRGN  hRgnBlur;
        BOOL  fTransitionOnMaximized;
    };
    const DWORD DWM_BB_ENABLE = 0x00000001;

    DWM_BLURBEHIND b = {};
    b.dwFlags = DWM_BB_ENABLE;
    b.fEnable = TRUE;
    b.hRgnBlur = NULL;
    b.fTransitionOnMaximized = FALSE;

    HRESULT hr = DwmEnableBlurBehindWindow(hwnd, &b);
    FreeLibrary(hDwm);
    return SUCCEEDED(hr);
}

// --- Window proc and dragging support ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static bool acrylicEnabled = false;
    switch (msg)
    {
    case WM_CREATE:
    {
        // Try acrylic/blurring when the window is created
        acrylicEnabled = TryEnableAcrylic(hwnd);
        if (!acrylicEnabled) {
            // fallback to DWM blur
            TryEnableDwmBlur(hwnd);
        }
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        // Begin move - emulate caption dragging so user can drag anywhere on widget
        ReleaseCapture();
        SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        Graphics g(hdc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

        // Background clear: fully transparent (let acrylic show through)
        // If acrylic works, the composition provides blurred background; still draw a subtle tint.
        g.Clear(Color::MakeARGB(0, 0, 0, 0)); // keep transparent base

        // Card rectangle (we draw a translucent fill to get glassmorphism over blur)
        RectF cardRect(20, 20, (REAL)WIDGET_W - 40, (REAL)WIDGET_H - 40);
        float radius = 14.0f;

        // Slight outer glow / faint border
        {
            // subtle outer stroke to separate from background
            GraphicsPath borderPath;
            BuildRoundedPath(borderPath, cardRect, radius);
            Pen borderPen(Color::MakeARGB(120, 255, 255, 255), 1.0f); // faint white edge
            g.DrawPath(&borderPen, &borderPath);
        }

        // Fill the card with semi-transparent white (this + acrylic makes frosted effect)
        SolidBrush fillBrush(Color::MakeARGB(160, 255, 255, 255));
        GraphicsPath cardPath;
        BuildRoundedPath(cardPath, cardRect, radius);
        g.FillPath(&fillBrush, &cardPath);

        // Optional inner highlight (to feel gloss)
        {
            LinearGradientBrush lgb(
                RectF(cardRect.X, cardRect.Y, cardRect.Width, cardRect.Height / 2),
                Color::MakeARGB(90, 255, 255, 255),
                Color::MakeARGB(0, 255, 255, 255),
                LinearGradientModeVertical
            );
            GraphicsPath topHalf;
            topHalf.AddRectangle(RectF(cardRect.X, cardRect.Y, cardRect.Width, cardRect.Height / 2));
            // intersect with rounded path: use clipping to draw only inside card
            GraphicsState gs = g.Save();
            g.SetClip(&cardPath, CombineModeIntersect);
            g.FillPath(&lgb, &cardPath);
            g.Restore(gs);
        }

        // Text inside
        {
            FontFamily ff(L"Segoe UI");
            Font font(&ff, 18, FontStyleRegular, UnitPixel);
            SolidBrush textBrush(Color::MakeARGB(255, 30, 30, 30));
            PointF pt(cardRect.X + 18, cardRect.Y + 18);
            g.DrawString(L"Hello — Acrylic Widget", -1, &font, pt, &textBrush);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// --- WinMain ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // Initialize GDI+
    GdiplusStartupInput gsi;
    GdiplusStartup(&g_gdiplusToken, &gsi, NULL);

    const char CLASS_NAME[] = "AcrylicWidgetClass";

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL; // we'll paint everything

    RegisterClassA(&wc);

    // Create a popup (borderless) window that is clickable and draggable
    DWORD ex = WS_EX_TOOLWINDOW; // small taskbar footprint; don't use WS_EX_TOPMOST unless desired
    HWND hwnd = CreateWindowExA(
        ex,
        CLASS_NAME,
        "Acrylic Widget",
        WS_POPUP | WS_VISIBLE,
        200, 200, WIDGET_W, WIDGET_H,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        GdiplusShutdown(g_gdiplusToken);
        return 0;
    }

    // Show
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    GdiplusShutdown(g_gdiplusToken);
    return 0;
}

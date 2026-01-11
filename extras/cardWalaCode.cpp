#undef UNICODE
#undef _UNICODE
#include <windows.h>
#include <gdiplus.h>
#pragma comment (lib,"Gdiplus.lib")

ULONG_PTR gdiplusToken;

// ----------------------
// Create rounded rect path
// ----------------------
void BuildRoundedPath(Gdiplus::GraphicsPath &path,
                      const Gdiplus::RectF &r,
                      float radius)
{
    float d = radius * 2;

    path.AddArc(r.X, r.Y, d, d, 180, 90);
    path.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    path.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    path.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    path.CloseFigure();
}

// ----------------------
// Fill rounded rectangle
// ----------------------
void FillRoundedRect(Gdiplus::Graphics &g,
                     const Gdiplus::RectF &rect,
                     float radius,
                     const Gdiplus::Color &color)
{
    Gdiplus::GraphicsPath path;
    BuildRoundedPath(path, rect, radius);
    Gdiplus::SolidBrush brush(color);
    g.FillPath(&brush, &path);
}

// ----------------------
// Window Procedure
// ----------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        // --------- Background ---------
        g.Clear(Gdiplus::Color(255, 10, 100, 175));

        // --------- REAL CARD RECT ---------
        Gdiplus::RectF cardRect(50, 50, 220, 120);

        // --------- CLIPPING PATH FOR CARD ---------
        Gdiplus::GraphicsPath clipPath;
        BuildRoundedPath(clipPath, cardRect, 20);

        // --------- SHADOW BELOW CARD ---------
        {
            // Save graphics state
            Gdiplus::GraphicsState gs = g.Save();

            // EXCLUDE card area so shadow doesn’t bleed inside
            g.SetClip(&clipPath, Gdiplus::CombineModeExclude);

            // Draw shadow
            FillRoundedRect(
                g,
                Gdiplus::RectF(cardRect.X + 8, cardRect.Y + 8,
                                cardRect.Width, cardRect.Height),
                20,
                Gdiplus::Color(50, 0, 0, 0) // stronger 50 alpha
            );

            g.Restore(gs);
        }

        // --------- MAIN CARD ---------
        FillRoundedRect(
            g,
            cardRect,
            20,
            Gdiplus::Color(180, 255, 255, 255)
        );

        // --------- TEXT ---------
        {
            Gdiplus::FontFamily fontFamily(L"Segoe UI");
            Gdiplus::Font font(&fontFamily, 18, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 25, 25, 25));

            g.DrawString(L"My Widget", -1, &font, Gdiplus::PointF(80, 90), &textBrush);
        }

        EndPaint(hwnd, &ps);
    }
    return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ----------------------
// WinMain
// ----------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // Start GDI+
    Gdiplus::GdiplusStartupInput gsi;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gsi, NULL);

    const char CLASS_NAME[] = "ModernWidget";

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = NULL;

    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(
        WS_EX_LAYERED,
        CLASS_NAME,
        "GDI+ Widget",
        WS_POPUP | WS_VISIBLE,
        100, 100, 350, 250,
        NULL, NULL, hInstance, NULL
    );

    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return 0;
}

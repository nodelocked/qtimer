/*
 * qtimer - minimalist desktop pet timer
 * Coffee Time | Pomodoro | Shutdown Timer
 * Pure Win32, zero dependencies.
 */
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")

// ── Layout ────────────────────────────────────────────────────────────────
static const int W = 104, H = 128;

// ── Colors ────────────────────────────────────────────────────────────────
static const COLORREF C_BG    = RGB(20, 20, 30);
static const COLORREF C_DIM   = RGB(80, 80, 110);
static const COLORREF C_TEXT  = RGB(200, 200, 230);
static const COLORREF C_AMBER = RGB(255, 185, 50);
static const COLORREF C_RED   = RGB(255, 80,  80);
static const COLORREF C_GREEN = RGB(70,  210, 130);
// cat palette
static const COLORREF C_FUR   = RGB(185, 145, 95);
static const COLORREF C_DARK  = RGB(110,  80, 45);
static const COLORREF C_PINK  = RGB(230, 150, 140);
static const COLORREF C_EYE   = RGB(80,  200, 120);

// ── IDs ───────────────────────────────────────────────────────────────────
enum { T_ANIM = 1, T_COUNT = 2 };
enum {
    CMD_COFFEE = 100,
    CMD_POMO,
    CMD_SD_15, CMD_SD_30, CMD_SD_60, CMD_SD_120,
    CMD_STOP,
    CMD_EXIT
};
enum Mode { IDLE, COFFEE, POMO, SHUTDOWN };

// ── State ─────────────────────────────────────────────────────────────────
static Mode  s_mode     = IDLE;
static int   s_secs     = 0;
static int   s_pomPhase = 0;   // 0=work 1=break
static int   s_pomCount = 0;
static int   s_frame    = 0;
static HWND  s_hwnd;

// ── GDI helpers ───────────────────────────────────────────────────────────
static void gdi_ellipse(HDC dc, int x, int y, int rx, int ry, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    HPEN   p = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    Ellipse(dc, x-rx, y-ry, x+rx, y+ry);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); DeleteObject(p);
}
static void gdi_tri(HDC dc, POINT* v, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    HPEN   p = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    Polygon(dc, v, 3);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); DeleteObject(p);
}
static void gdi_line(HDC dc, int x0, int y0, int x1, int y1, COLORREF c, int w = 1) {
    HPEN p = CreatePen(PS_SOLID, w, c);
    HGDIOBJ op = SelectObject(dc, p);
    MoveToEx(dc, x0, y0, NULL); LineTo(dc, x1, y1);
    SelectObject(dc, op); DeleteObject(p);
}
static void gdi_rect_border(HDC dc, COLORREF c) {
    HPEN   p  = CreatePen(PS_SOLID, 1, c);
    HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH);
    HGDIOBJ op = SelectObject(dc, p), ob = SelectObject(dc, nb);
    RoundRect(dc, 0, 0, W, H, 10, 10);
    SelectObject(dc, op); SelectObject(dc, ob);
    DeleteObject(p);
}

// ── Procedural cat ────────────────────────────────────────────────────────
// cat centered at (cx, cy), total height ~55px
static void draw_cat(HDC dc, int cx, int cy) {
    // animation
    int  f       = s_frame;
    bool blink   = (f % 90 < 3);
    int  tailOff = ((f / 7) % 4 < 2) ? 5 : -5;
    int  breathe = ((f / 20) % 2);          // body subtle pulse

    // tail (behind body — draw first)
    gdi_line(dc, cx+10, cy+4,  cx+20, cy+15+tailOff, C_FUR, 2);
    gdi_line(dc, cx+20, cy+15+tailOff, cx+16, cy+23+tailOff, C_FUR, 2);
    gdi_ellipse(dc, cx+16, cy+24+tailOff, 3, 2, C_FUR);

    // body
    gdi_ellipse(dc, cx, cy+6, 13+breathe, 11, C_FUR);

    // head
    gdi_ellipse(dc, cx, cy-10, 11, 10, C_FUR);

    // ears (outer)
    POINT earL[] = { {cx-10, cy-17}, {cx-6, cy-28}, {cx-1, cy-17} };
    POINT earR[] = { {cx+1,  cy-17}, {cx+6, cy-28}, {cx+10, cy-17} };
    gdi_tri(dc, earL, C_FUR);
    gdi_tri(dc, earR, C_FUR);
    // ears (inner)
    POINT eiL[] = { {cx-8,cy-18}, {cx-6,cy-24}, {cx-2,cy-18} };
    POINT eiR[] = { {cx+2,cy-18}, {cx+6,cy-24}, {cx+8,cy-18} };
    gdi_tri(dc, eiL, C_PINK);
    gdi_tri(dc, eiR, C_PINK);

    // eyes
    if (!blink) {
        gdi_ellipse(dc, cx-4, cy-11, 2, 2, C_EYE);
        gdi_ellipse(dc, cx+4, cy-11, 2, 2, C_EYE);
        // pupils
        gdi_ellipse(dc, cx-4, cy-11, 1, 1, RGB(0,0,0));
        gdi_ellipse(dc, cx+4, cy-11, 1, 1, RGB(0,0,0));
    } else {
        gdi_line(dc, cx-6, cy-11, cx-2, cy-11, C_DARK);
        gdi_line(dc, cx+2, cy-11, cx+6, cy-11, C_DARK);
    }

    // nose
    gdi_ellipse(dc, cx, cy-6, 2, 1, C_PINK);

    // mouth
    gdi_line(dc, cx,   cy-5, cx-2, cy-3, C_DARK);
    gdi_line(dc, cx,   cy-5, cx+2, cy-3, C_DARK);

    // whiskers
    gdi_line(dc, cx-2, cy-6, cx-11, cy-5, C_DARK);
    gdi_line(dc, cx-2, cy-6, cx-11, cy-7, C_DARK);
    gdi_line(dc, cx+2, cy-6, cx+11, cy-5, C_DARK);
    gdi_line(dc, cx+2, cy-6, cx+11, cy-7, C_DARK);

    // paws
    gdi_ellipse(dc, cx-6, cy+16, 5, 3, C_FUR);
    gdi_ellipse(dc, cx+6, cy+16, 5, 3, C_FUR);

    // coffee mode: little "z z" above head
    if (s_mode == COFFEE) {
        int zx = cx + 13, zy = cy - 22;
        int zf = (f / 25) % 3;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, C_AMBER);
        HFONT zf_ = CreateFontA(7,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,0,DEFAULT_PITCH,"Segoe UI");
        HGDIOBJ oz = SelectObject(dc, zf_);
        if (zf >= 1) TextOutA(dc, zx,   zy+4, "z", 1);
        if (zf >= 2) TextOutA(dc, zx+5, zy,   "z", 1);
        SelectObject(dc, oz); DeleteObject(zf_);
    }
}

// ── Utilities ─────────────────────────────────────────────────────────────
static void fmt_time(char* buf, int s) {
    int h = s/3600, m = (s%3600)/60, sec = s%60;
    if (h) sprintf(buf, "%d:%02d:%02d", h, m, sec);
    else   sprintf(buf, "%02d:%02d", m, sec);
}
static void keep_awake(bool on) {
    SetThreadExecutionState(on
        ? ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED
        : ES_CONTINUOUS);
}
static void start_count(int secs) {
    s_secs = secs;
    SetTimer(s_hwnd, T_COUNT, 1000, NULL);
}
static void stop_all() {
    KillTimer(s_hwnd, T_COUNT);
    keep_awake(false);
    s_mode = IDLE; s_secs = 0; s_pomPhase = 0; s_pomCount = 0;
}
static void do_shutdown() {
    ShellExecuteA(NULL, "open", "shutdown.exe", "/s /t 0", NULL, SW_HIDE);
}

// ── Window Procedure ──────────────────────────────────────────────────────
static LRESULT CALLBACK WndProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_CREATE:
        SetTimer(hw, T_ANIM, 80, NULL);
        return 0;

    case WM_TIMER:
        if (wp == T_ANIM) {
            s_frame++;
            InvalidateRect(hw, NULL, FALSE);
        } else if (wp == T_COUNT && s_secs > 0) {
            s_secs--;
            if (s_secs == 0) {
                if (s_mode == POMO) {
                    MessageBeep(MB_OK);
                    FlashWindow(hw, FALSE);
                    if (s_pomPhase == 0) { s_pomCount++; s_pomPhase = 1; s_secs = 5*60; }
                    else                { s_pomPhase = 0; s_secs = 25*60; }
                } else if (s_mode == SHUTDOWN) {
                    do_shutdown();
                    stop_all();
                }
            }
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc  = BeginPaint(hw, &ps);
        // double buffer
        HDC mdc = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, W, H);
        HGDIOBJ obmp = SelectObject(mdc, bmp);

        // fill background
        HBRUSH bg = CreateSolidBrush(C_BG);
        RECT rc = {0,0,W,H};
        FillRect(mdc, &rc, bg);
        DeleteObject(bg);

        // cat
        draw_cat(mdc, W/2, H/2 + 8);

        // top label
        SetBkMode(mdc, TRANSPARENT);
        HFONT font = CreateFontA(11,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
                                 0,0,DEFAULT_QUALITY,DEFAULT_PITCH,"Segoe UI");
        HGDIOBJ ofont = SelectObject(mdc, font);

        char txt[40] = "";
        COLORREF tc = C_DIM;
        switch(s_mode) {
        case IDLE:     strcpy(txt, "qtimer"); break;
        case COFFEE:   strcpy(txt, "coffee~"); tc = C_AMBER; break;
        case POMO: {
            char t[16]; fmt_time(t, s_secs);
            sprintf(txt, "%s  %s", s_pomPhase ? "break" : "work", t);
            tc = s_pomPhase ? C_GREEN : C_RED;
            break;
        }
        case SHUTDOWN: {
            char t[16]; fmt_time(t, s_secs);
            sprintf(txt, "off  %s", t);
            tc = C_RED;
            break;
        }
        }
        SetTextColor(mdc, tc);
        RECT tr = {0, 5, W, 20};
        DrawTextA(mdc, txt, -1, &tr, DT_CENTER | DT_SINGLELINE);

        // pomodoro session dots
        if (s_mode == POMO && s_pomCount > 0) {
            int count = s_pomCount < 8 ? s_pomCount : 8;
            int startX = (W - count * 8) / 2 + 3;
            for (int i = 0; i < count; i++)
                gdi_ellipse(mdc, startX + i*8, H-7, 2, 2,
                            s_pomPhase ? C_GREEN : C_RED);
        }

        SelectObject(mdc, ofont);
        DeleteObject(font);

        // border glow
        COLORREF border = C_BG;
        switch(s_mode) {
        case COFFEE:   border = C_AMBER; break;
        case POMO:     border = s_pomPhase ? C_GREEN : C_RED; break;
        case SHUTDOWN: border = C_RED; break;
        default: break;
        }
        if (border != C_BG) gdi_rect_border(mdc, border);

        BitBlt(dc, 0, 0, W, H, mdc, 0, 0, SRCCOPY);
        SelectObject(mdc, obmp);
        DeleteObject(bmp);
        DeleteDC(mdc);
        EndPaint(hw, &ps);
        return 0;
    }

    case WM_RBUTTONUP: {
        POINT pt; GetCursorPos(&pt);
        HMENU m = CreatePopupMenu();
        if (s_mode == IDLE) {
            AppendMenuA(m, MF_STRING, CMD_COFFEE, "Coffee Time  (keep awake)");
            AppendMenuA(m, MF_STRING, CMD_POMO,   "Pomodoro  25 + 5");
            HMENU sub = CreatePopupMenu();
            AppendMenuA(sub, MF_STRING, CMD_SD_15,  "15 min");
            AppendMenuA(sub, MF_STRING, CMD_SD_30,  "30 min");
            AppendMenuA(sub, MF_STRING, CMD_SD_60,  "1 hour");
            AppendMenuA(sub, MF_STRING, CMD_SD_120, "2 hours");
            AppendMenuA(m, MF_POPUP, (UINT_PTR)sub, "Shutdown after...");
        } else {
            AppendMenuA(m, MF_STRING, CMD_STOP, "Stop");
        }
        AppendMenuA(m, MF_SEPARATOR, 0, NULL);
        AppendMenuA(m, MF_STRING, CMD_EXIT, "Exit");
        TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hw, NULL);
        DestroyMenu(m);
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case CMD_COFFEE:   s_mode = COFFEE; keep_awake(true); break;
        case CMD_POMO:     s_mode = POMO; s_pomPhase=0; s_pomCount=0; start_count(25*60); break;
        case CMD_SD_15:    s_mode = SHUTDOWN; start_count(15*60);  break;
        case CMD_SD_30:    s_mode = SHUTDOWN; start_count(30*60);  break;
        case CMD_SD_60:    s_mode = SHUTDOWN; start_count(60*60);  break;
        case CMD_SD_120:   s_mode = SHUTDOWN; start_count(120*60); break;
        case CMD_STOP:     stop_all(); break;
        case CMD_EXIT:     DestroyWindow(hw); break;
        }
        InvalidateRect(hw, NULL, FALSE);
        return 0;

    // drag anywhere
    case WM_LBUTTONDOWN:
        ReleaseCapture();
        SendMessage(hw, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;

    case WM_DESTROY:
        stop_all();
        KillTimer(hw, T_ANIM);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hw, msg, wp, lp);
}

// ── Entry ─────────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hi, HINSTANCE, LPSTR, int) {
    // single instance
    HANDLE mtx = CreateMutexA(NULL, TRUE, "qtimer_single");
    if (GetLastError() == ERROR_ALREADY_EXISTS) { CloseHandle(mtx); return 0; }

    WNDCLASSEXA wc = { sizeof(wc) };
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hi;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "QTimer";
    RegisterClassExA(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    s_hwnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        "QTimer", "qtimer",
        WS_POPUP,
        sw - W - 24, sh - H - 56,
        W, H,
        NULL, NULL, hi, NULL
    );

    // slight transparency
    SetLayeredWindowAttributes(s_hwnd, 0, 235, LWA_ALPHA);

    ShowWindow(s_hwnd, SW_SHOW);
    UpdateWindow(s_hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CloseHandle(mtx);
    return (int)msg.wParam;
}

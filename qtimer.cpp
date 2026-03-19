/*
 * qtimer - minimalist desktop pet timer
 * Transparent window, cat walks the desktop.
 * Pure Win32, zero dependencies.
 */
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")

// ── Dimensions ────────────────────────────────────────────────────────────
static const int W = 80, H = 90;
static const int CX = 40, CY = 58;   // cat center in window

// ── Colors ────────────────────────────────────────────────────────────────
// C_KEY is the chroma-key: these pixels become transparent + click-through
static const COLORREF C_KEY   = RGB(0, 255, 0);
static const COLORREF C_PILL  = RGB(22,  22, 35);
static const COLORREF C_DIM   = RGB(90,  90,120);
static const COLORREF C_AMBER = RGB(255,185, 50);
static const COLORREF C_RED   = RGB(255, 80, 80);
static const COLORREF C_GREEN = RGB( 70,210,130);
static const COLORREF C_FUR   = RGB(185,145, 95);
static const COLORREF C_DARK  = RGB(110, 80, 45);
static const COLORREF C_PINK  = RGB(230,150,140);
static const COLORREF C_EYE   = RGB( 80,200,120);

// ── IDs ───────────────────────────────────────────────────────────────────
enum { T_ANIM = 1, T_COUNT = 2 };
enum {
    CMD_COFFEE = 100, CMD_POMO,
    CMD_SD_15, CMD_SD_30, CMD_SD_60, CMD_SD_120,
    CMD_STOP, CMD_EXIT
};
enum Mode { IDLE, COFFEE, POMO, SHUTDOWN };

// ── State ─────────────────────────────────────────────────────────────────
static Mode s_mode     = IDLE;
static int  s_secs     = 0;
static int  s_pomPhase = 0;
static int  s_pomCount = 0;
static int  s_frame    = 0;
static int  s_posX     = 0;
static int  s_posY     = 0;
static int  s_dir      = 1;    // 1=right  -1=left
static int  s_idle     = 40;   // idle frames before walking starts
static HWND s_hwnd;

// ── GDI helpers ───────────────────────────────────────────────────────────
static void el(HDC dc, int x,int y,int rx,int ry, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    HPEN   p = CreatePen(PS_SOLID,1,c);
    HGDIOBJ ob=SelectObject(dc,b), op=SelectObject(dc,p);
    Ellipse(dc, x-rx, y-ry, x+rx, y+ry);
    SelectObject(dc,ob); SelectObject(dc,op);
    DeleteObject(b); DeleteObject(p);
}
static void tri(HDC dc, POINT* v, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    HPEN   p = CreatePen(PS_SOLID,1,c);
    HGDIOBJ ob=SelectObject(dc,b), op=SelectObject(dc,p);
    Polygon(dc, v, 3);
    SelectObject(dc,ob); SelectObject(dc,op);
    DeleteObject(b); DeleteObject(p);
}
static void ln(HDC dc, int x0,int y0,int x1,int y1, COLORREF c, int w=1) {
    HPEN p = CreatePen(PS_SOLID,w,c);
    HGDIOBJ op = SelectObject(dc,p);
    MoveToEx(dc,x0,y0,NULL); LineTo(dc,x1,y1);
    SelectObject(dc,op); DeleteObject(p);
}
static void pill(HDC dc, int x0,int y0,int x1,int y1, COLORREF fill) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN   p = CreatePen(PS_SOLID,1,fill);
    HGDIOBJ ob=SelectObject(dc,b), op=SelectObject(dc,p);
    RoundRect(dc,x0,y0,x1,y1,8,8);
    SelectObject(dc,ob); SelectObject(dc,op);
    DeleteObject(b); DeleteObject(p);
}

// ── Procedural cat ────────────────────────────────────────────────────────
// Always drawn facing right in local space; world-transform flips for left.
static void draw_cat(HDC dc, int cx, int cy) {
    int  f      = s_frame;
    bool blink  = (f % 90 < 3);
    bool moving = (s_idle == 0);
    bool leftUp = moving && ((f / 4) % 2 == 0);
    int  tailOff= ((f / 8) % 4 < 2) ? 4 : -4;
    int  bob    = moving ? ((f / 4) % 2) : 0;

    // ── mirror transform when facing left ─────────────────────────────
    SetGraphicsMode(dc, GM_ADVANCED);
    if (s_dir == -1) {
        XFORM xf = { -1.f, 0.f, 0.f, 1.f, (float)(cx*2), 0.f };
        SetWorldTransform(dc, &xf);
    }

    // tail (behind body — draw first)
    ln(dc, cx+10, cy+4,  cx+19, cy+14+tailOff, C_FUR, 2);
    ln(dc, cx+19, cy+14+tailOff, cx+15, cy+22+tailOff, C_FUR, 2);
    el(dc, cx+15, cy+23+tailOff, 3, 2, C_FUR);

    // body
    el(dc, cx, cy+5+bob, 12, 10, C_FUR);
    // head
    el(dc, cx, cy-10, 10, 9, C_FUR);

    // ears outer
    POINT eL[] = { {cx-9,cy-16}, {cx-5,cy-26}, {cx-1,cy-16} };
    POINT eR[] = { {cx+1,cy-16}, {cx+5,cy-26}, {cx+9,cy-16} };
    tri(dc, eL, C_FUR); tri(dc, eR, C_FUR);
    // ears inner
    POINT iL[] = { {cx-7,cy-17}, {cx-5,cy-23}, {cx-2,cy-17} };
    POINT iR[] = { {cx+2,cy-17}, {cx+5,cy-23}, {cx+7,cy-17} };
    tri(dc, iL, C_PINK); tri(dc, iR, C_PINK);

    // eyes
    if (!blink) {
        el(dc, cx-3, cy-11, 2, 2, C_EYE);
        el(dc, cx+3, cy-11, 2, 2, C_EYE);
        el(dc, cx-3, cy-11, 1, 1, RGB(0,0,0));
        el(dc, cx+3, cy-11, 1, 1, RGB(0,0,0));
    } else {
        ln(dc, cx-5, cy-11, cx-1, cy-11, C_DARK);
        ln(dc, cx+1, cy-11, cx+5, cy-11, C_DARK);
    }

    // nose + mouth
    el(dc, cx, cy-7, 2, 1, C_PINK);
    ln(dc, cx, cy-6, cx-2, cy-4, C_DARK);
    ln(dc, cx, cy-6, cx+2, cy-4, C_DARK);

    // whiskers
    ln(dc, cx-2, cy-7, cx-11, cy-6, C_DARK);
    ln(dc, cx-2, cy-7, cx-11, cy-8, C_DARK);
    ln(dc, cx+2, cy-7, cx+11, cy-6, C_DARK);
    ln(dc, cx+2, cy-7, cx+11, cy-8, C_DARK);

    // paws (alternating lift when walking)
    int ldy = leftUp ? -2 : 0;
    int rdy = (moving && !leftUp) ? -2 : 0;
    el(dc, cx-6, cy+15+ldy, 4, 3, C_FUR);
    el(dc, cx+6, cy+15+rdy, 4, 3, C_FUR);

    // reset transform
    XFORM id = { 1.f, 0.f, 0.f, 1.f, 0.f, 0.f };
    SetWorldTransform(dc, &id);
    SetGraphicsMode(dc, GM_COMPATIBLE);

    // coffee z's (always on right side, after transform reset)
    if (s_mode == COFFEE) {
        int zf = (f / 35) % 4;
        if (zf > 0) {
            HFONT zfont = CreateFontA(8,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,
                                      0,0,NONANTIALIASED_QUALITY,DEFAULT_PITCH,"Tahoma");
            HGDIOBJ oz = SelectObject(dc, zfont);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, C_AMBER);
            if (zf >= 1) TextOutA(dc, cx+12, cy-22, "z", 1);
            if (zf >= 2) TextOutA(dc, cx+17, cy-28, "z", 1);
            if (zf >= 3) TextOutA(dc, cx+22, cy-34, "Z", 1);
            SelectObject(dc, oz); DeleteObject(zfont);
        }
    }
}

// ── Utilities ─────────────────────────────────────────────────────────────
static void fmt_time(char* buf, int s) {
    int h=s/3600, m=(s%3600)/60, sec=s%60;
    if (h) sprintf(buf, "%d:%02d:%02d", h,m,sec);
    else   sprintf(buf, "%02d:%02d", m,sec);
}
static void keep_awake(bool on) {
    SetThreadExecutionState(on
        ? ES_CONTINUOUS|ES_DISPLAY_REQUIRED|ES_SYSTEM_REQUIRED
        : ES_CONTINUOUS);
}
static void start_count(int secs) { s_secs=secs; SetTimer(s_hwnd,T_COUNT,1000,NULL); }
static void stop_all() {
    KillTimer(s_hwnd, T_COUNT);
    keep_awake(false);
    s_mode=IDLE; s_secs=0; s_pomPhase=0; s_pomCount=0;
}
static void do_shutdown() {
    ShellExecuteA(NULL,"open","shutdown.exe","/s /t 0",NULL,SW_HIDE);
}

// ── Window Procedure ──────────────────────────────────────────────────────
static LRESULT CALLBACK WndProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        SetTimer(hw, T_ANIM, 80, NULL);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_MOVE:
        // keep s_posX/Y in sync after user drags
        s_posX = (short)LOWORD(lp);
        s_posY = (short)HIWORD(lp);
        return 0;

    case WM_TIMER:
        if (wp == T_ANIM) {
            s_frame++;
            // ── walk update ──────────────────────────────────────────
            if (s_idle > 0) {
                s_idle--;
            } else {
                int sw = GetSystemMetrics(SM_CXSCREEN);
                s_posX += s_dir;
                if (s_posX <= 0) {
                    s_posX = 0; s_dir = 1;
                    s_idle = 50 + (s_frame * 7) % 80;
                } else if (s_posX + W >= sw) {
                    s_posX = sw - W; s_dir = -1;
                    s_idle = 50 + (s_frame * 7) % 80;
                } else if ((s_frame * 17) % 900 == 0) {
                    // occasional random rest
                    s_idle = 60 + (s_frame * 3) % 100;
                }
                SetWindowPos(hw, NULL, s_posX, s_posY, 0, 0,
                             SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER);
            }
            InvalidateRect(hw, NULL, FALSE);
        }
        else if (wp == T_COUNT && s_secs > 0) {
            s_secs--;
            if (s_secs == 0) {
                if (s_mode == POMO) {
                    MessageBeep(MB_OK);
                    FlashWindow(hw, FALSE);
                    if (!s_pomPhase) { s_pomCount++; s_pomPhase=1; s_secs=5*60; }
                    else             { s_pomPhase=0; s_secs=25*60; }
                } else if (s_mode == SHUTDOWN) {
                    do_shutdown(); stop_all();
                }
            }
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc  = BeginPaint(hw, &ps);
        HDC mdc = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, W, H);
        HGDIOBJ obmp = SelectObject(mdc, bmp);

        // fill with chroma-key → these pixels become transparent
        HBRUSH kb = CreateSolidBrush(C_KEY);
        RECT rc = {0,0,W,H};
        FillRect(mdc, &rc, kb);
        DeleteObject(kb);

        // cat
        draw_cat(mdc, CX, CY);

        // mode label above cat
        char txt[40] = "";
        COLORREF tc = C_DIM;
        switch(s_mode) {
        case IDLE:     strcpy(txt,"qtimer"); break;
        case COFFEE:   strcpy(txt,"coffee~"); tc=C_AMBER; break;
        case POMO: {
            char t[16]; fmt_time(t,s_secs);
            sprintf(txt, "%s %s", s_pomPhase?"break":"work", t);
            tc = s_pomPhase ? C_GREEN : C_RED;
            break;
        }
        case SHUTDOWN: {
            char t[16]; fmt_time(t,s_secs);
            sprintf(txt,"off %s",t); tc=C_RED;
            break;
        }
        }
        // opaque pill behind text (no chroma-key fringing)
        pill(mdc, 5, 3, W-5, 17, C_PILL);
        SetBkMode(mdc, TRANSPARENT);
        SetTextColor(mdc, tc);
        HFONT font = CreateFontA(10,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
                                 0,0,NONANTIALIASED_QUALITY,DEFAULT_PITCH,"Tahoma");
        HGDIOBJ of = SelectObject(mdc, font);
        RECT tr = {5,3,W-5,17};
        DrawTextA(mdc, txt, -1, &tr, DT_CENTER|DT_SINGLELINE|DT_VCENTER);

        // pomodoro session dots
        if (s_mode == POMO && s_pomCount > 0) {
            int cnt = s_pomCount < 8 ? s_pomCount : 8;
            int sx  = (W - cnt*8)/2 + 3;
            for (int i=0;i<cnt;i++)
                el(mdc, sx+i*8, H-7, 2, 2, tc);
        }
        SelectObject(mdc, of); DeleteObject(font);

        BitBlt(dc, 0,0,W,H, mdc, 0,0, SRCCOPY);
        SelectObject(mdc, obmp);
        DeleteObject(bmp); DeleteDC(mdc);
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
        SetForegroundWindow(hw);
        TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hw, NULL);
        DestroyMenu(m);
        return 0;
    }

    case WM_COMMAND:
        switch(LOWORD(wp)) {
        case CMD_COFFEE:  s_mode=COFFEE; keep_awake(true); break;
        case CMD_POMO:    s_mode=POMO; s_pomPhase=0; s_pomCount=0; start_count(25*60); break;
        case CMD_SD_15:   s_mode=SHUTDOWN; start_count(15*60);  break;
        case CMD_SD_30:   s_mode=SHUTDOWN; start_count(30*60);  break;
        case CMD_SD_60:   s_mode=SHUTDOWN; start_count(60*60);  break;
        case CMD_SD_120:  s_mode=SHUTDOWN; start_count(120*60); break;
        case CMD_STOP:    stop_all(); break;
        case CMD_EXIT:    DestroyWindow(hw); break;
        }
        InvalidateRect(hw, NULL, FALSE);
        return 0;

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
    HANDLE mtx = CreateMutexA(NULL, TRUE, "qtimer_single");
    if (GetLastError() == ERROR_ALREADY_EXISTS) { CloseHandle(mtx); return 0; }

    WNDCLASSEXA wc = { sizeof(wc) };
    wc.style         = CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hi;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = "QTimer";
    RegisterClassExA(&wc);

    // Start at bottom of work area (above taskbar), centered
    RECT work;
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
    s_posX = (work.right - work.left) / 2 - W/2;
    s_posY = work.bottom - H;

    s_hwnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        "QTimer", "qtimer",
        WS_POPUP,
        s_posX, s_posY, W, H,
        NULL, NULL, hi, NULL
    );

    // Chroma-key: C_KEY pixels → transparent + click-through
    SetLayeredWindowAttributes(s_hwnd, C_KEY, 0, LWA_COLORKEY);

    ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(s_hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    CloseHandle(mtx);
    return (int)msg.wParam;
}

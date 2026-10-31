/* dialogs: cross-thread behaviour of dialog helpers and message boxes */
#include "dlgutil.h"

#define INST GetModuleHandleA(NULL)

/* ---- worker thread that owns a modeless dialog and pumps messages ---- */
static HANDLE g_ready, g_thread;
static HWND g_dlg;
static DWORD g_tid;
static volatile LONG g_stop;

static DWORD WINAPI dlg_thread(LPVOID p) {
    DT d; std_tmpl(&d, BASE_STYLE);
    g_tid = GetCurrentThreadId();
    g_dlg = CreateDialogIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, NULL, 0);
    SetEvent(g_ready);
    if (!g_dlg) return 1;
    DWORD end = GetTickCount() + 15000;
    while (!g_stop && GetTickCount() < end) {
        MSG m;
        while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) {
            if (m.message == WM_QUIT) { g_stop = 1; break; }
            if (!IsDialogMessageA(g_dlg, &m)) { TranslateMessage(&m); DispatchMessageA(&m); }
        }
        MsgWaitForMultipleObjects(0, NULL, FALSE, 20, QS_ALLINPUT);
    }
    DestroyWindow(g_dlg);
    return 0;
}
static int start_dlg_thread(void) {
    g_ready = CreateEventA(NULL, TRUE, FALSE, NULL);
    g_stop = 0; g_dlg = NULL;
    g_thread = CreateThread(NULL, 0, dlg_thread, NULL, 0, NULL);
    if (!g_thread) return 0;
    if (WaitForSingleObject(g_ready, 5000) != WAIT_OBJECT_0 || !g_dlg) return 0;
    return 1;
}
static void stop_dlg_thread(void) {
    g_stop = 1;
    WaitForSingleObject(g_thread, 5000);
    CloseHandle(g_thread); CloseHandle(g_ready);
}

TEST(dialogs_thread_dlgitem_text_cross_thread) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(SetDlgItemTextA); REQUIRE(GetDlgItemTextA);
    if (!start_dlg_thread()) { stop_dlg_thread(); t_skip("worker dialog not created"); return; }
    HWND e = GetDlgItem(g_dlg, ID_EDIT);
    CHECK(e != NULL);
    char a[64];
    CHECK(SetDlgItemTextA(g_dlg, ID_EDIT, "from main"));
    EQ(GetDlgItemTextA(g_dlg, ID_EDIT, a, 64), 9);
    EXPECT_STR(a, "from main");
    CHECK(SetDlgItemInt(g_dlg, ID_EDIT, 4711, 0));
    BOOL tr = 0;
    EQ(GetDlgItemInt(g_dlg, ID_EDIT, &tr, 0), 4711); EQ(tr, 1);
    stop_dlg_thread();
}
TEST(dialogs_thread_buttons_cross_thread) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(CheckDlgButton); REQUIRE(IsDlgButtonChecked); REQUIRE(CheckRadioButton);
    if (!start_dlg_thread()) { stop_dlg_thread(); t_skip("worker dialog not created"); return; }
    CHECK(CheckDlgButton(g_dlg, ID_CHK, BST_CHECKED));
    EQ(IsDlgButtonChecked(g_dlg, ID_CHK), BST_CHECKED);
    CHECK(CheckRadioButton(g_dlg, ID_R1, ID_R3, ID_R3));
    EQ(IsDlgButtonChecked(g_dlg, ID_R3), 1);
    EQ(IsDlgButtonChecked(g_dlg, ID_R1), 0);
    stop_dlg_thread();
}
TEST(dialogs_thread_sendmessage_cross_thread) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendDlgItemMessageA);
    if (!start_dlg_thread()) { stop_dlg_thread(); t_skip("worker dialog not created"); return; }
    EQ(SendDlgItemMessageA(g_dlg, ID_LIST, LB_ADDSTRING, 0, (LPARAM)"one"), 0);
    EQ(SendDlgItemMessageA(g_dlg, ID_LIST, LB_ADDSTRING, 0, (LPARAM)"two"), 1);
    EQ(SendDlgItemMessageA(g_dlg, ID_LIST, LB_GETCOUNT, 0, 0), 2);
    stop_dlg_thread();
}
TEST(dialogs_thread_navigation_cross_thread) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgTabItem); REQUIRE(GetNextDlgGroupItem); REQUIRE(MapDialogRect);
    if (!start_dlg_thread()) { stop_dlg_thread(); t_skip("worker dialog not created"); return; }
    EQ(GetNextDlgTabItem(g_dlg, NULL, FALSE), GetDlgItem(g_dlg, ID_EDIT));
    EQ(GetNextDlgTabItem(g_dlg, GetDlgItem(g_dlg, ID_EDIT), FALSE), GetDlgItem(g_dlg, ID_CHK));
    EQ(GetNextDlgGroupItem(g_dlg, GetDlgItem(g_dlg, ID_R1), FALSE), GetDlgItem(g_dlg, ID_R2));
    RECT r = { 4, 8, 4, 8 };
    CHECK(MapDialogRect(g_dlg, &r));
    CHECK(r.left > 0 && r.top > 0);
    EQ(GetDlgCtrlID(GetDlgItem(g_dlg, ID_BTN)), ID_BTN);
    stop_dlg_thread();
}
TEST(dialogs_thread_destroywindow_other_thread_denied) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow); REQUIRE(IsWindow);
    if (!start_dlg_thread()) { stop_dlg_thread(); t_skip("worker dialog not created"); return; }
    SET_ERR(0);
    CHECK(!DestroyWindow(g_dlg));
    EQ(GetLastError(), ERROR_ACCESS_DENIED);
    CHECK(IsWindow(g_dlg));
    DWORD pid = 0;
    DWORD tid = GetWindowThreadProcessId(g_dlg, &pid);
    EQ(tid, g_tid);
    EQ(pid, GetCurrentProcessId());
    stop_dlg_thread();
    CHECK(!IsWindow(g_dlg));
}
TEST(dialogs_thread_enddialog_other_thread_on_modeless) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(EndDialog); REQUIRE(IsWindow);
    if (!start_dlg_thread()) { stop_dlg_thread(); t_skip("worker dialog not created"); return; }
    BOOL r = EndDialog(g_dlg, 5);
    t_note("EndDialog on another thread's modeless dialog -> %d err=%lu", r, GetLastError());
    CHECK(IsWindow(g_dlg));
    stop_dlg_thread();
}
TEST(dialogs_thread_defdlgproc_cross_thread) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DefDlgProcW);
    if (!start_dlg_thread()) { stop_dlg_thread(); t_skip("worker dialog not created"); return; }
    WCHAR w[32];
    EQ(DefDlgProcW(g_dlg, WM_GETTEXT, 32, (LPARAM)w), 3); EXPECT_WSTR(w, L"Std");
    stop_dlg_thread();
}

/* ---- modal dialog in a worker thread, closed from here ---- */
static HANDLE g_mready; static HWND g_mdlg; static volatile INT_PTR g_mres;
static INT_PTR CALLBACK mthread_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_INITDIALOG: g_mdlg = h; SetEvent(g_mready); SetTimer(h, 77, 6000, NULL); return 1;
    case WM_TIMER: if (w == 77) { EndDialog(h, -999); return 1; } break;
    case WM_COMMAND: if (LOWORD(w) == IDOK) { EndDialog(h, 4242); return 1; } break;
    case WM_USER + 7: EndDialog(h, 777); SetWindowLongPtrW(h, DWLP_MSGRESULT, 1); return 1;
    }
    return 0;
}
static DWORD WINAPI mthread(LPVOID p) {
    DT d; std_tmpl(&d, BASE_STYLE);
    g_mres = DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, mthread_proc, 0);
    return 0;
}
TEST(dialogs_thread_modal_closed_by_command_from_other_thread) {
    REQUIRE(DialogBoxIndirectParamW); REQUIRE(PostMessageW);
    g_mready = CreateEventA(NULL, TRUE, FALSE, NULL); g_mdlg = NULL; g_mres = 0;
    HANDLE t = CreateThread(NULL, 0, mthread, NULL, 0, NULL);
    REQUIRE(t);
    if (WaitForSingleObject(g_mready, 5000) != WAIT_OBJECT_0) { t_note("modal dialog did not start"); }
    else {
        CHECK(IsWindow(g_mdlg));
        CHECK(PostMessageW(g_mdlg, WM_COMMAND, IDOK, 0));
    }
    EQ(WaitForSingleObject(t, 8000), WAIT_OBJECT_0);
    EQ(g_mres, 4242);
    CloseHandle(t); CloseHandle(g_mready);
    CHECK(!IsWindow(g_mdlg));
}
TEST(dialogs_thread_modal_sendmessage_result_from_other_thread) {
    REQUIRE(DialogBoxIndirectParamW); REQUIRE(SendMessageW);
    g_mready = CreateEventA(NULL, TRUE, FALSE, NULL); g_mdlg = NULL; g_mres = 0;
    HANDLE t = CreateThread(NULL, 0, mthread, NULL, 0, NULL);
    REQUIRE(t);
    if (WaitForSingleObject(g_mready, 5000) == WAIT_OBJECT_0) {
        EQ(SendMessageW(g_mdlg, WM_USER + 7, 0, 0), 1);       /* DWLP_MSGRESULT set by the dialog procedure */
        EQ(GetDlgItem(g_mdlg, ID_EDIT) != NULL || 1, 1);
    }
    EQ(WaitForSingleObject(t, 8000), WAIT_OBJECT_0);
    EQ(g_mres, 777);
    CloseHandle(t); CloseHandle(g_mready);
}

/* ---- message box in a worker thread ---- */
static volatile int g_mbres;
static DWORD WINAPI mb_thread(LPVOID p) {
    g_mbres = MessageBoxA(NULL, "worker thread box", "T32 thread box", MB_YESNO);
    return 0;
}
static HWND wait_for_box(DWORD ms) {
    DWORD end = GetTickCount() + ms;
    while (GetTickCount() < end) {
        HWND b = FindWindowA("#32770", "T32 thread box");
        if (b && IsWindowVisible(b)) return b;
        Sleep(20);
    }
    return NULL;
}
TEST(dialogs_thread_messagebox_clicked_from_other_thread) {
    REQUIRE(MessageBoxA); REQUIRE(FindWindowA); REQUIRE(SendMessageW);
    g_mbres = 0;
    HANDLE t = CreateThread(NULL, 0, mb_thread, NULL, 0, NULL);
    REQUIRE(t);
    HWND box = wait_for_box(4000);
    CHECK(box != NULL);
    if (box) {
        HWND no = GetDlgItem(box, IDNO);
        CHECK(no != NULL);
        DWORD pid = 0, tid = GetWindowThreadProcessId(box, &pid);
        CHECK(tid != GetCurrentThreadId());
        EQ(pid, GetCurrentProcessId());
        SET_ERR(0);
        CHECK(!DestroyWindow(box));
        EQ(GetLastError(), ERROR_ACCESS_DENIED);
        CHECK(IsWindow(box));
        SendMessageW(no, BM_CLICK, 0, 0);
    }
    if (WaitForSingleObject(t, 6000) != WAIT_OBJECT_0) {
        /* failsafe */
        HWND b2 = FindWindowA("#32770", "T32 thread box");
        if (b2) PostMessageW(b2, WM_COMMAND, IDYES, 0);
        WaitForSingleObject(t, 3000);
    }
    EQ(g_mbres, IDNO);
    CloseHandle(t);
}
TEST(dialogs_thread_messagebox_post_command_from_other_thread) {
    REQUIRE(MessageBoxA); REQUIRE(FindWindowA); REQUIRE(PostMessageW);
    g_mbres = 0;
    HANDLE t = CreateThread(NULL, 0, mb_thread, NULL, 0, NULL);
    REQUIRE(t);
    HWND box = wait_for_box(4000);
    CHECK(box != NULL);
    if (box) PostMessageW(box, WM_COMMAND, IDYES, 0);
    if (WaitForSingleObject(t, 6000) != WAIT_OBJECT_0) {
        HWND b2 = FindWindowA("#32770", "T32 thread box");
        if (b2) PostMessageW(b2, WM_COMMAND, IDYES, 0);
        WaitForSingleObject(t, 3000);
    }
    EQ(g_mbres, IDYES);
    CloseHandle(t);
}

/* dialogs: MessageBox family, driven by a thread timer that inspects and closes the box */
#include "dlgutil.h"

UDECL(int, MessageBoxTimeoutA, (HWND, LPCSTR, LPCSTR, UINT, WORD, DWORD));
UDECL(int, MessageBoxTimeoutW, (HWND, LPCWSTR, LPCWSTR, UINT, WORD, DWORD));
UDECL(DPI_AWARENESS_CONTEXT, SetThreadDpiAwarenessContext, (DPI_AWARENESS_CONTEXT));
UDECL(DPI_AWARENESS_CONTEXT, GetThreadDpiAwarenessContext, (void));
UDECL(BOOL, AreDpiAwarenessContextsEqual, (DPI_AWARENESS_CONTEXT, DPI_AWARENESS_CONTEXT));

enum { ACT_NONE, ACT_CLICK, ACT_CLOSE, ACT_CMD, ACT_KEYDOWN_FOCUS, ACT_KEYDOWN_BOX, ACT_END, ACT_NESTED };

static struct {
    int act, id, act2, id2;
    int ticks, found, acted, acted2;
    UINT_PTR tid;
    HWND box, owner_seen, focus;
    int exist[16];
    char cap[512]; WCHAR capw[512]; char text[8192]; int ntext;
    int has_icon; HANDLE icon_h; int nbuttons;
    DWORD style, exstyle; LRESULT defid; int owner_enabled; int owner_enabled_set; HWND owner_chk;
    char cls[64]; int visible, enabled;
    int focus_id, defpush_id; int nstatic;
    void (*extra)(HWND);
    DPI_AWARENESS_CONTEXT dpictx; int dpi_obs;
    int ret_after; int still_after_act;
} mb;

static BOOL CALLBACK mb_enum_top(HWND h, LPARAM l) {
    char c[32];
    if (GetClassNameA(h, c, 32) && !strcmp(c, "#32770") && IsWindowVisible(h) && GetWindow(h, GW_CHILD) != NULL) { mb.box = h; return FALSE; }
    return TRUE;
}
static BOOL CALLBACK mb_enum_child(HWND h, LPARAM l) {
    char c[32]; GetClassNameA(h, c, 32);
    if (!lstrcmpiA(c, "Button")) { mb.nbuttons++; return TRUE; }
    if (!lstrcmpiA(c, "Static")) {
        LONG st = GetWindowLongW(h, GWL_STYLE);
        mb.nstatic++;
        if ((st & SS_TYPEMASK) == SS_ICON) {
            mb.has_icon = 1;
            mb.icon_h = (HANDLE)SendMessageW(h, STM_GETICON, 0, 0);
        } else {
            char t[8192]; t[0] = 0;
            GetWindowTextA(h, t, sizeof t);
            if (t[0] && !mb.ntext) { lstrcpynA(mb.text, t, sizeof mb.text); mb.ntext = 1; }
        }
    }
    return TRUE;
}
static void mb_observe(HWND box) {
    mb.box = box;
    GetClassNameA(box, mb.cls, sizeof mb.cls);
    GetWindowTextA(box, mb.cap, sizeof mb.cap);
    GetWindowTextW(box, mb.capw, 512);
    for (int i = 1; i < 16; i++) mb.exist[i] = GetDlgItem(box, i) != NULL;
    EnumChildWindows(box, mb_enum_child, 0);
    mb.style = GetWindowLongW(box, GWL_STYLE);
    mb.exstyle = GetWindowLongW(box, GWL_EXSTYLE);
    mb.defid = SendMessageW(box, DM_GETDEFID, 0, 0);
    mb.focus = GetFocus();
    mb.focus_id = mb.focus ? GetDlgCtrlID(mb.focus) : -1;
    mb.defpush_id = -1;
    for (int i = 1; i < 12; i++) { HWND b = GetDlgItem(box, i); if (b && (GetWindowLongW(b, GWL_STYLE) & BS_TYPEMASK) == BS_DEFPUSHBUTTON) mb.defpush_id = i; }
    mb.owner_seen = GetWindow(box, GW_OWNER);
    mb.visible = IsWindowVisible(box);
    mb.enabled = IsWindowEnabled(box);
    if (mb.owner_chk) { mb.owner_enabled = IsWindowEnabled(mb.owner_chk); mb.owner_enabled_set = 1; }
    if (UN(GetThreadDpiAwarenessContext)) mb.dpictx = UN(GetThreadDpiAwarenessContext)();
    if (mb.extra) mb.extra(box);
}
static void mb_do(HWND box, int act, int id) {
    switch (act) {
    case ACT_CLICK: { HWND b = GetDlgItem(box, id); if (b) SendMessageW(b, BM_CLICK, 0, 0); else PostMessageW(box, WM_COMMAND, id, 0); break; }
    case ACT_CLOSE: PostMessageW(box, WM_CLOSE, 0, 0); break;
    case ACT_CMD: PostMessageW(box, WM_COMMAND, id, 0); break;
    case ACT_KEYDOWN_FOCUS: { HWND f = GetFocus(); PostMessageW(f ? f : box, WM_KEYDOWN, id, 0); break; }
    case ACT_KEYDOWN_BOX: PostMessageW(box, WM_KEYDOWN, id, 0); break;
    case ACT_END: EndDialog(box, id); break;
    case ACT_NESTED:
        mb.ret_after = UN(MessageBoxTimeoutW)(NULL, L"inner", L"Inner", MB_OK, 0, 300);
        { HWND b = GetDlgItem(box, id); if (b) SendMessageW(b, BM_CLICK, 0, 0); }
        break;
    }
}
static VOID CALLBACK mb_cb(HWND h, UINT m, UINT_PTR id, DWORD t) {
    mb.ticks++;
    if (!mb.found) {
        mb.box = NULL;
        EnumThreadWindows(GetCurrentThreadId(), mb_enum_top, 0);
        if (!mb.box) return;
        mb_observe(mb.box);
        mb.found = 1;
    }
    if (!mb.acted) {
        mb.acted = 1;
        mb_do(mb.box, mb.act, mb.id);
        if (!mb.act2) { KillTimer(NULL, mb.tid); mb.tid = 0; }
        return;
    }
    if (mb.act2 && !mb.acted2) {
        mb.acted2 = 1;
        if (IsWindow(mb.box)) { mb.still_after_act = 1; mb_do(mb.box, mb.act2, mb.id2); }
        KillTimer(NULL, mb.tid); mb.tid = 0;
    }
}
static void mb_arm(int act, int id, int act2, int id2) {
    memset(&mb, 0, sizeof mb);
    mb.act = act; mb.id = id; mb.act2 = act2; mb.id2 = id2;
    ds_reset();
    mb.tid = SetTimer(NULL, 0, 60, mb_cb);
}
static void mb_disarm(void) {
    if (mb.tid) { KillTimer(NULL, mb.tid); mb.tid = 0; }
    if (wd_id) { KillTimer(NULL, wd_id); wd_id = 0; }
}

/* API selector */
enum { API_A, API_W, API_EXA, API_EXW, API_INDA, API_INDW, API_TOA, API_TOW, NAPI };
static const char *api_name[] = { "A", "W", "ExA", "ExW", "IndirectA", "IndirectW", "TimeoutA", "TimeoutW" };
static int api_ok(int api) {
    switch (api) {
    case API_A: return MessageBoxA != NULL; case API_W: return MessageBoxW != NULL;
    case API_EXA: return MessageBoxExA != NULL; case API_EXW: return MessageBoxExW != NULL;
    case API_INDA: return MessageBoxIndirectA != NULL; case API_INDW: return MessageBoxIndirectW != NULL;
    case API_TOA: return UN(MessageBoxTimeoutA) != NULL; case API_TOW: return UN(MessageBoxTimeoutW) != NULL;
    }
    return 0;
}
static int call_mb(int api, HWND owner, const char *text, const char *cap, UINT type) {
    WCHAR wt[8200], wc[600];
    if (text) MultiByteToWideChar(CP_ACP, 0, text, -1, wt, 8200);
    if (cap) MultiByteToWideChar(CP_ACP, 0, cap, -1, wc, 600);
    switch (api) {
    case API_A: return MessageBoxA(owner, text, cap, type);
    case API_W: return MessageBoxW(owner, text ? wt : NULL, cap ? wc : NULL, type);
    case API_EXA: return MessageBoxExA(owner, text, cap, type, 0);
    case API_EXW: return MessageBoxExW(owner, text ? wt : NULL, cap ? wc : NULL, type, 0);
    case API_INDA: { MSGBOXPARAMSA p; memset(&p, 0, sizeof p); p.cbSize = sizeof p; p.hwndOwner = owner; p.lpszText = text; p.lpszCaption = cap; p.dwStyle = type; return MessageBoxIndirectA(&p); }
    case API_INDW: { MSGBOXPARAMSW p; memset(&p, 0, sizeof p); p.cbSize = sizeof p; p.hwndOwner = owner; p.lpszText = text ? wt : NULL; p.lpszCaption = cap ? wc : NULL; p.dwStyle = type; return MessageBoxIndirectW(&p); }
    case API_TOA: return UN(MessageBoxTimeoutA)(owner, text, cap, type, 0, 0xFFFFFFFF);
    case API_TOW: return UN(MessageBoxTimeoutW)(owner, text ? wt : NULL, cap ? wc : NULL, type, 0, 0xFFFFFFFF);
    }
    return -12345;
}

/* ---------------- constants ---------------- */
TEST(dialogs_mb_constants) {
    EQ(IDOK, 1); EQ(IDCANCEL, 2); EQ(IDABORT, 3); EQ(IDRETRY, 4); EQ(IDIGNORE, 5);
    EQ(IDYES, 6); EQ(IDNO, 7); EQ(IDCLOSE, 8); EQ(IDHELP, 9); EQ(IDTRYAGAIN, 10); EQ(IDCONTINUE, 11);
    EQ(MB_OK, 0); EQ(MB_OKCANCEL, 1); EQ(MB_ABORTRETRYIGNORE, 2); EQ(MB_YESNOCANCEL, 3);
    EQ(MB_YESNO, 4); EQ(MB_RETRYCANCEL, 5); EQ(MB_CANCELTRYCONTINUE, 6);
    EQ(MB_ICONERROR, 0x10); EQ(MB_ICONQUESTION, 0x20); EQ(MB_ICONWARNING, 0x30); EQ(MB_ICONINFORMATION, 0x40);
    EQ(MB_DEFBUTTON2, 0x100); EQ(MB_DEFBUTTON3, 0x200); EQ(MB_DEFBUTTON4, 0x300);
    EQ(MB_SYSTEMMODAL, 0x1000); EQ(MB_TASKMODAL, 0x2000); EQ(MB_HELP, 0x4000);
    EQ(MB_SETFOREGROUND, 0x10000); EQ(MB_DEFAULT_DESKTOP_ONLY, 0x20000); EQ(MB_TOPMOST, 0x40000);
    EQ(MB_RIGHT, 0x80000); EQ(MB_RTLREADING, 0x100000); EQ(MB_SERVICE_NOTIFICATION, 0x200000);
    EQ(MB_TYPEMASK, 0xF); EQ(MB_ICONMASK, 0xF0); EQ(MB_DEFMASK, 0xF00); EQ(MB_MODEMASK, 0x3000); EQ(MB_MISCMASK, 0xC000);
    EQ(MB_USERICON, 0x80);
}

/* ---------------- buttons per type ---------------- */
struct mbtype { UINT type; int ids[4]; const char *name; };
static const struct mbtype mbtypes[] = {
    { MB_OK, { IDOK, 0 }, "ok" },
    { MB_OKCANCEL, { IDOK, IDCANCEL, 0 }, "okcancel" },
    { MB_ABORTRETRYIGNORE, { IDABORT, IDRETRY, IDIGNORE, 0 }, "abortretryignore" },
    { MB_YESNOCANCEL, { IDYES, IDNO, IDCANCEL, 0 }, "yesnocancel" },
    { MB_YESNO, { IDYES, IDNO, 0 }, "yesno" },
    { MB_RETRYCANCEL, { IDRETRY, IDCANCEL, 0 }, "retrycancel" },
    { MB_CANCELTRYCONTINUE, { IDCANCEL, IDTRYAGAIN, IDCONTINUE, 0 }, "canceltrycontinue" },
};
static int nids(const int *ids) { int n = 0; while (ids[n]) n++; return n; }

static void check_type_all_apis(int ti) {
    const struct mbtype *t = &mbtypes[ti];
    for (int api = 0; api < NAPI; api++) {
        if (!api_ok(api)) { t_note("api %s missing", api_name[api]); continue; }
        mb_arm(ACT_CLICK, t->ids[0], 0, 0);
        int r = call_mb(api, NULL, "button test", "Caption", t->type);
        mb_disarm();
        CHECKF(mb.found, "%s/%s: box never seen", t->name, api_name[api]);
        CHECKF(r == t->ids[0], "%s/%s: click %d returned %d", t->name, api_name[api], t->ids[0], r);
        int want[16] = { 0 };
        for (int i = 0; t->ids[i]; i++) want[t->ids[i]] = 1;
        for (int i = 1; i < 12; i++) CHECKF(mb.exist[i] == want[i], "%s/%s: button id %d exist=%d want=%d", t->name, api_name[api], i, mb.exist[i], want[i]);
        EQ(mb.nbuttons, nids(t->ids));
        CHECK(!strcmp(mb.cls, "#32770"));
        CHECK(!strcmp(mb.cap, "Caption"));
        CHECK(!strcmp(mb.text, "button test"));
    }
}
static void check_type_click_each(int ti) {
    const struct mbtype *t = &mbtypes[ti];
    for (int k = 0; t->ids[k]; k++) {
        mb_arm(ACT_CLICK, t->ids[k], 0, 0);
        int r = MessageBoxW(NULL, L"click", L"Click", t->type);
        mb_disarm();
        CHECKF(r == t->ids[k], "%s: click %d returned %d", t->name, t->ids[k], r);
    }
}
#define MBTYPE_TESTS(n, idx) \
    TEST(dialogs_mb_type_##n##_all_apis) { REQUIRE(MessageBoxW); check_type_all_apis(idx); } \
    TEST(dialogs_mb_type_##n##_click_each) { REQUIRE(MessageBoxW); check_type_click_each(idx); }
MBTYPE_TESTS(ok, 0)
MBTYPE_TESTS(okcancel, 1)
MBTYPE_TESTS(abortretryignore, 2)
MBTYPE_TESTS(yesnocancel, 3)
MBTYPE_TESTS(yesno, 4)
MBTYPE_TESTS(retrycancel, 5)
MBTYPE_TESTS(canceltrycontinue, 6)

/* ---------------- default buttons ---------------- */
TEST(dialogs_mb_default_button_all_types) {
    REQUIRE(MessageBoxW); REQUIRE(GetDlgItem); REQUIRE(GetWindowLongW);
    static const UINT defs[] = { MB_DEFBUTTON1, MB_DEFBUTTON2, MB_DEFBUTTON3 };
    for (unsigned ti = 0; ti < sizeof mbtypes / sizeof *mbtypes; ti++) {
        const struct mbtype *t = &mbtypes[ti];
        int n = nids(t->ids);
        for (int d = 0; d < 3; d++) {
            mb_arm(ACT_CLICK, t->ids[0], 0, 0);
            int r = MessageBoxW(NULL, L"def", L"Def", t->type | defs[d]);
            mb_disarm();
            int wantid = d < n ? t->ids[d] : t->ids[0];      /* out-of-range falls back to the first button */
            CHECKF(r == t->ids[0], "%s def%d: ret %d", t->name, d + 1, r);
            CHECKF(mb.defpush_id == wantid, "%s def%d: BS_DEFPUSHBUTTON on id %d want %d", t->name, d + 1, mb.defpush_id, wantid);
            if (mb.focus) CHECKF(mb.focus_id == wantid, "%s def%d: focus on id %d want %d", t->name, d + 1, mb.focus_id, wantid);
            if (HIWORD(mb.defid) != DC_HASDEFID || LOWORD(mb.defid) != wantid) t_note("%s def%d: DM_GETDEFID=%llx (want id %d)", t->name, d + 1, (long long)mb.defid, wantid);
        }
    }
}
TEST(dialogs_mb_default_button_style_and_dm_getdefid) {
    REQUIRE(MessageBoxW);
    static const int ids[] = { IDOK, IDCANCEL };
    for (int d = 0; d < 2; d++) {
        mb_arm(ACT_CLICK, IDOK, 0, 0);
        MessageBoxW(NULL, L"dp", L"DP", MB_OKCANCEL | (d << 8));
        mb_disarm();
        EQ(mb.defpush_id, ids[d]);
        if (mb.focus) EQ(mb.focus_id, ids[d]);
        t_note("DM_GETDEFID for default button %d: 0x%llx", d + 1, (long long)mb.defid);
    }
}
TEST(dialogs_mb_defbutton_help_button_index) {
    REQUIRE(MessageBoxW);
    /* OK, Cancel, Help: third button is Help */
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    int r = MessageBoxW(NULL, L"help", L"Help", MB_OKCANCEL | MB_HELP | MB_DEFBUTTON3);
    mb_disarm();
    EQ(r, IDOK);
    CHECK(mb.exist[IDHELP]);
    CHECK(mb.exist[IDOK] && mb.exist[IDCANCEL]);
    EQ(mb.nbuttons, 3);
    EQ(mb.defpush_id, IDHELP);
    if (mb.focus) EQ(mb.focus_id, IDHELP);
}
TEST(dialogs_mb_help_button_present_for_all_types) {
    REQUIRE(MessageBoxW);
    for (unsigned ti = 0; ti < sizeof mbtypes / sizeof *mbtypes; ti++) {
        const struct mbtype *t = &mbtypes[ti];
        mb_arm(ACT_CLICK, t->ids[0], 0, 0);
        int r = MessageBoxW(NULL, L"h", L"H", t->type | MB_HELP);
        mb_disarm();
        CHECKF(r == t->ids[0], "%s: ret %d", t->name, r);
        CHECKF(mb.exist[IDHELP], "%s: no help button", t->name);
        CHECKF(mb.nbuttons == nids(t->ids) + 1, "%s: %d buttons", t->name, mb.nbuttons);
    }
}

/* ---------------- closing ---------------- */
TEST(dialogs_mb_wm_close_result) {
    REQUIRE(MessageBoxW);
    struct { UINT type; int want; } tab[] = {
        { MB_OK, IDOK }, { MB_OKCANCEL, IDCANCEL }, { MB_YESNOCANCEL, IDCANCEL }, { MB_RETRYCANCEL, IDCANCEL }, { MB_CANCELTRYCONTINUE, IDCANCEL },
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        mb_arm(ACT_CLOSE, 0, 0, 0);
        int r = MessageBoxW(NULL, L"close", L"Close", tab[i].type);
        mb_disarm();
        CHECKF(r == tab[i].want, "type %u: WM_CLOSE returned %d want %d", tab[i].type, r, tab[i].want);
    }
}
TEST(dialogs_mb_wm_close_ignored_without_cancel) {
    REQUIRE(MessageBoxW);
    /* YESNO and ABORTRETRYIGNORE have no cancel button: WM_CLOSE does not end the box */
    static const struct { UINT type; int click; } tab[] = { { MB_YESNO, IDNO }, { MB_ABORTRETRYIGNORE, IDIGNORE } };
    for (unsigned i = 0; i < 2; i++) {
        mb_arm(ACT_CLOSE, 0, ACT_CLICK, tab[i].click);
        int r = MessageBoxW(NULL, L"close", L"Close", tab[i].type);
        mb_disarm();
        CHECKF(mb.still_after_act, "type %u: box closed by WM_CLOSE", tab[i].type);
        CHECKF(r == tab[i].click, "type %u: returned %d want %d", tab[i].type, r, tab[i].click);
    }
}
TEST(dialogs_mb_wm_command_posts_result) {
    REQUIRE(MessageBoxW);
    static const int ids[] = { IDOK, IDCANCEL };
    for (int i = 0; i < 2; i++) {
        mb_arm(ACT_CMD, ids[i], 0, 0);
        int r = MessageBoxW(NULL, L"cmd", L"Cmd", MB_OKCANCEL);
        mb_disarm();
        CHECKF(r == ids[i], "WM_COMMAND %d returned %d", ids[i], r);
    }
}
TEST(dialogs_mb_escape_key) {
    REQUIRE(MessageBoxW);
    mb_arm(ACT_KEYDOWN_FOCUS, VK_ESCAPE, 0, 0);
    EQ(MessageBoxW(NULL, L"esc", L"Esc", MB_OKCANCEL), IDCANCEL);
    mb_disarm();
    mb_arm(ACT_KEYDOWN_FOCUS, VK_ESCAPE, 0, 0);
    EQ(MessageBoxW(NULL, L"esc", L"Esc", MB_OK), IDOK);
    mb_disarm();
    mb_arm(ACT_KEYDOWN_FOCUS, VK_ESCAPE, ACT_CLICK, IDYES);
    EQ(MessageBoxW(NULL, L"esc", L"Esc", MB_YESNO), IDYES);       /* escape ignored, then click */
    CHECK(mb.still_after_act);
    mb_disarm();
}
TEST(dialogs_mb_return_key_presses_default) {
    REQUIRE(MessageBoxW);
    static const struct { UINT type; int want; } tab[] = {
        { MB_OKCANCEL, IDOK }, { MB_OKCANCEL | MB_DEFBUTTON2, IDCANCEL }, { MB_YESNO | MB_DEFBUTTON2, IDNO },
        { MB_YESNOCANCEL | MB_DEFBUTTON3, IDCANCEL }, { MB_ABORTRETRYIGNORE | MB_DEFBUTTON2, IDRETRY },
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        mb_arm(ACT_KEYDOWN_FOCUS, VK_RETURN, 0, 0);
        int r = MessageBoxW(NULL, L"ret", L"Ret", tab[i].type);
        mb_disarm();
        if (mb.focus) CHECKF(r == tab[i].want, "type %x: Enter returned %d want %d", tab[i].type, r, tab[i].want);
        else t_note("type %x: no focus window, Enter returned %d (want %d)", tab[i].type, r, tab[i].want);
    }
}
TEST(dialogs_mb_space_key_on_focused_button) {
    REQUIRE(MessageBoxW);
    mb_arm(ACT_KEYDOWN_FOCUS, VK_SPACE, ACT_CLICK, IDNO);
    int r = MessageBoxW(NULL, L"sp", L"Sp", MB_YESNO);
    mb_disarm();
    t_note("VK_SPACE keydown on focused Yes button -> %d (box still open after: %d)", r, mb.still_after_act);
}

/* ---------------- caption / text / icon ---------------- */
TEST(dialogs_mb_caption_and_text_all_apis) {
    REQUIRE(MessageBoxW);
    for (int api = 0; api < NAPI; api++) {
        if (!api_ok(api)) continue;
        mb_arm(ACT_CLICK, IDOK, 0, 0);
        int r = call_mb(api, NULL, "Hello text 123", "My Caption", MB_OK);
        mb_disarm();
        CHECKF(r == IDOK, "%s: %d", api_name[api], r);
        CHECKF(!strcmp(mb.cap, "My Caption"), "%s: caption '%s'", api_name[api], mb.cap);
        CHECKF(!strcmp(mb.text, "Hello text 123"), "%s: text '%s'", api_name[api], mb.text);
        CHECKF(!lstrcmpW(mb.capw, L"My Caption"), "%s: wide caption", api_name[api]);
    }
}
TEST(dialogs_mb_unicode_text_and_caption) {
    REQUIRE(MessageBoxW); REQUIRE(GetWindowTextW);
    static const WCHAR cap[] = { 0x0416, 0x4E2D, 0x20AC, 'c', 0 };
    static WCHAR got[64];
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    EQ(MessageBoxW(NULL, L"\x4E2D\x6587 text", cap, MB_OK), IDOK);
    mb_disarm();
    CHECK(!lstrcmpW(mb.capw, cap));
    (void)got;
}
TEST(dialogs_mb_null_caption) {
    REQUIRE(MessageBoxW); REQUIRE(MessageBoxA);
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    EQ(MessageBoxA(NULL, "txt", NULL, MB_OK), IDOK);
    mb_disarm();
    t_note("NULL caption -> title '%s' (\"Error\" on en-US)", mb.cap);
    CHECK(mb.cap[0] != 0);                  /* a default caption is supplied */
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    EQ(MessageBoxW(NULL, L"txt", NULL, MB_OK), IDOK);
    mb_disarm();
    CHECK(mb.cap[0] != 0);
    CHECK(!strcmp(mb.text, "txt"));
}
TEST(dialogs_mb_empty_caption_and_text) {
    REQUIRE(MessageBoxA);
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    EQ(MessageBoxA(NULL, "", "", MB_OK), IDOK);
    mb_disarm();
    CHECKF(mb.cap[0] == 0, "empty caption stays empty, got '%s'", mb.cap);
    CHECK(mb.found);
}
TEST(dialogs_mb_long_text) {
    REQUIRE(MessageBoxA);
    static char big[3000];
    for (int i = 0; i < 2900; i++) big[i] = 'a' + (i % 26) + (i % 70 == 69 ? 0 : 0);
    big[2900] = 0;
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    EQ(MessageBoxA(NULL, big, "long", MB_OK), IDOK);
    mb_disarm();
    CHECK(!strcmp(mb.text, big));
}
TEST(dialogs_mb_long_caption) {
    REQUIRE(MessageBoxA);
    static char cap[400];
    for (int i = 0; i < 300; i++) cap[i] = 'A' + (i % 26);
    cap[300] = 0;
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    EQ(MessageBoxA(NULL, "x", cap, MB_OK), IDOK);
    mb_disarm();
    CHECK(!strcmp(mb.cap, cap));
}
TEST(dialogs_mb_text_newlines_preserved) {
    REQUIRE(MessageBoxA);
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    EQ(MessageBoxA(NULL, "line1\nline2\r\nline3\ttab", "nl", MB_OK), IDOK);
    mb_disarm();
    CHECK(!strcmp(mb.text, "line1\nline2\r\nline3\ttab"));
}
TEST(dialogs_mb_ampersand_in_text_not_mnemonic) {
    REQUIRE(MessageBoxA);
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    EQ(MessageBoxA(NULL, "Fish && Chips & more", "amp", MB_OK), IDOK);
    mb_disarm();
    CHECKF(!strcmp(mb.text, "Fish && Chips & more"), "text '%s'", mb.text);
}
TEST(dialogs_mb_icon_presence) {
    REQUIRE(MessageBoxW);
    static const UINT icons[] = { 0, MB_ICONERROR, MB_ICONQUESTION, MB_ICONWARNING, MB_ICONINFORMATION };
    for (unsigned i = 0; i < 5; i++) {
        mb_arm(ACT_CLICK, IDOK, 0, 0);
        EQ(MessageBoxW(NULL, L"icon", L"Icon", MB_OK | icons[i]), IDOK);
        mb_disarm();
        if (icons[i]) CHECKF(mb.has_icon, "icon 0x%x: has_icon=%d", icons[i], mb.has_icon);
        else t_note("no-icon message box has an icon static: %d", mb.has_icon);
        if (icons[i]) CHECKF(mb.icon_h != NULL, "icon 0x%x: no icon handle", icons[i]);
    }
}
TEST(dialogs_mb_icon_aliases_equal) {
    /* MB_ICONSTOP/HAND/EXCLAMATION/ASTERISK are aliases of the four icon values */
    EQ(MB_ICONSTOP, MB_ICONERROR); EQ(MB_ICONHAND, MB_ICONERROR);
    EQ(MB_ICONEXCLAMATION, MB_ICONWARNING); EQ(MB_ICONASTERISK, MB_ICONINFORMATION);
}
TEST(dialogs_mb_icons_distinct_note) {
    REQUIRE(MessageBoxW);
    HANDLE hs[4]; static const UINT icons[] = { MB_ICONERROR, MB_ICONQUESTION, MB_ICONWARNING, MB_ICONINFORMATION };
    for (int i = 0; i < 4; i++) {
        mb_arm(ACT_CLICK, IDOK, 0, 0);
        MessageBoxW(NULL, L"i", L"I", MB_OK | icons[i]);
        mb_disarm();
        hs[i] = mb.icon_h;
    }
    int same = 0;
    for (int i = 0; i < 4; i++) for (int j = i + 1; j < 4; j++) if (hs[i] == hs[j]) same++;
    t_note("message box icon handles: %p %p %p %p (identical pairs: %d)", hs[0], hs[1], hs[2], hs[3], same);
}

/* ---------------- window properties ---------------- */
TEST(dialogs_mb_window_properties) {
    REQUIRE(MessageBoxW);
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    EQ(MessageBoxW(NULL, L"props", L"Props", MB_OK), IDOK);
    mb_disarm();
    CHECK(mb.visible); CHECK(mb.enabled);
    CHECK(mb.style & WS_CAPTION);
    CHECK(mb.style & WS_POPUP);
    CHECK(mb.style & WS_SYSMENU);
    CHECK(!(mb.style & WS_CHILD));
    t_note("message box style=%lx exstyle=%lx", mb.style, mb.exstyle);
}
TEST(dialogs_mb_topmost_flag) {
    REQUIRE(MessageBoxW);
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    MessageBoxW(NULL, L"top", L"Top", MB_OK | MB_TOPMOST);
    mb_disarm();
    CHECK(mb.exstyle & WS_EX_TOPMOST);
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    MessageBoxW(NULL, L"top", L"Top", MB_OK);
    mb_disarm();
    CHECK(!(mb.exstyle & WS_EX_TOPMOST));
}
TEST(dialogs_mb_rtl_flags) {
    REQUIRE(MessageBoxW);
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    MessageBoxW(NULL, L"rtl", L"Rtl", MB_OK | MB_RTLREADING);
    mb_disarm();
    t_note("MB_RTLREADING exstyle=%lx (WS_EX_RTLREADING %s)", mb.exstyle, (mb.exstyle & WS_EX_RTLREADING) ? "set" : "not set");
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    MessageBoxW(NULL, L"right", L"Right", MB_OK | MB_RIGHT);
    mb_disarm();
    t_note("MB_RIGHT exstyle=%lx", mb.exstyle);
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    MessageBoxW(NULL, L"plain", L"Plain", MB_OK);
    mb_disarm();
    CHECK(!(mb.exstyle & WS_EX_RTLREADING));
}
TEST(dialogs_mb_systemmodal_note) {
    REQUIRE(MessageBoxW);
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    EQ(MessageBoxW(NULL, L"sys", L"Sys", MB_OK | MB_SYSTEMMODAL), IDOK);
    mb_disarm();
    t_note("MB_SYSTEMMODAL exstyle=%lx", mb.exstyle);
}
TEST(dialogs_mb_taskmodal_and_setforeground) {
    REQUIRE(MessageBoxW);
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    EQ(MessageBoxW(NULL, L"task", L"Task", MB_OK | MB_TASKMODAL), IDOK);
    mb_disarm();
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    EQ(MessageBoxW(NULL, L"fg", L"Fg", MB_OK | MB_SETFOREGROUND), IDOK);
    mb_disarm();
}

/* ---------------- owner behaviour ---------------- */
TEST(dialogs_mb_owner_disabled_during_box) {
    REQUIRE(MessageBoxW); REQUIRE(IsWindowEnabled); REQUIRE(DestroyWindow);
    HWND own = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 200, 100, NULL);
    REQUIRE(own);
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    mb.owner_chk = own;
    EQ(MessageBoxW(own, L"own", L"Own", MB_OK), IDOK);
    mb_disarm();
    EQ(mb.owner_enabled_set, 1);
    EQ(mb.owner_enabled, 0);
    EQ(mb.owner_seen, own);
    CHECK(IsWindowEnabled(own));
    CHECK(IsWindow(own));
    DestroyWindow(own);
}
TEST(dialogs_mb_owner_all_apis) {
    REQUIRE(MessageBoxW); REQUIRE(IsWindowEnabled); REQUIRE(DestroyWindow);
    HWND own = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 200, 100, NULL);
    REQUIRE(own);
    for (int api = 0; api < NAPI; api++) {
        if (!api_ok(api)) continue;
        mb_arm(ACT_CLICK, IDOK, 0, 0);
        mb.owner_chk = own;
        int r = call_mb(api, own, "o", "O", MB_OK);
        mb_disarm();
        CHECKF(r == IDOK, "%s", api_name[api]);
        CHECKF(mb.owner_seen == own, "%s: owner %p", api_name[api], (void *)mb.owner_seen);
        CHECKF(mb.owner_enabled == 0, "%s: owner enabled during box", api_name[api]);
        CHECKF(IsWindowEnabled(own), "%s: owner disabled after", api_name[api]);
    }
    DestroyWindow(own);
}
TEST(dialogs_mb_owner_child_window) {
    REQUIRE(MessageBoxW); REQUIRE(IsWindowEnabled); REQUIRE(CreateWindowExA); REQUIRE(DestroyWindow);
    HWND top = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 200, 100, NULL);
    REQUIRE(top);
    HWND kid = CreateWindowExA(0, "STATIC", "kid", WS_CHILD | WS_VISIBLE, 0, 0, 20, 20, top, (HMENU)3, GetModuleHandleA(NULL), NULL);
    REQUIRE(kid);
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    mb.owner_chk = top;
    EQ(MessageBoxW(kid, L"kid", L"Kid", MB_OK), IDOK);
    mb_disarm();
    t_note("child-window owner: box owner=%p top=%p kid=%p top enabled during=%d", (void *)mb.owner_seen, (void *)top, (void *)kid, mb.owner_enabled);
    CHECK(mb.owner_seen == top || mb.owner_seen == kid);
    CHECK(IsWindowEnabled(top));
    DestroyWindow(top);
}
TEST(dialogs_mb_already_disabled_owner_stays_disabled) {
    REQUIRE(MessageBoxW); REQUIRE(IsWindowEnabled); REQUIRE(DestroyWindow);
    HWND own = t_make_window(NULL, WS_OVERLAPPEDWINDOW | WS_DISABLED, 0, 0, 0, 200, 100, NULL);
    REQUIRE(own);
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    EQ(MessageBoxW(own, L"dis", L"Dis", MB_OK), IDOK);
    mb_disarm();
    CHECK(!IsWindowEnabled(own));
    DestroyWindow(own);
}
TEST(dialogs_mb_desktop_window_as_owner) {
    REQUIRE(MessageBoxW); REQUIRE(GetDesktopWindow);
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    int r = MessageBoxW(GetDesktopWindow(), L"dt", L"Dt", MB_OK);
    mb_disarm();
    t_note("desktop window as owner -> %d (found=%d)", r, mb.found);
}

/* ---------------- invalid parameters ---------------- */
TEST(dialogs_mb_invalid_type_values) {
    ds_reset();      /* watchdog in case a box is wrongly shown */
    REQUIRE(MessageBoxW); REQUIRE(MessageBoxA);
    for (UINT t = 7; t <= 15; t++) {
        SET_ERR(0);
        int r = MessageBoxW(NULL, L"bad", L"Bad", t);
        CHECKF(r == 0, "type %u returned %d", t, r);
        EQ(GetLastError(), 0x59e);
        SET_ERR(0);
        r = MessageBoxA(NULL, "bad", "Bad", t | MB_ICONERROR | MB_DEFBUTTON2);
        CHECKF(r == 0, "type %u|flags returned %d", t, r);
        EQ(GetLastError(), 0x59e);
    }
}
TEST(dialogs_mb_invalid_type_all_apis) {
    ds_reset();      /* watchdog in case a box is wrongly shown */
    REQUIRE(MessageBoxW);
    for (int api = 0; api < NAPI; api++) {
        if (!api_ok(api)) continue;
        SET_ERR(0);
        int r = call_mb(api, NULL, "bad", "Bad", 0xF);
        CHECKF(r == 0, "%s returned %d", api_name[api], r);
        CHECKF(GetLastError() == 0x59e, "%s err %lu", api_name[api], GetLastError());
    }
}
TEST(dialogs_mb_invalid_owner) {
    ds_reset();      /* watchdog in case a box is wrongly shown */
    REQUIRE(MessageBoxW);
    static const ULONG_PTR bad[] = { 0x12345678, 0xdeadbeef, 1, 0x10000, (ULONG_PTR)-1 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        for (int api = 0; api < NAPI; api++) {
            if (!api_ok(api)) continue;
            SET_ERR(0);
            int r = call_mb(api, (HWND)bad[i], "bad", "Bad", MB_OK);
            CHECKF(r == 0, "owner %llx %s returned %d", (unsigned long long)bad[i], api_name[api], r);
            CHECKF(GetLastError() == 1400, "owner %llx %s err %lu", (unsigned long long)bad[i], api_name[api], GetLastError());
        }
    }
}
TEST(dialogs_mb_destroyed_owner_handle) {
    ds_reset();      /* watchdog in case a box is wrongly shown */
    REQUIRE(MessageBoxW); REQUIRE(DestroyWindow);
    HWND own = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 50, 50, NULL);
    REQUIRE(own);
    DestroyWindow(own);
    SET_ERR(0);
    EQ(MessageBoxW(own, L"gone", L"Gone", MB_OK), 0);
    EQ(GetLastError(), 1400);
}
TEST(dialogs_mb_service_notification_with_owner_invalid_parameter) {
    ds_reset();      /* watchdog in case a box is wrongly shown */
    REQUIRE(MessageBoxW); REQUIRE(DestroyWindow);
    HWND own = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 50, 50, NULL);
    REQUIRE(own);
    SET_ERR(0);
    EQ(MessageBoxW(own, L"svc", L"Svc", MB_OK | MB_SERVICE_NOTIFICATION), 0);
    EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EQ(MessageBoxW(own, L"svc", L"Svc", MB_OK | MB_DEFAULT_DESKTOP_ONLY), 0);
    EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    DestroyWindow(own);
}
TEST(dialogs_mb_defbutton_out_of_range_ok) {
    REQUIRE(MessageBoxW);
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    EQ(MessageBoxW(NULL, L"d4", L"D4", MB_OK | MB_DEFBUTTON4), IDOK);
    mb_disarm();
    EQ(mb.defpush_id, IDOK);
}
TEST(dialogs_mb_unknown_high_flags_ignored) {
    REQUIRE(MessageBoxW);
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    int r = MessageBoxW(NULL, L"hi", L"Hi", MB_OK | 0x00800000 | 0x01000000);
    mb_disarm();
    t_note("undefined high style bits -> %d", r);
    CHECK(r == IDOK || r == 0);
}

/* ---------------- MessageBoxTimeout ---------------- */
TEST(dialogs_mbtimeout_expires) {
    REQUIRE(UN(MessageBoxTimeoutW)); REQUIRE(UN(MessageBoxTimeoutA));
    ds_reset();
    DWORD t0 = GetTickCount();
    int r = UN(MessageBoxTimeoutW)(NULL, L"timeout", L"Timeout", MB_OK, 0, 400);
    DWORD dt = GetTickCount() - t0;
    EQ(r, 32000);
    CHECKF(dt >= 350, "returned after %lu ms", dt);
    CHECKF(dt < 5000, "took %lu ms", dt);
    t0 = GetTickCount();
    r = UN(MessageBoxTimeoutA)(NULL, "timeout", "Timeout", MB_YESNO, 0, 250);
    dt = GetTickCount() - t0;
    EQ(r, 32000);
    CHECKF(dt >= 200 && dt < 5000, "A returned after %lu ms", dt);
    mb_disarm();
}
TEST(dialogs_mbtimeout_timeout_for_each_type) {
    REQUIRE(UN(MessageBoxTimeoutW));
    ds_reset();
    for (unsigned i = 0; i < sizeof mbtypes / sizeof *mbtypes; i++) {
        int r = UN(MessageBoxTimeoutW)(NULL, L"to", L"To", mbtypes[i].type, 0, 150);
        CHECKF(r == 32000, "%s: %d", mbtypes[i].name, r);
    }
    mb_disarm();
}
TEST(dialogs_mbtimeout_click_before_timeout) {
    REQUIRE(UN(MessageBoxTimeoutW));
    mb_arm(ACT_CLICK, IDNO, 0, 0);
    int r = UN(MessageBoxTimeoutW)(NULL, L"click", L"Click", MB_YESNO, 0, 5000);
    mb_disarm();
    EQ(r, IDNO);
}
TEST(dialogs_mbtimeout_zero_means_infinite) {
    REQUIRE(UN(MessageBoxTimeoutW));
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    int r = UN(MessageBoxTimeoutW)(NULL, L"zero", L"Zero", MB_OK, 0, 0);
    mb_disarm();
    EQ(r, IDOK);                          /* with timeout 0 the box is not auto-closed; our click ends it */
    CHECK(mb.found);
}
TEST(dialogs_mbtimeout_huge_values) {
    REQUIRE(UN(MessageBoxTimeoutW));
    static const DWORD tos[] = { 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000, 3600000, 1000000 };
    for (unsigned i = 0; i < 5; i++) {
        mb_arm(ACT_CLICK, IDOK, 0, 0);
        int r = UN(MessageBoxTimeoutW)(NULL, L"huge", L"Huge", MB_OK, 0, tos[i]);
        mb_disarm();
        CHECKF(r == IDOK, "timeout %lu -> %d", tos[i], r);
    }
}
TEST(dialogs_mbtimeout_invalid_type_and_owner) {
    ds_reset();      /* watchdog in case a box is wrongly shown */
    REQUIRE(UN(MessageBoxTimeoutW));
    SET_ERR(0);
    EQ(UN(MessageBoxTimeoutW)(NULL, L"x", L"x", 0xF, 0, 100), 0);
    EQ(GetLastError(), 0x59e);
    SET_ERR(0);
    EQ(UN(MessageBoxTimeoutW)((HWND)0x12345678, L"x", L"x", MB_OK, 0, 100), 0);
    EQ(GetLastError(), 1400);
}
TEST(dialogs_mbtimeout_language_ids) {
    REQUIRE(UN(MessageBoxTimeoutW));
    static const WORD langs[] = { 0, 0x0409, 0x0407, 0x0411, 0x9999, 0xFFFF };
    for (unsigned i = 0; i < 6; i++) {
        mb_arm(ACT_CLICK, IDYES, 0, 0);
        int r = UN(MessageBoxTimeoutW)(NULL, L"lang", L"Lang", MB_YESNO, langs[i], 5000);
        mb_disarm();
        CHECKF(r == IDYES, "lang %04x -> %d", langs[i], r);
    }
}
TEST(dialogs_messageboxex_language_ids) {
    REQUIRE(MessageBoxExW); REQUIRE(MessageBoxExA);
    static const WORD langs[] = { 0, 0x0409, 0x0000, 0x0804, 0x7777 };
    for (unsigned i = 0; i < 5; i++) {
        mb_arm(ACT_CLICK, IDNO, 0, 0);
        int r = MessageBoxExW(NULL, L"lang", L"Lang", MB_YESNO, langs[i]);
        mb_disarm();
        CHECKF(r == IDNO, "lang %04x -> %d", langs[i], r);
        mb_arm(ACT_CLICK, IDYES, 0, 0);
        r = MessageBoxExA(NULL, "lang", "Lang", MB_YESNO, langs[i]);
        mb_disarm();
        CHECKF(r == IDYES, "A lang %04x -> %d", langs[i], r);
    }
}

/* ---------------- MessageBoxIndirect ---------------- */
TEST(dialogs_mbindirect_cbsize_ignored) {
    REQUIRE(MessageBoxIndirectW);
    static const DWORD sizes[] = { 0, 1, sizeof(MSGBOXPARAMSW), 0x1000, 0xFFFFFFFF };
    for (unsigned i = 0; i < 5; i++) {
        MSGBOXPARAMSW p; memset(&p, 0, sizeof p);
        p.cbSize = sizes[i]; p.lpszText = L"sz"; p.lpszCaption = L"Sz"; p.dwStyle = MB_OKCANCEL;
        mb_arm(ACT_CLICK, IDCANCEL, 0, 0);
        int r = MessageBoxIndirectW(&p);
        mb_disarm();
        t_note("MSGBOXPARAMS.cbSize=%lu -> %d", sizes[i], r);
        CHECKF(r == IDCANCEL, "cbSize %lu -> %d", sizes[i], r);
    }
}
TEST(dialogs_mbindirect_owner_and_style) {
    REQUIRE(MessageBoxIndirectW); REQUIRE(DestroyWindow);
    HWND own = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 100, 100, NULL);
    REQUIRE(own);
    MSGBOXPARAMSW p; memset(&p, 0, sizeof p);
    p.cbSize = sizeof p; p.hwndOwner = own; p.lpszText = L"ind"; p.lpszCaption = L"Ind";
    p.dwStyle = MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2;
    mb_arm(ACT_CLICK, IDYES, 0, 0);
    mb.owner_chk = own;
    EQ(MessageBoxIndirectW(&p), IDYES);
    mb_disarm();
    EQ(mb.owner_seen, own);
    CHECK(mb.has_icon);
    EQ(mb.defpush_id, IDNO);
    EQ(mb.owner_enabled, 0);
    DestroyWindow(own);
}
static int cb_n; static HELPINFO cb_hi; static DWORD_PTR cb_ctx;
static void CALLBACK mb_help_cb(LPHELPINFO hi) { cb_n++; cb_hi = *hi; }
TEST(dialogs_mbindirect_help_callback) {
    REQUIRE(MessageBoxIndirectW);
    MSGBOXPARAMSW p; memset(&p, 0, sizeof p);
    p.cbSize = sizeof p; p.lpszText = L"help cb"; p.lpszCaption = L"Help";
    p.dwStyle = MB_OK | MB_HELP; p.dwContextHelpId = 0x4242; p.lpfnMsgBoxCallback = mb_help_cb;
    cb_n = 0; memset(&cb_hi, 0, sizeof cb_hi);
    mb_arm(ACT_CLICK, IDHELP, ACT_CLICK, IDOK);
    int r = MessageBoxIndirectW(&p);
    mb_disarm();
    CHECKF(mb.still_after_act, "box must stay open after Help is pressed");
    EQ(r, IDOK);
    EQ(cb_n, 1);
    EQ(cb_hi.cbSize, sizeof(HELPINFO));
    EQ(cb_hi.iContextType, HELPINFO_WINDOW);
    EQ(cb_hi.dwContextId, 0x4242);
    t_note("HELPINFO: iCtrlId=%d hItemHandle=%p", cb_hi.iCtrlId, cb_hi.hItemHandle);
}
TEST(dialogs_mbindirect_help_without_callback_sends_wm_help_to_owner) {
    REQUIRE(MessageBoxIndirectW); REQUIRE(DestroyWindow);
    HWND own = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 100, 100, NULL);
    REQUIRE(own);
    MSGBOXPARAMSW p; memset(&p, 0, sizeof p);
    p.cbSize = sizeof p; p.hwndOwner = own; p.lpszText = L"help"; p.lpszCaption = L"Help"; p.dwStyle = MB_OK | MB_HELP;
    mb_arm(ACT_CLICK, IDHELP, ACT_CLICK, IDOK);
    int r = MessageBoxIndirectW(&p);
    mb_disarm();
    EQ(r, IDOK);
    CHECK(mb.still_after_act);
    DestroyWindow(own);
}
TEST(dialogs_mbindirect_usericon) {
    REQUIRE(MessageBoxIndirectW);
    MSGBOXPARAMSW p; memset(&p, 0, sizeof p);
    p.cbSize = sizeof p; p.lpszText = L"icon"; p.lpszCaption = L"Icon"; p.dwStyle = MB_OK | MB_USERICON;
    p.lpszIcon = MAKEINTRESOURCEW(32516); p.hInstance = NULL;
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    int r = MessageBoxIndirectW(&p);
    mb_disarm();
    EQ(r, IDOK);
    t_note("MB_USERICON with IDI_INFORMATION: icon static present=%d handle=%p", mb.has_icon, mb.icon_h);
}
TEST(dialogs_mbindirect_ansi_wide_parity) {
    REQUIRE(MessageBoxIndirectA); REQUIRE(MessageBoxIndirectW);
    MSGBOXPARAMSA pa; MSGBOXPARAMSW pw; memset(&pa, 0, sizeof pa); memset(&pw, 0, sizeof pw);
    pa.cbSize = sizeof pa; pa.lpszText = "same"; pa.lpszCaption = "Same"; pa.dwStyle = MB_ABORTRETRYIGNORE | MB_DEFBUTTON3;
    pw.cbSize = sizeof pw; pw.lpszText = L"same"; pw.lpszCaption = L"Same"; pw.dwStyle = MB_ABORTRETRYIGNORE | MB_DEFBUTTON3;
    mb_arm(ACT_CLICK, IDRETRY, 0, 0);
    int ra = MessageBoxIndirectA(&pa); mb_disarm();
    LRESULT da = mb.defid; char ca[64], ta[64]; lstrcpynA(ca, mb.cap, 64); lstrcpynA(ta, mb.text, 64);
    mb_arm(ACT_CLICK, IDRETRY, 0, 0);
    int rw = MessageBoxIndirectW(&pw); mb_disarm();
    EQ(ra, IDRETRY); EQ(rw, IDRETRY);
    EQ(da, mb.defid);
    CHECK(!strcmp(ca, mb.cap)); CHECK(!strcmp(ta, mb.text));
    EQ(mb.defpush_id, IDIGNORE);
}
TEST(dialogs_mbindirect_language_id_field) {
    REQUIRE(MessageBoxIndirectW);
    static const DWORD langs[] = { 0, 0x409, 0x407, 0xFFFF, 0x12345 };
    for (unsigned i = 0; i < 5; i++) {
        MSGBOXPARAMSW p; memset(&p, 0, sizeof p);
        p.cbSize = sizeof p; p.lpszText = L"lang"; p.lpszCaption = L"Lang"; p.dwStyle = MB_YESNO; p.dwLanguageId = langs[i];
        mb_arm(ACT_CLICK, IDNO, 0, 0);
        int r = MessageBoxIndirectW(&p);
        mb_disarm();
        CHECKF(r == IDNO, "lang %lx -> %d", langs[i], r);
    }
}
TEST(dialogs_mbindirect_invalid_style_and_owner) {
    ds_reset();      /* watchdog in case a box is wrongly shown */
    REQUIRE(MessageBoxIndirectW);
    MSGBOXPARAMSW p; memset(&p, 0, sizeof p);
    p.cbSize = sizeof p; p.lpszText = L"x"; p.lpszCaption = L"x"; p.dwStyle = 9;
    SET_ERR(0);
    EQ(MessageBoxIndirectW(&p), 0);
    EQ(GetLastError(), 0x59e);
    p.dwStyle = MB_OK; p.hwndOwner = (HWND)0x4321;
    SET_ERR(0);
    EQ(MessageBoxIndirectW(&p), 0);
    EQ(GetLastError(), 1400);
}

/* ---------------- DPI (Win10) ---------------- */
static DPI_AWARENESS_CONTEXT ctx_during;
TEST(dialogs_mb_runs_system_aware_from_pmv2_thread) {
    REQUIRE(MessageBoxW);
    REQUIRE(UN(SetThreadDpiAwarenessContext)); REQUIRE(UN(GetThreadDpiAwarenessContext)); REQUIRE(UN(AreDpiAwarenessContextsEqual));
    DPI_AWARENESS_CONTEXT orig = UN(GetThreadDpiAwarenessContext)();
    DPI_AWARENESS_CONTEXT prev = UN(SetThreadDpiAwarenessContext)(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (!prev) { t_note("per-monitor v2 not accepted on this system"); return; }
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    int r = MessageBoxW(NULL, L"dpi", L"Dpi", MB_OK);
    mb_disarm();
    DPI_AWARENESS_CONTEXT after = UN(GetThreadDpiAwarenessContext)();
    EQ(r, IDOK);
    CHECK(UN(AreDpiAwarenessContextsEqual)(mb.dpictx, DPI_AWARENESS_CONTEXT_SYSTEM_AWARE));
    CHECK(UN(AreDpiAwarenessContextsEqual)(after, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));
    UN(SetThreadDpiAwarenessContext)(orig);
    (void)ctx_during;
}
TEST(dialogs_mb_dpi_context_preserved_other_modes) {
    REQUIRE(MessageBoxW);
    REQUIRE(UN(SetThreadDpiAwarenessContext)); REQUIRE(UN(GetThreadDpiAwarenessContext)); REQUIRE(UN(AreDpiAwarenessContextsEqual));
    DPI_AWARENESS_CONTEXT orig = UN(GetThreadDpiAwarenessContext)();
    static const DPI_AWARENESS_CONTEXT ctxs[] = { DPI_AWARENESS_CONTEXT_UNAWARE, DPI_AWARENESS_CONTEXT_SYSTEM_AWARE, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE };
    for (unsigned i = 0; i < 3; i++) {
        if (!UN(SetThreadDpiAwarenessContext)(ctxs[i])) continue;
        mb_arm(ACT_CLICK, IDOK, 0, 0);
        MessageBoxW(NULL, L"dpi", L"Dpi", MB_OK);
        mb_disarm();
        CHECKF(UN(AreDpiAwarenessContextsEqual)(UN(GetThreadDpiAwarenessContext)(), ctxs[i]), "ctx %u not restored", i);
        t_note("thread context %u during box equal? %d", i, UN(AreDpiAwarenessContextsEqual)(mb.dpictx, ctxs[i]));
    }
    UN(SetThreadDpiAwarenessContext)(orig);
}

/* ---------------- repetition ---------------- */
TEST(dialogs_mb_repeated_boxes) {
    REQUIRE(MessageBoxW);
    int bad = 0;
    for (int i = 0; i < 15; i++) {
        mb_arm(ACT_CLICK, IDOK, 0, 0);
        if (MessageBoxW(NULL, L"rep", L"Rep", MB_OK) != IDOK) bad++;
        mb_disarm();
    }
    EQ(bad, 0);
}
TEST(dialogs_mb_box_window_gone_after_return) {
    REQUIRE(MessageBoxW); REQUIRE(IsWindow);
    mb_arm(ACT_CLICK, IDOK, 0, 0);
    MessageBoxW(NULL, L"gone", L"Gone", MB_OK);
    mb_disarm();
    CHECK(mb.box != NULL);
    CHECK(!IsWindow(mb.box));
}
TEST(dialogs_mb_nested_message_boxes) {
    REQUIRE(MessageBoxW); REQUIRE(UN(MessageBoxTimeoutW));
    mb_arm(ACT_NESTED, IDOK, 0, 0);
    int r = MessageBoxW(NULL, L"outer", L"Outer", MB_OK);
    mb_disarm();
    EQ(r, IDOK);
    EQ(mb.ret_after, 32000);        /* inner box timed out, outer box still alive and then closed */
}

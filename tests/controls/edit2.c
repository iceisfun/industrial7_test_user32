#include "ctl.h"

#ifndef EM_SETIMESTATUS
#define EM_SETIMESTATUS 0x00D8
#define EM_GETIMESTATUS 0x00D9
#endif

static HWND mkedit(HWND p, DWORD style, int id, const char *text)
{
    return c_ctl(p, "Edit", text, ES_AUTOHSCROLL | style, id, 2, 2, 200, 24);
}
static HWND mkmedit(HWND p, DWORD style, int id, const char *text)
{
    return c_ctl(p, "Edit", text, ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | style, id, 2, 2, 200, 100);
}
static void type_str(HWND e, const char *s) { for (; *s; s++) SA(e, WM_CHAR, (unsigned char)*s, 1); }

/* ---- limits ---- */
TEST(edit_limit_default_30000)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 1, ""), m = mkmedit(p, 0, 2, "");
    EXPECT_EQ(SA(e, EM_GETLIMITTEXT, 0, 0), 30000);
    EXPECT_EQ(SA(m, EM_GETLIMITTEXT, 0, 0), 30000);
    U(DestroyWindow)(p);
}

TEST(edit_limit_set_get)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 1, ""), m = mkmedit(p, 0, 2, "");
    static const unsigned vals[] = { 1, 2, 5, 100, 29999, 30000, 30001, 65535, 65536, 1000000, 0x7FFFFFFE };
    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        EXPECT_EQ(SA(e, EM_SETLIMITTEXT, vals[i], 0), 1);
        CHECKF((DWORD)SA(e, EM_GETLIMITTEXT, 0, 0) == vals[i], "single %u", vals[i]);
        EXPECT_EQ(SA(m, EM_LIMITTEXT, vals[i], 0), 1);
        CHECKF((DWORD)SA(m, EM_GETLIMITTEXT, 0, 0) == vals[i], "multi %u", vals[i]);
    }
    U(DestroyWindow)(p);
}

TEST(edit_limit_zero_and_huge)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 1, ""), m = mkmedit(p, 0, 2, "");
    /* 0 means "maximum": single-line 0x7FFFFFFE, multi-line unlimited (0xFFFFFFFF) */
    SA(e, EM_SETLIMITTEXT, 0, 0);
    EXPECT_EQ((DWORD)SA(e, EM_GETLIMITTEXT, 0, 0), 0x7FFFFFFE);
    SA(m, EM_SETLIMITTEXT, 0, 0);
    EXPECT_EQ((DWORD)SA(m, EM_GETLIMITTEXT, 0, 0), 0xFFFFFFFF);
    /* single-line clamps larger values to 0x7FFFFFFE */
    SA(e, EM_SETLIMITTEXT, 0x7FFFFFFF, 0);
    EXPECT_EQ((DWORD)SA(e, EM_GETLIMITTEXT, 0, 0), 0x7FFFFFFE);
    SA(e, EM_SETLIMITTEXT, 0xFFFFFFF0u, 0);
    EXPECT_EQ((DWORD)SA(e, EM_GETLIMITTEXT, 0, 0), 0x7FFFFFFE);
    SA(m, EM_SETLIMITTEXT, 0x7FFFFFFF, 0);
    EXPECT_EQ((DWORD)SA(m, EM_GETLIMITTEXT, 0, 0), 0x7FFFFFFF);
    U(DestroyWindow)(p);
}

TEST(edit_limit_typing_enforced)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 7, "");
    SA(e, EM_SETLIMITTEXT, 3, 0);
    c_logclear();
    type_str(e, "abc");
    CHECK(c_text_is(e, "abc"));
    EXPECT_EQ(c_ncmd(7, EN_MAXTEXT), 0);
    type_str(e, "d");
    CHECK(c_text_is(e, "abc"));
    EXPECT_EQ(c_ncmd(7, EN_MAXTEXT), 1);
    type_str(e, "ef");
    CHECK(c_text_is(e, "abc"));
    EXPECT_EQ(c_ncmd(7, EN_MAXTEXT), 3);
    /* replacing a selection is allowed at the limit */
    SA(e, EM_SETSEL, 1, 2);
    type_str(e, "X");
    CHECK(c_text_is(e, "aXc"));
    /* backspace works at the limit */
    SA(e, EM_SETSEL, 3, 3);
    SA(e, WM_CHAR, '\b', 1);
    CHECK(c_text_is(e, "aX"));
    type_str(e, "Z");
    CHECK(c_text_is(e, "aXZ"));
    U(DestroyWindow)(p);
}

TEST(edit_limit_lower_than_existing_text)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 7, "");
    SA(e, WM_SETTEXT, 0, "0123456789");
    SA(e, EM_SETLIMITTEXT, 4, 0);
    /* existing text is not truncated */
    CHECK(c_text_is(e, "0123456789"));
    EXPECT_EQ((DWORD)SA(e, EM_GETLIMITTEXT, 0, 0), 4);
    SA(e, EM_SETSEL, 10, 10);
    c_logclear();
    type_str(e, "x");
    CHECK(c_text_is(e, "0123456789"));
    EXPECT_EQ(c_ncmd(7, EN_MAXTEXT), 1);
    U(DestroyWindow)(p);
}

TEST(edit_limit_settext_and_replacesel_overflow)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 7, "");
    SA(e, EM_SETLIMITTEXT, 5, 0);
    SA(e, WM_SETTEXT, 0, "abc");
    c_logclear();
    LRESULT r = SA(e, WM_SETTEXT, 0, "0123456789");
    char b[64]; c_gettext(e, b, sizeof b);
    t_note("WM_SETTEXT beyond limit: ret=%lld text=\"%s\" maxtext=%d errspace=%d", (long long)r, b,
           c_ncmd(7, EN_MAXTEXT), c_ncmd(7, EN_ERRSPACE));
    SA(e, WM_SETTEXT, 0, "ab");
    SA(e, EM_SETSEL, 2, 2);
    c_logclear();
    SA(e, EM_REPLACESEL, FALSE, "123456");
    c_gettext(e, b, sizeof b);
    t_note("EM_REPLACESEL beyond limit: text=\"%s\" maxtext=%d", b, c_ncmd(7, EN_MAXTEXT));
    CHECK(strlen(b) <= 5 || strlen(b) == 8);
    U(DestroyWindow)(p);
}

TEST(edit_limit_large_text)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND m = mkmedit(p, ES_AUTOHSCROLL, 7, "");
    SA(m, EM_SETLIMITTEXT, 200000, 0);
    static char big[60001], out[60002];
    memset(big, 'k', 60000); big[60000] = 0;
    EXPECT_EQ(SA(m, WM_SETTEXT, 0, big), TRUE);
    EXPECT_EQ(SA(m, WM_GETTEXTLENGTH, 0, 0), 60000);
    EXPECT_EQ(SA(m, WM_GETTEXT, 60002, out), 60000);
    CHECK(!strcmp(out, big));
    EXPECT_EQ(SA(m, EM_GETLINECOUNT, 0, 0), 1);
    U(DestroyWindow)(p);
}

/* ---- typed-character styles ---- */
TEST(edit_style_uppercase_lowercase)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND u = mkedit(p, ES_UPPERCASE, 1, ""), l = mkedit(p, ES_LOWERCASE, 2, "");
    type_str(u, "Hello World 123");
    CHECK(c_text_is(u, "HELLO WORLD 123"));
    type_str(l, "Hello World 123");
    CHECK(c_text_is(l, "hello world 123"));
    U(DestroyWindow)(p);
}

TEST(edit_style_number)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND n = mkedit(p, ES_NUMBER, 1, "");
    type_str(n, "12ab-3.4 5");
    CHECK(c_text_is(n, "12345"));
    /* programmatic text is not filtered */
    SA(n, WM_SETTEXT, 0, "abc");
    CHECK(c_text_is(n, "abc"));
    U(DestroyWindow)(p);
}

TEST(edit_style_readonly_create)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, ES_READONLY, 1, "fixed");
    SA(e, EM_SETSEL, 5, 5);
    type_str(e, "abc");
    SA(e, WM_CHAR, '\b', 1);
    CHECK(c_text_is(e, "fixed"));
    SA(e, EM_SETSEL, 0, 1);
    SA(e, WM_KEYDOWN, VK_DELETE, 0);
    CHECK(c_text_is(e, "fixed"));
    /* programmatic changes are allowed */
    EXPECT_EQ(SA(e, WM_SETTEXT, 0, "changed"), TRUE);
    CHECK(c_text_is(e, "changed"));
    U(DestroyWindow)(p);
}

TEST(edit_setreadonly_toggle)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetWindowLongA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 1, "");
    CHECK(!(U(GetWindowLongA)(e, GWL_STYLE) & ES_READONLY));
    EXPECT_EQ(SA(e, EM_SETREADONLY, TRUE, 0), 1);
    CHECK(U(GetWindowLongA)(e, GWL_STYLE) & ES_READONLY);
    type_str(e, "zzz");
    CHECK(c_text_is(e, ""));
    EXPECT_EQ(SA(e, EM_SETREADONLY, FALSE, 0), 1);
    CHECK(!(U(GetWindowLongA)(e, GWL_STYLE) & ES_READONLY));
    type_str(e, "ok");
    CHECK(c_text_is(e, "ok"));
    SA(e, EM_SETREADONLY, 9, 0);   /* any non-zero */
    CHECK(U(GetWindowLongA)(e, GWL_STYLE) & ES_READONLY);
    U(DestroyWindow)(p);
}

TEST(edit_style_alignment_bits)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetWindowLongA));
    HWND p = c_parent(300, 300);
    DWORD al[] = { ES_LEFT, ES_CENTER, ES_RIGHT };
    for (int i = 0; i < 3; i++) {
        HWND e = c_ctl(p, "Edit", "txt", ES_MULTILINE | al[i], 10 + i, 0, 0, 100, 50);
        CHECK((U(GetWindowLongA)(e, GWL_STYLE) & 3) == al[i]);
    }
    U(DestroyWindow)(p);
}

/* ---- password ---- */
TEST(edit_password_char)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetWindowLongA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 1, "secret");
    EXPECT_EQ(SA(e, EM_GETPASSWORDCHAR, 0, 0), 0);
    EXPECT_EQ(SA(e, EM_SETPASSWORDCHAR, '#', 0), 1);
    EXPECT_EQ(SA(e, EM_GETPASSWORDCHAR, 0, 0), '#');
    CHECK(U(GetWindowLongA)(e, GWL_STYLE) & ES_PASSWORD);
    CHECK(c_text_is(e, "secret"));
    EXPECT_EQ(SA(e, WM_GETTEXTLENGTH, 0, 0), 6);
    SA(e, EM_SETPASSWORDCHAR, '*', 0);
    EXPECT_EQ(SA(e, EM_GETPASSWORDCHAR, 0, 0), '*');
    SA(e, EM_SETPASSWORDCHAR, 0, 0);
    EXPECT_EQ(SA(e, EM_GETPASSWORDCHAR, 0, 0), 0);
    CHECK(!(U(GetWindowLongA)(e, GWL_STYLE) & ES_PASSWORD));
    CHECK(c_text_is(e, "secret"));
    U(DestroyWindow)(p);
}

TEST(edit_password_style_default_char)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, ES_PASSWORD, 1, "pw");
    LRESULT c = SA(e, EM_GETPASSWORDCHAR, 0, 0);
    CHECK(c != 0);
    t_note("default ES_PASSWORD char = U+%04llx", (long long)c);
    CHECK(c_text_is(e, "pw"));
    U(DestroyWindow)(p);
}

TEST(edit_password_wide_char)
{
    REQUIRE(U(CreateWindowExW));
    HWND p = c_parent(300, 300);
    HWND e = c_ctlw(p, L"Edit", L"abc", ES_AUTOHSCROLL, 1, 0, 0, 100, 24);
    SWm(e, EM_SETPASSWORDCHAR, 0x25CF, 0);
    EXPECT_EQ(SWm(e, EM_GETPASSWORDCHAR, 0, 0), 0x25CF);
    SWm(e, EM_SETPASSWORDCHAR, 0, 0);
    U(DestroyWindow)(p);
}

/* ---- undo ---- */
TEST(edit_undo_replace_selection)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 1, "");
    SA(e, WM_SETTEXT, 0, "hello");
    SA(e, EM_EMPTYUNDOBUFFER, 0, 0);
    EXPECT_EQ(SA(e, EM_CANUNDO, 0, 0), FALSE);
    SA(e, EM_SETSEL, 0, 5);
    SA(e, EM_REPLACESEL, TRUE, "X");
    CHECK(c_text_is(e, "X"));
    EXPECT_EQ(SA(e, EM_CANUNDO, 0, 0), TRUE);
    EXPECT_EQ(SA(e, EM_UNDO, 0, 0), TRUE);
    CHECK(c_text_is(e, "hello"));
    t_note("after undo: canundo=%lld sel=%d", (long long)SA(e, EM_CANUNDO, 0, 0), 0);
    U(DestroyWindow)(p);
}

TEST(edit_undo_typing_and_delete)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 1, "");
    type_str(e, "abc");
    EXPECT_EQ(SA(e, EM_CANUNDO, 0, 0), TRUE);
    SA(e, WM_UNDO, 0, 0);
    char b[16]; c_gettext(e, b, sizeof b);
    CHECK(strlen(b) < 3);
    t_note("undo of typed 'abc' -> \"%s\"", b);
    SA(e, WM_SETTEXT, 0, "keep");
    SA(e, EM_EMPTYUNDOBUFFER, 0, 0);
    SA(e, EM_SETSEL, 1, 3);
    SA(e, WM_KEYDOWN, VK_DELETE, 0);
    CHECK(c_text_is(e, "kp"));
    EXPECT_EQ(SA(e, EM_CANUNDO, 0, 0), TRUE);
    SA(e, EM_UNDO, 0, 0);
    CHECK(c_text_is(e, "keep"));
    U(DestroyWindow)(p);
}

TEST(edit_undo_emptybuffer)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 1, "");
    type_str(e, "zz");
    EXPECT_EQ(SA(e, EM_CANUNDO, 0, 0), TRUE);
    EXPECT_EQ(SA(e, EM_EMPTYUNDOBUFFER, 0, 0), 1);
    EXPECT_EQ(SA(e, EM_CANUNDO, 0, 0), FALSE);
    SA(e, EM_UNDO, 0, 0);
    CHECK(c_text_is(e, "zz"));
    U(DestroyWindow)(p);
}

TEST(edit_undo_multiline)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND m = mkmedit(p, 0, 1, "");
    SA(m, WM_SETTEXT, 0, "line1\r\nline2");
    SA(m, EM_EMPTYUNDOBUFFER, 0, 0);
    EXPECT_EQ(SA(m, EM_CANUNDO, 0, 0), FALSE);
    SA(m, EM_SETSEL, 0, 5);
    SA(m, EM_REPLACESEL, TRUE, "LINE");
    EXPECT_EQ(SA(m, EM_CANUNDO, 0, 0), TRUE);
    CHECK(c_text_is(m, "LINE\r\nline2"));
    EXPECT_EQ(SA(m, EM_UNDO, 0, 0), TRUE);
    CHECK(c_text_is(m, "line1\r\nline2"));
    U(DestroyWindow)(p);
}

/* ---- notifications ---- */
TEST(edit_notify_update_before_change)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 9, "");
    c_logclear();
    SA(e, EM_REPLACESEL, FALSE, "abc");
    EXPECT_EQ(c_ncmd(9, EN_UPDATE), 1);
    EXPECT_EQ(c_ncmd(9, EN_CHANGE), 1);
    CHECK(c_first_cmd(9, EN_UPDATE) < c_first_cmd(9, EN_CHANGE));
    int i = c_first_cmd(9, EN_CHANGE);
    CHECK(i >= 0 && c_log[i].lp == (LPARAM)e);
    c_logclear();
    SA(e, WM_CHAR, 'x', 1);
    EXPECT_EQ(c_ncmd(9, EN_UPDATE), 1);
    EXPECT_EQ(c_ncmd(9, EN_CHANGE), 1);
    CHECK(c_first_cmd(9, EN_UPDATE) < c_first_cmd(9, EN_CHANGE));
    c_logclear();
    SA(e, EM_SETSEL, 0, 1);
    SA(e, WM_CLEAR, 0, 0);
    EXPECT_EQ(c_ncmd(9, EN_CHANGE), 1);
    c_logclear();
    SA(e, EM_SETSEL, 0, 1);
    EXPECT_EQ(c_nmsg(WM_COMMAND), 0);   /* selection changes are silent */
    SA(e, EM_SETMODIFY, TRUE, 0);
    SA(e, EM_SETLIMITTEXT, 100, 0);
    EXPECT_EQ(c_nmsg(WM_COMMAND), 0);
    U(DestroyWindow)(p);
}

TEST(edit_notify_settext)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 9, "");
    c_logclear();
    SA(e, WM_SETTEXT, 0, "first");
    t_note("WM_SETTEXT notifications: update=%d change=%d", c_ncmd(9, EN_UPDATE), c_ncmd(9, EN_CHANGE));
    c_logclear();
    SA(e, WM_SETTEXT, 0, "");
    t_note("WM_SETTEXT(empty) notifications: update=%d change=%d", c_ncmd(9, EN_UPDATE), c_ncmd(9, EN_CHANGE));
    U(DestroyWindow)(p);
}

TEST(edit_notify_multiline_change)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND m = mkmedit(p, 0, 9, "");
    c_logclear();
    SA(m, EM_REPLACESEL, FALSE, "a\r\nb");
    EXPECT_EQ(c_ncmd(9, EN_UPDATE), 1);
    EXPECT_EQ(c_ncmd(9, EN_CHANGE), 1);
    c_logclear();
    SA(m, WM_CHAR, '\r', 1);
    EXPECT_EQ(c_ncmd(9, EN_CHANGE), 1);
    U(DestroyWindow)(p);
}

/* ---- return values of simple setters (user32 returns fixed values) ---- */
TEST(edit_setter_return_values)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 1, "text"), m = mkmedit(p, 0, 2, "text\r\nmore");
    EXPECT_EQ(SA(e, EM_SETSEL, 0, 2), 1);
    EXPECT_EQ(SA(m, EM_SETSEL, 0, 2), 1);
    EXPECT_EQ(SA(e, EM_REPLACESEL, TRUE, "q"), 1);
    EXPECT_EQ(SA(m, EM_REPLACESEL, TRUE, "q"), 1);
    EXPECT_EQ(SA(e, EM_SETPASSWORDCHAR, 0, 0), 1);
    EXPECT_EQ(SA(e, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(1, 1)), 0);
    EXPECT_EQ(SA(m, EM_SCROLLCARET, 0, 0), 1);
    EXPECT_EQ(SA(m, EM_LINESCROLL, 0, 0), 1);
    EXPECT_EQ(SA(e, EM_EMPTYUNDOBUFFER, 0, 0), 1);
    RECT r;
    EXPECT_EQ(SA(m, EM_GETRECT, 0, &r), 1);
    CHECK(r.right > r.left && r.bottom > r.top);
    EXPECT_EQ(SA(e, EM_SETWORDBREAKPROC, 0, NULL), 1);
    U(DestroyWindow)(p);
}

TEST(edit_wordbreakproc_roundtrip)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 1, "alpha beta");
    HWND m = mkmedit(p, 0, 2, "alpha beta");
    EXPECT_EQ(SA(e, EM_GETWORDBREAKPROC, 0, 0), 0);
    EXPECT_EQ(SA(m, EM_GETWORDBREAKPROC, 0, 0), 0);
    FARPROC fake = (FARPROC)(ULONG_PTR)0x10000;   /* never called: no word-break UI is exercised */
    SA(e, EM_SETWORDBREAKPROC, 0, fake);
    EXPECT_EQ(SA(e, EM_GETWORDBREAKPROC, 0, 0), fake);
    SA(e, EM_SETWORDBREAKPROC, 0, NULL);
    EXPECT_EQ(SA(e, EM_GETWORDBREAKPROC, 0, 0), 0);
    SA(m, EM_SETWORDBREAKPROC, 0, fake);
    EXPECT_EQ(SA(m, EM_GETWORDBREAKPROC, 0, 0), fake);
    SA(m, EM_SETWORDBREAKPROC, 0, NULL);
    U(DestroyWindow)(p);
}

TEST(edit_imestatus_messages)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 1, "x");
    LRESULT r = SA(e, EM_SETIMESTATUS, 1, 0);
    EXPECT_EQ(r, 1);
    r = SA(e, EM_GETIMESTATUS, 1, 0);
    t_note("EM_GETIMESTATUS(EMSIS_COMPOSITIONSTRING) = %lld", (long long)r);
    t_note("EM_GETIMESTATUS(0) = %lld, EM_SETIMESTATUS(0) = %lld", (long long)SA(e, EM_GETIMESTATUS, 0, 0), (long long)SA(e, EM_SETIMESTATUS, 0, 0));
    U(DestroyWindow)(p);
}

TEST(edit_cuebanner_balloon_observed)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 1, "");
    static const WCHAR cue[] = { 'c', 'u', 'e', 0 };
    WCHAR buf[16] = { 0 };
    LRESULT r1 = SWm(e, 0x1501, FALSE, cue);          /* EM_SETCUEBANNER */
    LRESULT r2 = SWm(e, 0x1502, buf, 16);             /* EM_GETCUEBANNER */
    LRESULT r3 = SWm(e, 0x1504, 0, 0);                /* EM_HIDEBALLOONTIP */
    t_note("cue banner (no visual-style comctl): set=%lld get=%lld \"%ls\" hide=%lld", (long long)r1, (long long)r2, buf, (long long)r3);
    CHECK(c_text_is(e, ""));
    U(DestroyWindow)(p);
}

/* ---- scrolling (multi-line) ---- */
static HWND mk_tall(HWND p, int lines)
{
    static char txt[8192];
    int n = 0;
    for (int i = 0; i < lines; i++) n += sprintf(txt + n, i + 1 < lines ? "line %d\r\n" : "line %d", i);
    return c_ctl(p, "Edit", txt, ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL, 1, 0, 0, 200, 100);
}

TEST(edit_linescroll_firstvisible)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND m = mk_tall(p, 60);
    EXPECT_EQ(SA(m, EM_GETLINECOUNT, 0, 0), 60);
    EXPECT_EQ(SA(m, EM_GETFIRSTVISIBLELINE, 0, 0), 0);
    EXPECT_EQ(SA(m, EM_LINESCROLL, 0, 10), 1);
    EXPECT_EQ(SA(m, EM_GETFIRSTVISIBLELINE, 0, 0), 10);
    SA(m, EM_LINESCROLL, 0, -4);
    EXPECT_EQ(SA(m, EM_GETFIRSTVISIBLELINE, 0, 0), 6);
    SA(m, EM_LINESCROLL, 0, 1);
    EXPECT_EQ(SA(m, EM_GETFIRSTVISIBLELINE, 0, 0), 7);
    SA(m, EM_LINESCROLL, 0, -1000);
    EXPECT_EQ(SA(m, EM_GETFIRSTVISIBLELINE, 0, 0), 0);
    SA(m, EM_LINESCROLL, 0, 100000);
    LRESULT fv = SA(m, EM_GETFIRSTVISIBLELINE, 0, 0);
    CHECK(fv > 10 && fv <= 59);
    t_note("LINESCROLL clamp: first visible after +100000 = %lld of 60", (long long)fv);
    U(DestroyWindow)(p);
}

TEST(edit_scroll_codes)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND m = mk_tall(p, 60);
    SA(m, EM_SCROLL, SB_LINEDOWN, 0);
    EXPECT_EQ(SA(m, EM_GETFIRSTVISIBLELINE, 0, 0), 1);
    SA(m, EM_SCROLL, SB_LINEDOWN, 0);
    SA(m, EM_SCROLL, SB_LINEDOWN, 0);
    EXPECT_EQ(SA(m, EM_GETFIRSTVISIBLELINE, 0, 0), 3);
    SA(m, EM_SCROLL, SB_LINEUP, 0);
    EXPECT_EQ(SA(m, EM_GETFIRSTVISIBLELINE, 0, 0), 2);
    SA(m, EM_SCROLL, SB_PAGEDOWN, 0);
    LRESULT a = SA(m, EM_GETFIRSTVISIBLELINE, 0, 0);
    CHECK(a > 3);
    SA(m, EM_SCROLL, SB_PAGEUP, 0);
    EXPECT_EQ(SA(m, EM_GETFIRSTVISIBLELINE, 0, 0), 2);
    SA(m, EM_SCROLL, SB_LINEUP, 0); SA(m, EM_SCROLL, SB_LINEUP, 0); SA(m, EM_SCROLL, SB_LINEUP, 0);
    EXPECT_EQ(SA(m, EM_GETFIRSTVISIBLELINE, 0, 0), 0);
    LRESULT r = SA(m, EM_SCROLL, SB_LINEDOWN, 0);
    CHECK(HIWORD(r) != 0 && LOWORD(r) == 1);
    r = SA(m, EM_SCROLL, SB_LINEUP, 0);
    t_note("EM_SCROLL(SB_LINEUP) at line 1 -> %llx", (long long)r);
    U(DestroyWindow)(p);
}

TEST(edit_scrollcaret)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND m = mk_tall(p, 60);
    SA(m, EM_SETSEL, 0, 0);
    SA(m, EM_SCROLLCARET, 0, 0);
    EXPECT_EQ(SA(m, EM_GETFIRSTVISIBLELINE, 0, 0), 0);
    int len = (int)SA(m, WM_GETTEXTLENGTH, 0, 0);
    SA(m, EM_SETSEL, len, len);
    SA(m, EM_SCROLLCARET, 0, 0);
    LRESULT fv = SA(m, EM_GETFIRSTVISIBLELINE, 0, 0);
    CHECK(fv > 40);
    SA(m, EM_SETSEL, 0, 0);
    SA(m, EM_SCROLLCARET, 0, 0);
    EXPECT_EQ(SA(m, EM_GETFIRSTVISIBLELINE, 0, 0), 0);
    U(DestroyWindow)(p);
}

TEST(edit_wm_vscroll_message)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND m = mk_tall(p, 60);
    SA(m, WM_VSCROLL, SB_LINEDOWN, 0);
    EXPECT_EQ(SA(m, EM_GETFIRSTVISIBLELINE, 0, 0), 1);
    SA(m, WM_VSCROLL, SB_BOTTOM, 0);
    LRESULT b = SA(m, EM_GETFIRSTVISIBLELINE, 0, 0);
    CHECK(b > 40);
    SA(m, WM_VSCROLL, SB_TOP, 0);
    EXPECT_EQ(SA(m, EM_GETFIRSTVISIBLELINE, 0, 0), 0);
    SA(m, WM_VSCROLL, MAKEWPARAM(SB_THUMBPOSITION, 20), 0);
    EXPECT_EQ(SA(m, EM_GETFIRSTVISIBLELINE, 0, 0), 20);
    U(DestroyWindow)(p);
}

TEST(edit_window_scrollbar_range)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetScrollInfo));
    HWND p = c_parent(300, 300);
    HWND m = mk_tall(p, 60);
    SCROLLINFO si; memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_ALL;
    CHECK(U(GetScrollInfo)(m, SB_VERT, &si));
    EXPECT_EQ(si.nMin, 0);
    t_note("edit vscroll: max=%d page=%u pos=%d", si.nMax, si.nPage, si.nPos);
    CHECK(si.nMax >= 59 - 1);
    CHECK(si.nPage > 0);
    SA(m, EM_LINESCROLL, 0, 5);
    memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_POS;
    CHECK(U(GetScrollInfo)(m, SB_VERT, &si));
    EXPECT_EQ(si.nPos, 5);
    U(DestroyWindow)(p);
}

TEST(edit_horizontal_scroll_single_line)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = c_ctl(p, "Edit", "", ES_AUTOHSCROLL, 1, 0, 0, 60, 22);
    for (int i = 0; i < 40; i++) SA(e, WM_CHAR, 'M', 1);
    EXPECT_EQ(SA(e, WM_GETTEXTLENGTH, 0, 0), 40);
    SA(e, EM_SETSEL, 40, 40);
    SA(e, EM_SCROLLCARET, 0, 0);
    LRESULT fv = SA(e, EM_GETFIRSTVISIBLELINE, 0, 0);
    CHECK(fv > 0);   /* horizontal char offset for single-line */
    SA(e, EM_SETSEL, 0, 0);
    SA(e, EM_SCROLLCARET, 0, 0);
    EXPECT_EQ(SA(e, EM_GETFIRSTVISIBLELINE, 0, 0), 0);
    U(DestroyWindow)(p);
}

/* ---- rect / tabs / handle ---- */
TEST(edit_getrect_setrect)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND m = mkmedit(p, 0, 1, "abc");
    RECT r0, r1, cl;
    memset(&r0, 0, sizeof r0);
    EXPECT_EQ(SA(m, EM_GETRECT, 0, &r0), 1);
    U(GetClientRect)(m, &cl);
    CHECK(r0.left >= 0 && r0.top >= 0 && r0.right <= cl.right && r0.bottom <= cl.bottom);
    RECT set = { 6, 5, 150, 80 };
    SA(m, EM_SETRECT, 0, &set);
    memset(&r1, 0, sizeof r1);
    SA(m, EM_GETRECT, 0, &r1);
    t_note("SETRECT {6,5,150,80} -> {%ld,%ld,%ld,%ld}", r1.left, r1.top, r1.right, r1.bottom);
    CHECK(r1.left == 6 && r1.right == 150);
    SA(m, EM_SETRECTNP, 0, &set);
    SA(m, EM_GETRECT, 0, &r1);
    CHECK(r1.left == 6 && r1.right == 150);
    U(DestroyWindow)(p);
}

TEST(edit_settabstops)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND m = mkmedit(p, 0, 1, "a\tb\tc");
    int one = 16, many[3] = { 8, 32, 64 };
    LRESULT r0 = SA(m, EM_SETTABSTOPS, 0, 0);
    LRESULT r1 = SA(m, EM_SETTABSTOPS, 1, &one);
    LRESULT r3 = SA(m, EM_SETTABSTOPS, 3, many);
    t_note("EM_SETTABSTOPS returns %lld %lld %lld", (long long)r0, (long long)r1, (long long)r3);
    CHECK(r0 != 0 && r1 != 0 && r3 != 0);
    /* positions: with tab stop set to 64 dialog units the 2nd char moves right */
    SA(m, EM_SETTABSTOPS, 1, &one);
    int x_small = (short)LOWORD(SA(m, EM_POSFROMCHAR, 2, 0));
    int big = 100;
    SA(m, EM_SETTABSTOPS, 1, &big);
    int x_big = (short)LOWORD(SA(m, EM_POSFROMCHAR, 2, 0));
    CHECK(x_big > x_small);
    SA(m, EM_SETTABSTOPS, 0, 0);
    CHECK(c_text_is(m, "a\tb\tc"));
    U(DestroyWindow)(p);
}

TEST(edit_gethandle_sethandle)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND m = mkmedit(p, 0, 1, "alpha\r\nbeta");
    HLOCAL h = (HLOCAL)SA(m, EM_GETHANDLE, 0, 0);
    CHECK(h != NULL);
    if (h) {
        const char *s = LocalLock(h);
        if (s) { CHECKF(!strncmp(s, "alpha\r\nbeta", 11), "buffer \"%.12s\"", s); LocalUnlock(h); }
        else CHECK(0);
    }
    HLOCAL nh = LocalAlloc(LMEM_MOVEABLE | LMEM_ZEROINIT, 64);
    char *d = LocalLock(nh);
    if (d) { strcpy(d, "one\r\ntwo\r\nthree"); LocalUnlock(nh); }
    EXPECT_EQ(SA(m, EM_SETHANDLE, nh, 0), 1);
    CHECK(c_text_is(m, "one\r\ntwo\r\nthree"));
    EXPECT_EQ(SA(m, EM_GETLINECOUNT, 0, 0), 3);
    EXPECT_EQ(SA(m, WM_GETTEXTLENGTH, 0, 0), 15);
    EXPECT_EQ((HLOCAL)SA(m, EM_GETHANDLE, 0, 0), nh);
    U(DestroyWindow)(p);
}

/* ---- clipboard ---- */
static BOOL clip_text_only_state(HWND owner, WCHAR **saved)
{
    *saved = NULL;
    if (!U(OpenClipboard)(owner)) return FALSE;
    UINT f = 0; BOOL other = FALSE;
    while ((f = U(EnumClipboardFormats)(f)) != 0)
        if (f != CF_TEXT && f != CF_OEMTEXT && f != CF_UNICODETEXT && f != CF_LOCALE) other = TRUE;
    if (!other) {
        HANDLE h = U(GetClipboardData)(CF_UNICODETEXT);
        if (h) {
            WCHAR *s = GlobalLock(h);
            if (s) { *saved = (WCHAR *)LocalAlloc(LPTR, (lstrlenW(s) + 1) * 2); lstrcpyW(*saved, s); GlobalUnlock(h); }
        }
    }
    U(CloseClipboard)();
    return !other;
}
static void clip_set(HWND owner, const WCHAR *txt)
{
    if (!U(OpenClipboard)(owner)) return;
    U(EmptyClipboard)();
    if (txt) {
        HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (lstrlenW(txt) + 1) * 2);
        WCHAR *d = GlobalLock(g); lstrcpyW(d, txt); GlobalUnlock(g);
        if (!U(SetClipboardData)(CF_UNICODETEXT, g)) GlobalFree(g);
    }
    U(CloseClipboard)();
}
static int clip_get(HWND owner, WCHAR *out, int n)
{
    out[0] = 0;
    if (!U(OpenClipboard)(owner)) return 0;
    HANDLE h = U(GetClipboardData)(CF_UNICODETEXT);
    int ok = 0;
    if (h) { WCHAR *s = GlobalLock(h); if (s) { lstrcpynW(out, s, n); ok = 1; GlobalUnlock(h); } }
    U(CloseClipboard)();
    return ok;
}

TEST(edit_clipboard_copy_cut_paste_clear)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(OpenClipboard)); REQUIRE(U(CloseClipboard));
    REQUIRE(U(EmptyClipboard)); REQUIRE(U(SetClipboardData)); REQUIRE(U(GetClipboardData));
    REQUIRE(U(EnumClipboardFormats));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 1, "");
    WCHAR *saved; WCHAR got[64];
    if (!clip_text_only_state(p, &saved)) { t_skip("clipboard busy or holds non-text data"); U(DestroyWindow)(p); return; }
    SA(e, WM_SETTEXT, 0, "hello world");
    /* copy */
    clip_set(p, L"sentinel");
    SA(e, EM_SETSEL, 0, 5);
    SA(e, WM_COPY, 0, 0);
    CHECK(clip_get(p, got, 64));
    CHECK(!lstrcmpW(got, L"hello"));
    CHECK(c_text_is(e, "hello world"));
    /* copy with an empty selection leaves the clipboard unchanged */
    SA(e, EM_SETSEL, 3, 3);
    clip_set(p, L"keepme");
    SA(e, WM_COPY, 0, 0);
    clip_get(p, got, 64);
    CHECK(!lstrcmpW(got, L"keepme"));
    /* cut */
    SA(e, EM_SETSEL, 6, 11);
    SA(e, WM_CUT, 0, 0);
    clip_get(p, got, 64);
    CHECK(!lstrcmpW(got, L"world"));
    CHECK(c_text_is(e, "hello "));
    CHECK(c_sel_is(e, 6, 6));
    /* paste replaces the selection */
    clip_set(p, L"PASTED");
    SA(e, EM_SETSEL, 0, 5);
    SA(e, WM_PASTE, 0, 0);
    CHECK(c_text_is(e, "PASTED "));
    CHECK(c_sel_is(e, 6, 6));
    /* clear deletes the selection without touching the clipboard */
    SA(e, EM_SETSEL, 0, 6);
    SA(e, WM_CLEAR, 0, 0);
    CHECK(c_text_is(e, " "));
    clip_get(p, got, 64);
    CHECK(!lstrcmpW(got, L"PASTED"));
    /* paste of empty clipboard changes nothing */
    clip_set(p, NULL);
    SA(e, WM_PASTE, 0, 0);
    CHECK(c_text_is(e, " "));
    /* paste honours the limit */
    clip_set(p, L"123456789");
    SA(e, WM_SETTEXT, 0, "");
    SA(e, EM_SETLIMITTEXT, 4, 0);
    SA(e, WM_PASTE, 0, 0);
    char b[32]; c_gettext(e, b, sizeof b);
    t_note("paste of 9 chars with limit 4 -> \"%s\"", b);
    CHECK(strlen(b) <= 4);
    /* restore */
    clip_set(p, saved);
    if (saved) LocalFree(saved);
    U(DestroyWindow)(p);
}

TEST(edit_clipboard_readonly_and_password)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(OpenClipboard)); REQUIRE(U(EmptyClipboard));
    REQUIRE(U(SetClipboardData)); REQUIRE(U(GetClipboardData)); REQUIRE(U(CloseClipboard)); REQUIRE(U(EnumClipboardFormats));
    HWND p = c_parent(300, 300);
    HWND ro = mkedit(p, ES_READONLY, 1, "readonly text");
    HWND pw = mkedit(p, ES_PASSWORD, 2, "passwd");
    WCHAR *saved; WCHAR got[64];
    if (!clip_text_only_state(p, &saved)) { t_skip("clipboard busy or holds non-text data"); U(DestroyWindow)(p); return; }
    clip_set(p, L"orig");
    SA(ro, EM_SETSEL, 0, 8);
    SA(ro, WM_CUT, 0, 0);
    CHECK(c_text_is(ro, "readonly text"));
    SA(ro, EM_SETSEL, 0, 8);
    SA(ro, WM_COPY, 0, 0);
    clip_get(p, got, 64);
    CHECK(!lstrcmpW(got, L"readonly"));   /* copy from a read-only control works */
    clip_set(p, L"pasteme");
    SA(ro, WM_PASTE, 0, 0);
    CHECK(c_text_is(ro, "readonly text"));
    /* password control: copy/cut are suppressed by the edit control */
    clip_set(p, L"orig");
    SA(pw, EM_SETSEL, 0, 6);
    SA(pw, WM_COPY, 0, 0);
    clip_get(p, got, 64);
    t_note("clipboard after WM_COPY on password edit: \"%ls\"", got);
    CHECK(lstrcmpW(got, L"passwd") != 0);
    clip_set(p, saved);
    if (saved) LocalFree(saved);
    U(DestroyWindow)(p);
}

/* ---- Unicode typing / misc ---- */
TEST(edit_wide_char_typing)
{
    REQUIRE(U(CreateWindowExW));
    HWND p = c_parent(300, 300);
    HWND e = c_ctlw(p, L"Edit", L"", ES_AUTOHSCROLL, 1, 0, 0, 100, 24);
    SWm(e, WM_CHAR, 0x4E2D, 1);
    SWm(e, WM_CHAR, 0x0416, 1);
    SWm(e, WM_CHAR, 'a', 1);
    WCHAR w[8] = { 0 };
    EXPECT_EQ(SWm(e, WM_GETTEXT, 8, w), 3);
    CHECK(w[0] == 0x4E2D && w[1] == 0x0416 && w[2] == 'a');
    EXPECT_EQ(SWm(e, WM_GETTEXTLENGTH, 0, 0), 3);
    U(DestroyWindow)(p);
}

TEST(edit_settext_null_clears)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 1, "something");
    SA(e, WM_SETTEXT, 0, NULL);
    EXPECT_EQ(SA(e, WM_GETTEXTLENGTH, 0, 0), 0);
    U(DestroyWindow)(p);
}

TEST(edit_nohidesel_style_roundtrip)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetWindowLongA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, ES_NOHIDESEL | ES_OEMCONVERT, 1, "x");
    LONG st = U(GetWindowLongA)(e, GWL_STYLE);
    CHECK(st & ES_NOHIDESEL);
    CHECK(st & ES_OEMCONVERT);
    CHECK(st & ES_AUTOHSCROLL);
    U(DestroyWindow)(p);
}

TEST(edit_wm_gettext_wm_getfont_misc)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 1, "abc");
    EXPECT_EQ(SA(e, WM_GETTEXTLENGTH, 0, 0), 3);
    EXPECT_EQ(SA(e, EM_GETLIMITTEXT, 0, 0), 30000);
    CHECK(c_text_is(e, "abc"));
    U(DestroyWindow)(p);
}

TEST(edit_double_destroy_and_stale_handle)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(IsWindow));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 1, "abc");
    CHECK(U(DestroyWindow)(e));
    SetLastError(0);
    CHECK(!U(DestroyWindow)(e));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    CHECK(!U(IsWindow)(e));
    U(DestroyWindow)(p);
}

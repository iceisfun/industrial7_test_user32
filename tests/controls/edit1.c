#include "ctl.h"

#ifndef EM_SETIMESTATUS
#define EM_SETIMESTATUS 0x00D8
#define EM_GETIMESTATUS 0x00D9
#endif
#define XEM_SETCUEBANNER 0x1501
#define XEM_GETCUEBANNER 0x1502
#define XEM_SHOWBALLOONTIP 0x1503
#define XEM_HIDEBALLOONTIP 0x1504

static HWND mkedit(HWND p, DWORD style, int id, const char *text)
{
    return c_ctl(p, "Edit", text, ES_AUTOHSCROLL | style, id, 2, 2, 200, 24);
}
static HWND mkmedit(HWND p, DWORD style, int id, const char *text)
{
    return c_ctl(p, "Edit", text, ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | style, id, 2, 2, 200, 100);
}

TEST(edit_classname_and_style)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetWindowLongA));
    HWND p = c_parent(300, 200);
    HWND e = mkedit(p, 0, 1, "abc");
    char n[16];
    CHECK(e != NULL);
    c_classname(e, n, sizeof n);
    EXPECT_STR(n, "Edit");
    CHECK(!(U(GetWindowLongA)(e, GWL_STYLE) & ES_MULTILINE));
    HWND m = mkmedit(p, 0, 2, "abc");
    CHECK(U(GetWindowLongA)(m, GWL_STYLE) & ES_MULTILINE);
    U(DestroyWindow)(p);
}

TEST(edit_constants)
{
    EXPECT_EQ(EM_GETSEL, 0xB0); EXPECT_EQ(EM_SETSEL, 0xB1); EXPECT_EQ(EM_GETRECT, 0xB2);
    EXPECT_EQ(EM_SETRECT, 0xB3); EXPECT_EQ(EM_SETRECTNP, 0xB4); EXPECT_EQ(EM_SCROLL, 0xB5);
    EXPECT_EQ(EM_LINESCROLL, 0xB6); EXPECT_EQ(EM_SCROLLCARET, 0xB7); EXPECT_EQ(EM_GETMODIFY, 0xB8);
    EXPECT_EQ(EM_SETMODIFY, 0xB9); EXPECT_EQ(EM_GETLINECOUNT, 0xBA); EXPECT_EQ(EM_LINEINDEX, 0xBB);
    EXPECT_EQ(EM_SETHANDLE, 0xBC); EXPECT_EQ(EM_GETHANDLE, 0xBD); EXPECT_EQ(EM_GETTHUMB, 0xBE);
    EXPECT_EQ(EM_LINELENGTH, 0xC1); EXPECT_EQ(EM_REPLACESEL, 0xC2); EXPECT_EQ(EM_GETLINE, 0xC4);
    EXPECT_EQ(EM_LIMITTEXT, 0xC5); EXPECT_EQ(EM_CANUNDO, 0xC6); EXPECT_EQ(EM_UNDO, 0xC7);
    EXPECT_EQ(EM_FMTLINES, 0xC8); EXPECT_EQ(EM_LINEFROMCHAR, 0xC9); EXPECT_EQ(EM_SETTABSTOPS, 0xCB);
    EXPECT_EQ(EM_SETPASSWORDCHAR, 0xCC); EXPECT_EQ(EM_EMPTYUNDOBUFFER, 0xCD);
    EXPECT_EQ(EM_GETFIRSTVISIBLELINE, 0xCE); EXPECT_EQ(EM_SETREADONLY, 0xCF);
    EXPECT_EQ(EM_SETWORDBREAKPROC, 0xD0); EXPECT_EQ(EM_GETWORDBREAKPROC, 0xD1);
    EXPECT_EQ(EM_GETPASSWORDCHAR, 0xD2); EXPECT_EQ(EM_SETMARGINS, 0xD3); EXPECT_EQ(EM_GETMARGINS, 0xD4);
    EXPECT_EQ(EM_SETLIMITTEXT, EM_LIMITTEXT); EXPECT_EQ(EM_GETLIMITTEXT, 0xD5);
    EXPECT_EQ(EM_POSFROMCHAR, 0xD6); EXPECT_EQ(EM_CHARFROMPOS, 0xD7);
    EXPECT_EQ(EC_LEFTMARGIN, 1); EXPECT_EQ(EC_RIGHTMARGIN, 2); EXPECT_EQ(EC_USEFONTINFO, 0xFFFF);
    EXPECT_EQ(ES_LEFT, 0); EXPECT_EQ(ES_CENTER, 1); EXPECT_EQ(ES_RIGHT, 2); EXPECT_EQ(ES_MULTILINE, 4);
    EXPECT_EQ(ES_UPPERCASE, 8); EXPECT_EQ(ES_LOWERCASE, 0x10); EXPECT_EQ(ES_PASSWORD, 0x20);
    EXPECT_EQ(ES_AUTOVSCROLL, 0x40); EXPECT_EQ(ES_AUTOHSCROLL, 0x80); EXPECT_EQ(ES_NOHIDESEL, 0x100);
    EXPECT_EQ(ES_OEMCONVERT, 0x400); EXPECT_EQ(ES_READONLY, 0x800); EXPECT_EQ(ES_WANTRETURN, 0x1000);
    EXPECT_EQ(ES_NUMBER, 0x2000);
    EXPECT_EQ(EN_SETFOCUS, 0x100); EXPECT_EQ(EN_KILLFOCUS, 0x200); EXPECT_EQ(EN_CHANGE, 0x300);
    EXPECT_EQ(EN_UPDATE, 0x400); EXPECT_EQ(EN_ERRSPACE, 0x500); EXPECT_EQ(EN_MAXTEXT, 0x501);
    EXPECT_EQ(EN_HSCROLL, 0x601); EXPECT_EQ(EN_VSCROLL, 0x602);
}

TEST(edit_text_basic)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND e = mkedit(p, 0, 1, "Hello");
    char b[64];
    EXPECT_EQ(SA(e, WM_GETTEXTLENGTH, 0, 0), 5);
    EXPECT_EQ(SA(e, WM_GETTEXT, 64, b), 5);
    EXPECT_STR(b, "Hello");
    EXPECT_EQ(SA(e, WM_SETTEXT, 0, "Some other text"), TRUE);
    EXPECT_EQ(SA(e, WM_GETTEXTLENGTH, 0, 0), 15);
    memset(b, 'x', sizeof b);
    EXPECT_EQ(SA(e, WM_GETTEXT, 16, b), 15);
    EXPECT_STR(b, "Some other text");
    memset(b, 'x', sizeof b);
    EXPECT_EQ(SA(e, WM_GETTEXT, 15, b), 14);
    EXPECT_STR(b, "Some other tex");
    memset(b, 'x', sizeof b);
    EXPECT_EQ(SA(e, WM_GETTEXT, 2, b), 1);
    EXPECT_STR(b, "S");
    memset(b, 'x', sizeof b);
    EXPECT_EQ(SA(e, WM_GETTEXT, 1, b), 0);
    EXPECT_EQ(b[0], 0);
    memset(b, 'x', sizeof b);
    EXPECT_EQ(SA(e, WM_GETTEXT, 0, b), 0);
    EXPECT_EQ(b[0], 'x');
    EXPECT_EQ(SA(e, WM_SETTEXT, 0, ""), TRUE);
    EXPECT_EQ(SA(e, WM_GETTEXTLENGTH, 0, 0), 0);
    EXPECT_EQ(SA(e, WM_GETTEXT, 64, b), 0);
    EXPECT_EQ(b[0], 0);
    U(DestroyWindow)(p);
}

TEST(edit_text_wide_unicode)
{
    REQUIRE(U(CreateWindowExW));
    HWND p = c_parent(300, 200);
    static const WCHAR u[] = { 'a', 0x0416, 0x4E2D, 0x20AC, 0xD83D, 0xDE00, 'z', 0 };
    HWND e = c_ctlw(p, L"Edit", L"", ES_AUTOHSCROLL, 2, 0, 0, 200, 24);
    WCHAR w[32]; memset(w, 0, sizeof w);
    CHECK(U(IsWindowUnicode)(e));
    EXPECT_EQ(SWm(e, WM_SETTEXT, 0, u), TRUE);
    EXPECT_EQ(SWm(e, WM_GETTEXTLENGTH, 0, 0), 7);
    EXPECT_EQ(SWm(e, WM_GETTEXT, 32, w), 7);
    CHECK(!lstrcmpW(w, u));
    /* wide limits */
    memset(w, 'x', sizeof w);
    EXPECT_EQ(SWm(e, WM_GETTEXT, 3, w), 2);
    CHECK(w[0] == 'a' && w[1] == 0x0416 && w[2] == 0);
    /* selection counts UTF-16 units */
    SA(e, EM_SETSEL, 0, -1);
    CHECK(c_sel_is(e, 0, 7));
    EXPECT_EQ(SWm(e, EM_LINELENGTH, 0, 0), 7);
    /* wide EM_REPLACESEL */
    SWm(e, EM_SETSEL, 1, 3);
    SWm(e, EM_REPLACESEL, FALSE, L"__");
    memset(w, 0, sizeof w);
    SWm(e, WM_GETTEXT, 32, w);
    CHECK(w[0] == 'a' && w[1] == '_' && w[2] == '_' && w[3] == 0x20AC);
    EXPECT_EQ(SWm(e, WM_GETTEXTLENGTH, 0, 0), 7);
    U(DestroyWindow)(p);
}

TEST(edit_ansi_window_text_parity)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetWindowTextW)); REQUIRE(U(SetWindowTextW));
    HWND p = c_parent(300, 200);
    HWND e = mkedit(p, 0, 1, "ascii only");
    WCHAR w[32]; char a[32];
    CHECK(!U(IsWindowUnicode)(e));
    EXPECT_EQ(U(GetWindowTextW)(e, w, 32), 10);
    CHECK(!lstrcmpW(w, L"ascii only"));
    CHECK(U(SetWindowTextW)(e, L"from wide"));
    EXPECT_EQ(SA(e, WM_GETTEXT, 32, a), 9);
    EXPECT_STR(a, "from wide");
    EXPECT_EQ(SWm(e, WM_GETTEXTLENGTH, 0, 0), 9);
    U(DestroyWindow)(p);
}

TEST(edit_getsel_default_and_setsel_table)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND e = mkedit(p, 0, 1, "0123456789");
    CHECK(c_sel_is(e, 0, 0));
    static const struct { int s, e, rs, re; } t[] = {
        { 0, 0, 0, 0 }, { 2, 5, 2, 5 }, { 0, -1, 0, 10 }, { 5, 100, 5, 10 }, { 100, 200, 10, 10 },
        { 3, 3, 3, 3 }, { 0, 10, 0, 10 }, { 9, 10, 9, 10 }, { 10, 10, 10, 10 }, { 0, 1, 0, 1 },
        { 7, 7, 7, 7 }, { 1, 9, 1, 9 }, { 0, 0x7FFFFFFF, 0, 10 }, { 4, 11, 4, 10 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        SA(e, EM_SETSEL, t[i].s, t[i].e);
        int a, b; c_getsel(e, &a, &b);
        CHECKF(a == t[i].rs && b == t[i].re, "SETSEL(%d,%d) -> %d,%d want %d,%d", t[i].s, t[i].e, a, b, t[i].rs, t[i].re);
        /* GETSEL return packs start/end */
        LRESULT r = SA(e, EM_GETSEL, 0, 0);
        CHECKF(LOWORD(r) == (WORD)t[i].rs && HIWORD(r) == (WORD)t[i].re, "ret %llx", (long long)r);
    }
    U(DestroyWindow)(p);
}

TEST(edit_setsel_minus_one_removes_selection)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND e = mkedit(p, 0, 1, "0123456789");
    SA(e, EM_SETSEL, 2, 6);
    SA(e, EM_SETSEL, -1, 0);
    int a, b; c_getsel(e, &a, &b);
    CHECK(a == b);
    t_note("after SETSEL(-1,0): caret %d,%d (prev sel 2,6)", a, b);
    SA(e, EM_SETSEL, 2, 6);
    SA(e, EM_SETSEL, -1, -1);
    c_getsel(e, &a, &b);
    CHECK(a == b);
    SA(e, EM_SETSEL, 6, 2);
    c_getsel(e, &a, &b);
    t_note("SETSEL(6,2) -> %d,%d", a, b);
    CHECK(a <= b);
    U(DestroyWindow)(p);
}

TEST(edit_getsel_null_pointers)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND e = mkedit(p, 0, 1, "abcdef");
    SA(e, EM_SETSEL, 1, 4);
    DWORD s = 0xdeadbeef, en = 0xdeadbeef;
    EXPECT_EQ(LOWORD(SA(e, EM_GETSEL, &s, 0)), 1);
    EXPECT_EQ(s, 1);
    EXPECT_EQ(HIWORD(SA(e, EM_GETSEL, 0, &en)), 4);
    EXPECT_EQ(en, 4);
    LRESULT r = SA(e, EM_GETSEL, 0, 0);
    EXPECT_EQ(LOWORD(r), 1); EXPECT_EQ(HIWORD(r), 4);
    U(DestroyWindow)(p);
}

TEST(edit_replacesel_basic)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND e = mkedit(p, 0, 1, "hello world");
    SA(e, EM_SETSEL, 0, 5);
    SA(e, EM_REPLACESEL, FALSE, "HELLO!!");
    CHECK(c_text_is(e, "HELLO!! world"));
    CHECK(c_sel_is(e, 7, 7));
    /* no selection: insert at caret */
    SA(e, EM_REPLACESEL, FALSE, "<>");
    CHECK(c_text_is(e, "HELLO!!<> world"));
    CHECK(c_sel_is(e, 9, 9));
    /* empty string deletes selection */
    SA(e, EM_SETSEL, 7, 9);
    SA(e, EM_REPLACESEL, FALSE, "");
    CHECK(c_text_is(e, "HELLO!! world"));
    CHECK(c_sel_is(e, 7, 7));
    /* insert at start and end */
    SA(e, EM_SETSEL, 0, 0); SA(e, EM_REPLACESEL, FALSE, ">");
    CHECK(c_text_is(e, ">HELLO!! world"));
    SA(e, EM_SETSEL, -1, -1); SA(e, EM_SETSEL, 0, -1); SA(e, EM_SETSEL, 14, 14);
    SA(e, EM_REPLACESEL, FALSE, "<");
    CHECK(c_text_is(e, ">HELLO!! world<"));
    /* replace everything */
    SA(e, EM_SETSEL, 0, -1);
    SA(e, EM_REPLACESEL, FALSE, "x");
    CHECK(c_text_is(e, "x"));
    CHECK(c_sel_is(e, 1, 1));
    U(DestroyWindow)(p);
}

TEST(edit_replacesel_sets_modify_and_undo)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND e = mkedit(p, 0, 1, "");
    SA(e, WM_SETTEXT, 0, "abc");
    SA(e, EM_SETMODIFY, FALSE, 0);
    SA(e, EM_EMPTYUNDOBUFFER, 0, 0);
    EXPECT_EQ(SA(e, EM_GETMODIFY, 0, 0), FALSE);
    EXPECT_EQ(SA(e, EM_CANUNDO, 0, 0), FALSE);
    SA(e, EM_SETSEL, 3, 3);
    SA(e, EM_REPLACESEL, FALSE, "d");
    EXPECT_EQ(SA(e, EM_GETMODIFY, 0, 0), TRUE);
    EXPECT_EQ(SA(e, EM_CANUNDO, 0, 0), FALSE);   /* canUndo=FALSE: not undoable */
    SA(e, EM_REPLACESEL, TRUE, "e");
    EXPECT_EQ(SA(e, EM_CANUNDO, 0, 0), TRUE);
    EXPECT_EQ(SA(e, EM_UNDO, 0, 0), TRUE);
    CHECK(c_text_is(e, "abcd"));
    U(DestroyWindow)(p);
}

TEST(edit_modify_flag)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND e = mkedit(p, 0, 1, "");
    EXPECT_EQ(SA(e, EM_GETMODIFY, 0, 0), FALSE);   /* cleared when the control is created */
    HWND e2 = mkedit(p, 0, 2, "init");
    t_note("GETMODIFY after create with initial text: %lld", (long long)SA(e2, EM_GETMODIFY, 0, 0));
    SA(e, WM_SETTEXT, 0, "init");
    SA(e, EM_SETMODIFY, FALSE, 0);
    EXPECT_EQ(SA(e, EM_SETMODIFY, TRUE, 0), 1);
    EXPECT_EQ(SA(e, EM_GETMODIFY, 0, 0), TRUE);
    SA(e, EM_SETMODIFY, FALSE, 0);
    EXPECT_EQ(SA(e, EM_GETMODIFY, 0, 0), FALSE);
    SA(e, EM_SETMODIFY, 7, 0);
    CHECK(SA(e, EM_GETMODIFY, 0, 0) != 0);
    SA(e, EM_SETMODIFY, 0, 0);
    SA(e, WM_SETTEXT, 0, "other");
    t_note("GETMODIFY after WM_SETTEXT: %lld", (long long)SA(e, EM_GETMODIFY, 0, 0));
    SA(e, EM_SETMODIFY, FALSE, 0);
    /* typing sets it */
    SA(e, WM_CHAR, 'x', 1);
    EXPECT_EQ(SA(e, EM_GETMODIFY, 0, 0), TRUE);
    U(DestroyWindow)(p);
}

/* Reference multi-line document used by the line tests. */
static const char k_doc[] = "a\r\nbb\r\nccc\r\n\r\nlast";
/* line start indices: 0,3,7,12,14 ; lengths: 1,2,3,0,4 ; total 18 */
static const int k_doc_idx[5] = { 0, 3, 7, 12, 14 };
static const int k_doc_len[5] = { 1, 2, 3, 0, 4 };

TEST(edit_multiline_linecount)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    static const struct { const char *t; int n; } t[] = {
        { "", 1 }, { "x", 1 }, { "x\r\n", 2 }, { "\r\n", 2 }, { "\r\n\r\n", 3 }, { "a\r\nb", 2 },
        { "a\r\nb\r\n", 3 }, { "a\r\n\r\nb", 3 }, { k_doc, 5 }, { "one\r\ntwo\r\nthree\r\nfour\r\nfive\r\nsix\r\nseven", 7 },
    };
    HWND e = mkmedit(p, 0, 1, "");
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        SA(e, WM_SETTEXT, 0, t[i].t);
        LRESULT n = SA(e, EM_GETLINECOUNT, 0, 0);
        CHECKF(n == t[i].n, "text #%u count %lld want %d", i, (long long)n, t[i].n);
    }
    U(DestroyWindow)(p);
}

TEST(edit_multiline_lineindex_table)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkmedit(p, 0, 1, k_doc);
    EXPECT_EQ(SA(e, WM_GETTEXTLENGTH, 0, 0), 18);
    for (int i = 0; i < 5; i++) {
        LRESULT r = SA(e, EM_LINEINDEX, i, 0);
        CHECKF(r == k_doc_idx[i], "line %d idx %lld want %d", i, (long long)r, k_doc_idx[i]);
    }
    SetLastError(0);
    EXPECT_EQ(SA(e, EM_LINEINDEX, 5, 0), -1);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    EXPECT_EQ(SA(e, EM_LINEINDEX, 6, 0), -1);
    EXPECT_EQ(SA(e, EM_LINEINDEX, 1000, 0), -1);
    /* -1: line of the caret */
    SA(e, EM_SETSEL, 8, 8);
    EXPECT_EQ(SA(e, EM_LINEINDEX, -1, 0), 7);
    SA(e, EM_SETSEL, 0, 0);
    EXPECT_EQ(SA(e, EM_LINEINDEX, -1, 0), 0);
    SA(e, EM_SETSEL, 18, 18);
    EXPECT_EQ(SA(e, EM_LINEINDEX, -1, 0), 14);
    SA(e, EM_SETSEL, 12, 12);
    EXPECT_EQ(SA(e, EM_LINEINDEX, -1, 0), 12);
    U(DestroyWindow)(p);
}

TEST(edit_multiline_linelength_table)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkmedit(p, 0, 1, k_doc);
    /* char index -> line length of the line containing it */
    static const struct { int ich, len; } t[] = {
        { 0, 1 }, { 1, 1 }, { 3, 2 }, { 4, 2 }, { 5, 2 }, { 7, 3 }, { 9, 3 }, { 10, 3 }, { 12, 0 }, { 13, 0 },
        { 14, 4 }, { 16, 4 }, { 17, 4 }, { 18, 4 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        LRESULT r = SA(e, EM_LINELENGTH, t[i].ich, 0);
        CHECKF(r == t[i].len, "LINELENGTH(%d) = %lld want %d", t[i].ich, (long long)r, t[i].len);
    }
    /* beyond the text: zero */
    /* (documentation says zero; the user32 code maps an out-of-range index to the last line) */
    EXPECT_EQ(SA(e, EM_LINELENGTH, 100, 0), 4);
    EXPECT_EQ(SA(e, EM_LINELENGTH, 5000, 0), 4);
    /* every line via LINEINDEX */
    for (int i = 0; i < 5; i++) {
        LRESULT idx = SA(e, EM_LINEINDEX, i, 0);
        EXPECT_EQ(SA(e, EM_LINELENGTH, idx, 0), k_doc_len[i]);
    }
    /* -1 : unselected characters on the lines containing the selection */
    SA(e, EM_SETSEL, 4, 4);
    EXPECT_EQ(SA(e, EM_LINELENGTH, -1, 0), 2);
    SA(e, EM_SETSEL, 3, 4);
    EXPECT_EQ(SA(e, EM_LINELENGTH, -1, 0), 1);
    SA(e, EM_SETSEL, 7, 10);
    EXPECT_EQ(SA(e, EM_LINELENGTH, -1, 0), 0);
    SA(e, EM_SETSEL, 12, 12);
    EXPECT_EQ(SA(e, EM_LINELENGTH, -1, 0), 0);
    U(DestroyWindow)(p);
}

TEST(edit_multiline_linefromchar_table)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkmedit(p, 0, 1, k_doc);
    static const struct { int ich, line; } t[] = {
        { 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 1 }, { 4, 1 }, { 5, 1 }, { 6, 1 }, { 7, 2 }, { 9, 2 }, { 11, 2 },
        { 12, 3 }, { 13, 3 }, { 14, 4 }, { 17, 4 }, { 18, 4 }, { 100, 4 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        LRESULT r = SA(e, EM_LINEFROMCHAR, t[i].ich, 0);
        CHECKF(r == t[i].line, "LINEFROMCHAR(%d) = %lld want %d", t[i].ich, (long long)r, t[i].line);
    }
    SA(e, EM_SETSEL, 8, 10);
    EXPECT_EQ(SA(e, EM_LINEFROMCHAR, -1, 0), 2);
    SA(e, EM_SETSEL, 14, 16);
    EXPECT_EQ(SA(e, EM_LINEFROMCHAR, -1, 0), 4);
    SA(e, EM_SETSEL, 0, 0);
    EXPECT_EQ(SA(e, EM_LINEFROMCHAR, -1, 0), 0);
    U(DestroyWindow)(p);
}

TEST(edit_multiline_getline_table)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkmedit(p, 0, 1, k_doc);
    static const char *want[5] = { "a", "bb", "ccc", "", "last" };
    for (int i = 0; i < 5; i++) {
        char buf[32]; memset(buf, 'x', sizeof buf);
        *(WORD *)buf = 30;
        LRESULT n = SA(e, EM_GETLINE, i, buf);
        CHECKF(n == k_doc_len[i], "line %d n=%lld", i, (long long)n);
        CHECKF(!memcmp(buf, want[i], k_doc_len[i]), "line %d content", i);
        /* not NUL terminated */
        /* the copy never writes a terminator */
        /* buf[0..1] hold the WORD capacity (30) and are only overwritten by
         * copied text, so the terminator probe is meaningful for n >= 2; for
         * shorter lines the count bytes must be left untouched. */
        if (n >= 2) CHECKF(buf[n] == 'x', "line %d: byte after copy is 0x%02x", i, (unsigned char)buf[n]);
        else CHECKF((unsigned char)buf[n] == (n == 0 ? 30 : 0), "line %d: count byte clobbered (0x%02x)", i, (unsigned char)buf[n]);
    }
    char buf[32];
    /* invalid line number */
    memset(buf, 'x', sizeof buf); *(WORD *)buf = 30;
    SetLastError(0);
    EXPECT_EQ(SA(e, EM_GETLINE, 5, buf), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    EXPECT_EQ(SA(e, EM_GETLINE, 100, buf), 0);
    /* truncated by buffer size */
    memset(buf, 'x', sizeof buf); *(WORD *)buf = 2;
    EXPECT_EQ(SA(e, EM_GETLINE, 4, buf), 2);
    CHECK(buf[0] == 'l' && buf[1] == 'a' && buf[2] == 'x');
    memset(buf, 'x', sizeof buf); *(WORD *)buf = 1;
    EXPECT_EQ(SA(e, EM_GETLINE, 4, buf), 1);
    CHECK(buf[0] == 'l' && buf[1] == 0);   /* buf[1] is the untouched high byte of the WORD capacity */
    memset(buf, 'x', sizeof buf); *(WORD *)buf = 4;
    EXPECT_EQ(SA(e, EM_GETLINE, 4, buf), 4);
    CHECK(!memcmp(buf, "last", 4));
    memset(buf, 'x', sizeof buf); *(WORD *)buf = 0;
    EXPECT_EQ(SA(e, EM_GETLINE, 4, buf), 0);
    EXPECT_EQ(buf[0], 0);   /* untouched low byte of the WORD capacity (0) */
    U(DestroyWindow)(p);
}

TEST(edit_multiline_getline_wide)
{
    REQUIRE(U(CreateWindowExW));
    HWND p = c_parent(300, 300);
    HWND e = c_ctlw(p, L"Edit", L"one\r\n\x0416\x4E2D two\r\nlast", ES_MULTILINE | ES_AUTOVSCROLL, 3, 0, 0, 200, 100);
    WCHAR b[32];
    EXPECT_EQ(SWm(e, EM_GETLINECOUNT, 0, 0), 3);
    memset(b, 'x', sizeof b); b[0] = 30;
    EXPECT_EQ(SWm(e, EM_GETLINE, 1, b), 6);
    CHECK(b[0] == 0x0416 && b[1] == 0x4E2D && b[2] == ' ' && b[3] == 't' && b[5] == 'o');
    memset(b, 'x', sizeof b); b[0] = 3;
    EXPECT_EQ(SWm(e, EM_GETLINE, 1, b), 3);
    CHECK(b[0] == 0x0416 && b[1] == 0x4E2D && b[2] == ' ');
    EXPECT_EQ(SWm(e, EM_LINELENGTH, 6, 0), 6);
    EXPECT_EQ(SWm(e, EM_LINEINDEX, 1, 0), 5);
    EXPECT_EQ(SWm(e, EM_LINEINDEX, 2, 0), 13);
    U(DestroyWindow)(p);
}

TEST(edit_singleline_line_messages)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND e = mkedit(p, 0, 1, "single line");
    EXPECT_EQ(SA(e, EM_GETLINECOUNT, 0, 0), 1);
    EXPECT_EQ(SA(e, EM_LINEINDEX, 0, 0), 0);
    EXPECT_EQ(SA(e, EM_LINEINDEX, -1, 0), 0);
    EXPECT_EQ(SA(e, EM_LINELENGTH, 0, 0), 11);
    EXPECT_EQ(SA(e, EM_LINELENGTH, 5, 0), 11);
    EXPECT_EQ(SA(e, EM_LINEFROMCHAR, 5, 0), 0);
    EXPECT_EQ(SA(e, EM_LINEFROMCHAR, -1, 0), 0);
    char b[32]; memset(b, 'x', sizeof b); *(WORD *)b = 30;
    EXPECT_EQ(SA(e, EM_GETLINE, 0, b), 11);
    CHECK(!memcmp(b, "single line", 11));
    EXPECT_EQ(SA(e, EM_GETFIRSTVISIBLELINE, 0, 0), 0);
    U(DestroyWindow)(p);
}

TEST(edit_getline_empty_control)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND e = mkmedit(p, 0, 1, "");
    char b[8]; memset(b, 'x', sizeof b); *(WORD *)b = 6;
    EXPECT_EQ(SA(e, EM_GETLINECOUNT, 0, 0), 1);
    EXPECT_EQ(SA(e, EM_GETLINE, 0, b), 0);
    EXPECT_EQ(SA(e, EM_LINELENGTH, 0, 0), 0);
    EXPECT_EQ(SA(e, EM_LINEINDEX, 0, 0), 0);
    EXPECT_EQ(SA(e, EM_LINEINDEX, 1, 0), -1);
    EXPECT_EQ(SA(e, EM_LINEFROMCHAR, 0, 0), 0);
    U(DestroyWindow)(p);
}

TEST(edit_wrap_vs_autohscroll_linecount)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    char longtxt[400];
    for (int i = 0; i < 399; i++) longtxt[i] = (i % 6 == 5) ? ' ' : 'a';
    longtxt[399] = 0;
    HWND w = c_ctl(p, "Edit", longtxt, ES_MULTILINE | ES_AUTOVSCROLL, 1, 0, 0, 100, 100);
    HWND nw = c_ctl(p, "Edit", longtxt, ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL, 2, 0, 0, 100, 100);
    CHECK(SA(w, EM_GETLINECOUNT, 0, 0) > 1);
    EXPECT_EQ(SA(nw, EM_GETLINECOUNT, 0, 0), 1);
    /* soft wrapping does not change the text */
    EXPECT_EQ(SA(w, WM_GETTEXTLENGTH, 0, 0), 399);
    U(DestroyWindow)(p);
}

TEST(edit_fmtlines_softbreaks)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    char longtxt[200];
    for (int i = 0; i < 199; i++) longtxt[i] = (i % 6 == 5) ? ' ' : 'a';
    longtxt[199] = 0;
    HWND w = c_ctl(p, "Edit", longtxt, ES_MULTILINE | ES_AUTOVSCROLL, 1, 0, 0, 90, 100);
    int before = (int)SA(w, WM_GETTEXTLENGTH, 0, 0);
    LRESULT r = SA(w, EM_FMTLINES, TRUE, 0);
    int after = (int)SA(w, WM_GETTEXTLENGTH, 0, 0);
    EXPECT_EQ(r, 1);   /* returns (wParam != 0) */
    CHECK(after > before);
    EXPECT_EQ(SA(w, EM_FMTLINES, FALSE, 0), 0);
    EXPECT_EQ(SA(w, WM_GETTEXTLENGTH, 0, 0), before);
    U(DestroyWindow)(p);
}

TEST(edit_charfrompos_posfromchar)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 1, "WWWWWWWWWW");
    HWND m = mkmedit(p, 0, 2, "line0\r\nline1\r\nline2");
    LRESULT r = SA(e, EM_CHARFROMPOS, 0, MAKELPARAM(0, 0));
    EXPECT_EQ(LOWORD(r), 0);
    EXPECT_EQ(HIWORD(r), 0);
    /* far to the right: last position */
    r = SA(e, EM_CHARFROMPOS, 0, MAKELPARAM(190, 5));
    CHECK(LOWORD(r) <= 10);
    t_note("CHARFROMPOS(190,5) on 10 chars -> %d", LOWORD(r));
    /* positions of successive chars are strictly increasing in x, constant y */
    int px = -1000;
    for (int i = 0; i < 6; i++) {
        LRESULT pos = SA(e, EM_POSFROMCHAR, i, 0);
        int x = (short)LOWORD(pos), y = (short)HIWORD(pos);
        CHECKF(x > px, "char %d x=%d prev=%d", i, x, px);
        px = x;
        CHECK(y >= 0 && y < 24);
        /* round-trip: the point of char i maps back to i */
        LRESULT back = SA(e, EM_CHARFROMPOS, 0, MAKELPARAM(x + 1, y + 1));
        CHECKF(LOWORD(back) == i, "round trip %d -> %d", i, LOWORD(back));
    }
    /* multi-line: line 1 sits below line 0 */
    LRESULT p0 = SA(m, EM_POSFROMCHAR, 0, 0), p1 = SA(m, EM_POSFROMCHAR, 7, 0), p2 = SA(m, EM_POSFROMCHAR, 14, 0);
    int y0 = (short)HIWORD(p0), y1 = (short)HIWORD(p1), y2 = (short)HIWORD(p2);
    CHECK(y1 > y0);
    CHECK(y2 - y1 == y1 - y0);
    EXPECT_EQ((short)LOWORD(p0), (short)LOWORD(p1));
    r = SA(m, EM_CHARFROMPOS, 0, MAKELPARAM(1, y1 + 1));
    EXPECT_EQ(HIWORD(r), 1);
    EXPECT_EQ(LOWORD(r), 7);
    /* the char index in LOWORD is relative to the whole text for multiline */
    r = SA(m, EM_CHARFROMPOS, 0, MAKELPARAM(1, y2 + 1));
    EXPECT_EQ(HIWORD(r), 2);
    EXPECT_EQ(LOWORD(r), 14);
    U(DestroyWindow)(p);
}

TEST(edit_margins)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 1, "m");
    SA(e, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(5, 7));
    LRESULT r = SA(e, EM_GETMARGINS, 0, 0);
    EXPECT_EQ(LOWORD(r), 5); EXPECT_EQ(HIWORD(r), 7);
    SA(e, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(9, 123));
    r = SA(e, EM_GETMARGINS, 0, 0);
    EXPECT_EQ(LOWORD(r), 9); EXPECT_EQ(HIWORD(r), 7);
    SA(e, EM_SETMARGINS, EC_RIGHTMARGIN, MAKELPARAM(123, 11));
    r = SA(e, EM_GETMARGINS, 0, 0);
    EXPECT_EQ(LOWORD(r), 9); EXPECT_EQ(HIWORD(r), 11);
    SA(e, EM_SETMARGINS, 0, MAKELPARAM(1, 1));   /* no flags: no change */
    r = SA(e, EM_GETMARGINS, 0, 0);
    EXPECT_EQ(LOWORD(r), 9); EXPECT_EQ(HIWORD(r), 11);
    SA(e, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(0, 0));
    r = SA(e, EM_GETMARGINS, 0, 0);
    EXPECT_EQ(LOWORD(r), 0); EXPECT_EQ(HIWORD(r), 0);
    /* margin shifts the character position */
    SA(e, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(0, 0));
    int x0 = (short)LOWORD(SA(e, EM_POSFROMCHAR, 0, 0));
    SA(e, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(20, 0));
    int x1 = (short)LOWORD(SA(e, EM_POSFROMCHAR, 0, 0));
    EXPECT_EQ(x1 - x0, 20);
    /* EC_USEFONTINFO */
    SA(e, EM_SETMARGINS, EC_USEFONTINFO, 0);
    r = SA(e, EM_GETMARGINS, 0, 0);
    t_note("USEFONTINFO margins %d,%d", LOWORD(r), HIWORD(r));
    U(DestroyWindow)(p);
}

TEST(edit_margins_multiline)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkmedit(p, 0, 1, "m");
    SA(e, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(3, 4));
    LRESULT r = SA(e, EM_GETMARGINS, 0, 0);
    EXPECT_EQ(LOWORD(r), 3); EXPECT_EQ(HIWORD(r), 4);
    U(DestroyWindow)(p);
}

TEST(edit_empty_text_undo_state)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND e = mkedit(p, 0, 1, "");
    EXPECT_EQ(SA(e, EM_CANUNDO, 0, 0), FALSE);
    /* documented: EM_UNDO on a single-line edit control always returns TRUE */
    EXPECT_EQ(SA(e, EM_UNDO, 0, 0), TRUE);
    EXPECT_EQ(SA(e, EM_GETLINECOUNT, 0, 0), 1);
    EXPECT_EQ(SA(e, EM_GETMODIFY, 0, 0), FALSE);
    HWND m = mkmedit(p, 0, 2, "");
    t_note("multiline empty EM_UNDO -> %lld", (long long)SA(m, EM_UNDO, 0, 0));
    U(DestroyWindow)(p);
}

TEST(edit_wm_char_typing)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND e = mkedit(p, 0, 1, "");
    const char *s = "Hello";
    for (; *s; s++) SA(e, WM_CHAR, *s, 1);
    CHECK(c_text_is(e, "Hello"));
    CHECK(c_sel_is(e, 5, 5));
    SA(e, WM_CHAR, '\b', 1);
    CHECK(c_text_is(e, "Hell"));
    SA(e, EM_SETSEL, 1, 3);
    SA(e, WM_CHAR, 'a', 1);
    CHECK(c_text_is(e, "Hal"));
    CHECK(c_sel_is(e, 2, 2));
    /* backspace over a selection removes it */
    SA(e, EM_SETSEL, 0, 2);
    SA(e, WM_CHAR, '\b', 1);
    CHECK(c_text_is(e, "l"));
    SA(e, EM_SETSEL, 0, 0);
    SA(e, WM_CHAR, '\b', 1);
    CHECK(c_text_is(e, "l"));
    /* single-line: Enter is not inserted */
    SA(e, EM_SETSEL, 1, 1);
    SA(e, WM_CHAR, '\r', 1);
    CHECK(c_text_is(e, "l"));
    SA(e, WM_CHAR, '\n', 1);
    t_note("single-line edit after WM_CHAR(LF): \"%s\"", "");
    U(DestroyWindow)(p);
}

TEST(edit_wm_char_multiline_enter)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkmedit(p, 0, 1, "");
    SA(e, WM_CHAR, 'a', 1);
    SA(e, WM_CHAR, '\r', 1);
    SA(e, WM_CHAR, 'b', 1);
    CHECK(c_text_is(e, "a\r\nb"));
    EXPECT_EQ(SA(e, EM_GETLINECOUNT, 0, 0), 2);
    EXPECT_EQ(SA(e, WM_GETTEXTLENGTH, 0, 0), 4);
    SA(e, WM_CHAR, '\b', 1);
    SA(e, WM_CHAR, '\b', 1);   /* removes CRLF as a unit */
    CHECK(c_text_is(e, "a"));
    SA(e, WM_CHAR, '\t', 1);
    CHECK(c_text_is(e, "a\t"));
    U(DestroyWindow)(p);
}

TEST(edit_wm_keydown_movement)
{
    if (GetAsyncKeyState(VK_SHIFT) < 0 || GetAsyncKeyState(VK_CONTROL) < 0 || GetAsyncKeyState(VK_MENU) < 0) { t_skip("modifier key held"); return; }
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND e = mkedit(p, 0, 1, "abcdef");
    SA(e, EM_SETSEL, 3, 3);
    SA(e, WM_KEYDOWN, VK_LEFT, 0);
    CHECK(c_sel_is(e, 2, 2));
    SA(e, WM_KEYDOWN, VK_RIGHT, 0);
    SA(e, WM_KEYDOWN, VK_RIGHT, 0);
    CHECK(c_sel_is(e, 4, 4));
    SA(e, WM_KEYDOWN, VK_HOME, 0);
    CHECK(c_sel_is(e, 0, 0));
    SA(e, WM_KEYDOWN, VK_END, 0);
    CHECK(c_sel_is(e, 6, 6));
    SA(e, WM_KEYDOWN, VK_RIGHT, 0);
    CHECK(c_sel_is(e, 6, 6));
    SA(e, WM_KEYDOWN, VK_HOME, 0);
    SA(e, WM_KEYDOWN, VK_LEFT, 0);
    CHECK(c_sel_is(e, 0, 0));
    SA(e, WM_KEYDOWN, VK_DELETE, 0);
    CHECK(c_text_is(e, "bcdef"));
    SA(e, EM_SETSEL, 1, 3);
    SA(e, WM_KEYDOWN, VK_DELETE, 0);
    CHECK(c_text_is(e, "bef"));
    SA(e, EM_SETSEL, 3, 3);
    SA(e, WM_KEYDOWN, VK_DELETE, 0);
    CHECK(c_text_is(e, "bef"));
    U(DestroyWindow)(p);
}

TEST(edit_getdlgcode)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND e = mkedit(p, 0, 1, "x");
    HWND m = mkmedit(p, 0, 2, "x");
    LRESULT c = SA(e, WM_GETDLGCODE, 0, 0);
    CHECK(c & DLGC_WANTCHARS);
    CHECK(c & DLGC_HASSETSEL);
    CHECK(c & DLGC_WANTARROWS);
    c = SA(m, WM_GETDLGCODE, 0, 0);
    CHECK(c & DLGC_WANTCHARS);
    CHECK(c & DLGC_HASSETSEL);
    CHECK(c & DLGC_WANTARROWS);
    U(DestroyWindow)(p);
}

TEST(edit_font_getset)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND e = mkedit(p, 0, 1, "x");
    HFONT f = (HFONT)GetStockObject(ANSI_FIXED_FONT);
    EXPECT_EQ(SA(e, WM_GETFONT, 0, 0), 0);
    SA(e, WM_SETFONT, f, TRUE);
    EXPECT_EQ(SA(e, WM_GETFONT, 0, 0), f);
    /* position of char 1 equals the font's average width multiple (fixed pitch) */
    SA(e, WM_SETTEXT, 0, "iiii");
    int x0 = (short)LOWORD(SA(e, EM_POSFROMCHAR, 0, 0));
    int x1 = (short)LOWORD(SA(e, EM_POSFROMCHAR, 1, 0));
    int x2 = (short)LOWORD(SA(e, EM_POSFROMCHAR, 2, 0));
    CHECK(x1 > x0);
    EXPECT_EQ(x2 - x1, x1 - x0);
    SA(e, WM_SETFONT, NULL, TRUE);
    EXPECT_EQ(SA(e, WM_GETFONT, 0, 0), 0);
    U(DestroyWindow)(p);
}

TEST(edit_destroyed_handle)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND e = mkedit(p, 0, 1, "abc");
    U(DestroyWindow)(e);
    EXPECT_EQ(SA(e, WM_GETTEXTLENGTH, 0, 0), 0);
    EXPECT_EQ(SA(e, EM_GETLINECOUNT, 0, 0), 0);
    EXPECT_EQ(SA(e, EM_SETSEL, 0, -1), 0);
    EXPECT_EQ(SA(e, EM_GETSEL, 0, 0), 0);
    U(DestroyWindow)(p);
}

TEST(edit_many_create_destroy)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    for (int i = 0; i < 200; i++) {
        HWND e = (i & 1) ? mkmedit(p, 0, 1, "loop\r\ntext") : mkedit(p, 0, 1, "loop text");
        if (!e) { CHECKF(0, "create %d", i); break; }
        SA(e, EM_SETSEL, 0, -1);
        SA(e, EM_REPLACESEL, FALSE, "z");
        U(DestroyWindow)(e);
    }
    CHECK(1);
    U(DestroyWindow)(p);
}

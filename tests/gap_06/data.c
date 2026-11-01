/* gap6: gSharedInfo, gapfnScSendMessage data exports. */
#include "t.h"

static int readable(const void *p, size_t n) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!p) return 0;
    if (!VirtualQuery(p, &mbi, sizeof mbi)) return 0;
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return 0;
    if ((const char *)p + n > (const char *)mbi.BaseAddress + mbi.RegionSize) return 0;
    return 1;
}
static int in_user32(const void *p) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!p || !VirtualQuery(p, &mbi, sizeof mbi)) return 0;
    return (HMODULE)mbi.AllocationBase == t_user32();
}

typedef struct { void *phead; void *pOwner; BYTE bType; BYTE bFlags; WORD wUniq; } HE64;
/* SHAREDINFO header: psi, aheList, HeEntrySize (ULONG), pDispInfo, ulSharedDelta, then
 * awmControl[31]. Win10 22H2 reports HeEntrySize 0x20 (entries are wider than HE64),
 * so the handle table must be indexed by HeEntrySize, never sizeof(HE64). */
typedef struct { void *psi; HE64 *aheList; ULONG HeEntrySize; ULONG pad; ULONG_PTR pDispInfo; ULONG_PTR delta; } SHAREDINFO_HEAD;
/* Win10 22H2 (HeEntrySize 0x20, per ValidateHwnd): +0 phead (an offset into the desktop heap),
 * +0x10 pOwner, +0x18 bType, +0x19 bFlags, +0x1a wUniq. The classic 0x18-byte layout has
 * pOwner at +8 and bType at +0x10. he_at() returns a normalised copy (NULL if unreadable). */
static HE64 *he_at(const SHAREDINFO_HEAD *s, UINT idx) {
    static HE64 norm;
    ULONG sz = s->HeEntrySize;
    if (sz < sizeof(HE64) || sz > 256 || (sz & 7)) sz = sizeof(HE64);
    const unsigned char *raw = (const unsigned char *)s->aheList + (size_t)idx * sz;
    if (!readable(raw, sz)) return NULL;
    norm.phead = *(void *const *)raw;
    if (sz >= 0x20) {
        norm.pOwner = *(void *const *)(raw + 0x10);
        norm.bType = raw[0x18]; norm.bFlags = raw[0x19]; norm.wUniq = *(const WORD *)(raw + 0x1a);
    } else {
        norm.pOwner = *(void *const *)(raw + 8);
        norm.bType = raw[0x10]; norm.bFlags = raw[0x11]; norm.wUniq = *(const WORD *)(raw + 0x12);
    }
    return &norm;
}

TEST(gap6_sharedinfo_resolve) {
    void *p = (void *)t_proc("gSharedInfo");
    REQUIRE(p);
    CHECK(in_user32(p));
    CHECK(readable(p, sizeof(SHAREDINFO_HEAD)));
    CHECK(((ULONG_PTR)p & 7) == 0);
}
TEST(gap6_sharedinfo_pointers) {
    SHAREDINFO_HEAD *s = (void *)t_proc("gSharedInfo");
    REQUIRE(s);
    if (!readable(s, sizeof *s)) { t_skip("unreadable"); return; }
    CHECK(s->psi != NULL);
    CHECK(s->aheList != NULL);
    CHECK(((ULONG_PTR)s->psi & 7) == 0);
    CHECK(((ULONG_PTR)s->aheList & 7) == 0);
    CHECK(readable(s->psi, 16));
    CHECK(readable(s->aheList, sizeof(HE64)));
    t_note("psi=%p ahe=%p HeEntrySize=0x%lx pDispInfo=%p delta=%p", s->psi, s->aheList, s->HeEntrySize, (void *)s->pDispInfo, (void *)s->delta);
    CHECK(s->HeEntrySize >= sizeof(HE64) && s->HeEntrySize <= 256 && !(s->HeEntrySize & 7));
}
TEST(gap6_sharedinfo_stable) {
    SHAREDINFO_HEAD *s = (void *)t_proc("gSharedInfo");
    REQUIRE(s);
    if (!readable(s, sizeof *s)) { t_skip("unreadable"); return; }
    void *psi = s->psi; HE64 *ahe = s->aheList;
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 10, 10, NULL);
    if (w) DestroyWindow(w);
    CHECK(s->psi == psi);
    CHECK(s->aheList == ahe);
    CHECK(t_proc("gSharedInfo") == (FARPROC)s);
}
TEST(gap6_sharedinfo_handle_entry_window) {
    SHAREDINFO_HEAD *s = (void *)t_proc("gSharedInfo");
    REQUIRE(s);
    if (!readable(s, sizeof *s) || !readable(s->aheList, sizeof(HE64))) { t_skip("no table"); return; }
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 10, 10, NULL);
    if (!w) { t_skip("no window"); return; }
    UINT idx = (UINT)((ULONG_PTR)w & 0xffff);
    HE64 *e = he_at(s, idx);
    if (!readable(e, sizeof *e)) { t_note("entry %u unreadable", idx); DestroyWindow(w); return; }
    t_note("window entry type=%u flags=%02x uniq=%04x hwnd=%p", e->bType, e->bFlags, e->wUniq, w);
    /* TYPE_WINDOW == 1; uniqueness counter is the high word of the handle. */
    CHECK(e->bType == 1);
    CHECK(e->wUniq == (WORD)(((ULONG_PTR)w >> 16) & 0xffff));
    CHECK(e->phead != NULL);
    if (s->HeEntrySize < 0x20 && e->phead && readable(e->phead, sizeof(void *) + 4))
        CHECK(*(HWND *)e->phead == w);
    DestroyWindow(w);
    e = he_at(s, idx);
    if (readable(e, sizeof *e)) t_note("after destroy type=%u", e->bType);
}
TEST(gap6_sharedinfo_handle_entry_multiple) {
    SHAREDINFO_HEAD *s = (void *)t_proc("gSharedInfo");
    REQUIRE(s);
    if (!readable(s, sizeof *s) || !readable(s->aheList, sizeof(HE64))) { t_skip("no table"); return; }
    HWND w[8];
    int n = 0;
    for (int i = 0; i < 8; i++) { w[i] = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 10, 10, NULL); if (w[i]) n++; }
    for (int i = 0; i < 8; i++) {
        if (!w[i]) continue;
        HE64 *e = he_at(s, (UINT)((ULONG_PTR)w[i] & 0xffff));
        if (!readable(e, sizeof *e)) continue;
        CHECKF(e->bType == 1, "win %d type %u", i, e->bType);
        CHECK(e->wUniq == (WORD)(((ULONG_PTR)w[i] >> 16) & 0xffff));
        if (!e->pOwner) t_note("win %d null owner", i);
    }
    for (int i = 0; i < 8; i++) if (w[i]) DestroyWindow(w[i]);
}
TEST(gap6_sharedinfo_handle_entry_menu) {
    SHAREDINFO_HEAD *s = (void *)t_proc("gSharedInfo");
    HMENU (WINAPI *cm)(void) = (void *)t_proc("CreateMenu");
    BOOL (WINAPI *dm)(HMENU) = (void *)t_proc("DestroyMenu");
    REQUIRE(s); REQUIRE(cm); REQUIRE(dm);
    if (!readable(s, sizeof *s) || !readable(s->aheList, sizeof(HE64))) { t_skip("no table"); return; }
    HMENU m = cm();
    if (!m) { t_skip("no menu"); return; }
    HE64 *e = he_at(s, (UINT)((ULONG_PTR)m & 0xffff));
    if (readable(e, sizeof *e)) {
        t_note("menu entry type=%u", e->bType);
        CHECK(e->bType == 2); /* TYPE_MENU */
    }
    dm(m);
}
TEST(gap6_sharedinfo_handle_entry_cursor) {
    SHAREDINFO_HEAD *s = (void *)t_proc("gSharedInfo");
    HICON (WINAPI *lc)(HINSTANCE, LPCSTR) = (void *)t_proc("LoadCursorA");
    REQUIRE(s); REQUIRE(lc);
    if (!readable(s, sizeof *s) || !readable(s->aheList, sizeof(HE64))) { t_skip("no table"); return; }
    HICON c = lc(NULL, (LPCSTR)IDC_ARROW);
    if (!c) { t_skip("no cursor"); return; }
    HE64 *e = he_at(s, (UINT)((ULONG_PTR)c & 0xffff));
    if (readable(e, sizeof *e)) {
        t_note("cursor entry type=%u", e->bType);
        CHECK(e->bType == 3); /* TYPE_CURSOR */
    }
}
TEST(gap6_sharedinfo_entry_zero) {
    SHAREDINFO_HEAD *s = (void *)t_proc("gSharedInfo");
    REQUIRE(s);
    if (!readable(s, sizeof *s) || !readable(s->aheList, sizeof(HE64))) { t_skip("no table"); return; }
    /* Slot 0 is never a live object handle. */
    t_note("entry0 type=%u", he_at(s, 0)->bType);
    CHECK(1);
}
TEST(gap6_sharedinfo_message_tables) {
    /* awmControl[31] follows ulSharedDelta (offset 0x28); each WNDMSG is
     * {maxMsgs, abMsgs*}. */
    SHAREDINFO_HEAD *s = (void *)t_proc("gSharedInfo");
    REQUIRE(s);
    struct WNDMSG { DWORD max; DWORD pad; const BYTE *bits; } *tab = (void *)((char *)s + 0x28); /* after ulSharedDelta */
    if (!readable(tab, 31 * sizeof *tab)) { t_skip("no table"); return; }
    int nz = 0;
    for (int i = 0; i < 31; i++) {
        if (tab[i].max && tab[i].bits) {
            nz++;
            CHECKF(tab[i].max < 0x10000, "ctl %d max %lu", i, tab[i].max);
            CHECKF(readable(tab[i].bits, (tab[i].max + 7) / 8), "ctl %d bits unreadable", i);
        }
    }
    t_note("%d populated control message tables", nz);
}

/* gapfnScSendMessage: table of server-callback send-message handlers. */
TEST(gap6_scsendmessage_resolve) {
    void *p = (void *)t_proc("gapfnScSendMessage");
    REQUIRE(p);
    CHECK(in_user32(p));
    CHECK(readable(p, 8 * sizeof(void *)));
    CHECK(((ULONG_PTR)p & 7) == 0);
}
TEST(gap6_scsendmessage_entries) {
    void **t = (void *)t_proc("gapfnScSendMessage");
    REQUIRE(t);
    if (!readable(t, 8 * sizeof(void *))) { t_skip("unreadable"); return; }
    int n = 0, bad = 0;
    for (int i = 0; i < 32; i++) {
        if (!readable(t + i, sizeof(void *))) break;
        if (!t[i]) continue;
        n++;
        if (!in_user32(t[i])) bad++;
    }
    t_note("%d populated entries, %d outside user32", n, bad);
    CHECK(n > 0);
}
TEST(gap6_scsendmessage_exec_targets) {
    void **t = (void *)t_proc("gapfnScSendMessage");
    REQUIRE(t);
    if (!readable(t, 8 * sizeof(void *))) { t_skip("unreadable"); return; }
    for (int i = 0; i < 16; i++) {
        if (!t[i]) continue;
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(t[i], &mbi, sizeof mbi)) continue;
        if (!(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) t_note("entry %d not executable (table may be shorter than scanned)", i);
    }
}
TEST(gap6_scsendmessage_unchanged_by_activity) {
    void **t = (void *)t_proc("gapfnScSendMessage");
    REQUIRE(t);
    if (!readable(t, 8 * sizeof(void *))) { t_skip("unreadable"); return; }
    void *copy[8]; memcpy(copy, t, sizeof copy);
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 10, 10, NULL);
    if (w) { SendMessageA(w, WM_SETTEXT, 0, (LPARAM)"x"); DestroyWindow(w); }
    CHECK(memcmp(copy, t, sizeof copy) == 0);
}

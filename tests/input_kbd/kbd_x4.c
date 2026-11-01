/* Non-US keyboard layouts addressed through explicit HKLs (MapVirtualKeyEx, VkKeyScanEx,
 * ToUnicodeEx). Nothing here activates a layout or changes the thread's active layout; layouts
 * that were not already loaded are unloaded again. The tests skip when a layout cannot be
 * loaded. Character/key positions are the standard KBDGR/KBDFR/KBDUK/KBDRU tables. */
#include "kbd.h"

#define SH 1
#define CT 2
#define AL 4
#define AG 8   /* AltGr: Ctrl + Alt (generic and right-hand entries) */

typedef struct { HKL h; int loaded; } xl;

static int xl_load(xl *l, const WCHAR *klid, WORD lang)
{
    HKL list[64];
    int n, i, had = 0;
    l->h = NULL; l->loaded = 0;
    if (!U(LoadKeyboardLayoutW) || !U(GetKeyboardLayoutList) || !U(ToUnicodeEx) || !U(MapVirtualKeyExW) || !U(VkKeyScanExW)) {
        t_skip("missing exports"); return 0;
    }
    n = U(GetKeyboardLayoutList)(64, list);
    for (i = 0; i < n; i++) if ((WORD)(ULONG_PTR)list[i] == lang && (WORD)((ULONG_PTR)list[i] >> 16) == lang) had = 1;
    l->h = U(LoadKeyboardLayoutW)(klid, 0);
    if (!l->h) { t_skip("layout %ls cannot be loaded (err %lu)", klid, GetLastError()); return 0; }
    if ((WORD)(ULONG_PTR)l->h != lang) { t_skip("layout %ls loaded as %p, not language 0x%x", klid, l->h, lang); l->h = NULL; return 0; }
    l->loaded = !had;
    return 1;
}
static void xl_free(xl *l)
{
    if (l->loaded && l->h && U(UnloadKeyboardLayout)) U(UnloadKeyboardLayout)(l->h);
    l->h = NULL; l->loaded = 0;
}

static void mk(BYTE *st, DWORD m)
{
    memset(st, 0, 256);
    if (m & SH) st[VK_SHIFT] = st[VK_LSHIFT] = 0x80;
    if (m & (CT | AG)) st[VK_CONTROL] = st[VK_LCONTROL] = 0x80;
    if (m & AL) st[VK_MENU] = st[VK_LMENU] = 0x80;
    if (m & AG) st[VK_MENU] = st[VK_RMENU] = 0x80;
}
/* ToUnicodeEx on a flags=4 call (do not disturb the thread's dead-key state). Returns the count; *ch = first char. */
static int tu(HKL h, UINT vk, DWORD m, WCHAR *ch)
{
    BYTE st[256]; WCHAR o[4] = { 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA };
    UINT sc = U(MapVirtualKeyExW)(vk, MAPVK_VK_TO_VSC, h);
    int r;
    mk(st, m);
    r = U(ToUnicodeEx)(vk, sc, st, o, 4, 4, h);
    *ch = o[0];
    return r;
}
static void want(HKL h, UINT vk, DWORD m, WCHAR c)
{
    WCHAR g; int r = tu(h, vk, m, &g);
    CHECKF(r == 1 && g == c, "vk 0x%02x mod %lx: ret %d char %04x want %04x", vk, m, r, g, c);
}
/* VkKeyScanExW for c: low byte vk, high byte shift state (1 shift, 2 ctrl, 4 alt). */
static void vks(HKL h, WCHAR c, int vk, int state)
{
    SHORT r = U(VkKeyScanExW)(c, h);
    CHECKF(r != -1 && (r & 0xff) == vk && ((r >> 8) & 0xff) == state, "U+%04x: got 0x%04x want vk 0x%02x state %d", c, (unsigned short)r, vk, state);
}

TEST(input_kbd_x4_de_qwertz_swap) {
    xl l;
    if (!xl_load(&l, L"00000407", 0x0407)) return;
    EXPECT_EQ(U(MapVirtualKeyExW)('Z', MAPVK_VK_TO_VSC, l.h), 0x15);
    EXPECT_EQ(U(MapVirtualKeyExW)('Y', MAPVK_VK_TO_VSC, l.h), 0x2C);
    EXPECT_EQ(U(MapVirtualKeyExW)(0x15, MAPVK_VSC_TO_VK, l.h), 'Z');
    EXPECT_EQ(U(MapVirtualKeyExW)(0x2C, MAPVK_VSC_TO_VK, l.h), 'Y');
    vks(l.h, L'z', 'Z', 0); vks(l.h, L'y', 'Y', 0);
    vks(l.h, L'Z', 'Z', 1); vks(l.h, L'Y', 'Y', 1);
    want(l.h, 'Z', 0, L'z'); want(l.h, 'Y', 0, L'y');
    want(l.h, 'Z', SH, L'Z'); want(l.h, 'Y', SH, L'Y');
    xl_free(&l);
}
TEST(input_kbd_x4_de_umlauts_roundtrip) {
    xl l;
    static const WCHAR ch[] = { 0xE4, 0xF6, 0xFC, 0xDF, 0xC4, 0xD6, 0xDC };
    int i;
    if (!xl_load(&l, L"00000407", 0x0407)) return;
    for (i = 0; i < 7; i++) {
        SHORT r = U(VkKeyScanExW)(ch[i], l.h);
        BYTE vk = (BYTE)r;
        DWORD m = 0;
        if (r == -1) { CHECKF(0, "U+%04x not mapped on the German layout", ch[i]); continue; }
        if ((r >> 8) & 1) m |= SH;
        if (((r >> 8) & 6) == 6) m |= AG;
        want(l.h, vk, m, ch[i]);
    }
    /* positions: ue=OEM_1, oe=OEM_3, ae=OEM_7, sz=OEM_4 */
    vks(l.h, 0xFC, 0xBA, 0); vks(l.h, 0xF6, 0xC0, 0); vks(l.h, 0xE4, 0xDE, 0); vks(l.h, 0xDF, 0xDB, 0);
    xl_free(&l);
}
TEST(input_kbd_x4_de_altgr) {
    xl l;
    if (!xl_load(&l, L"00000407", 0x0407)) return;
    want(l.h, 'Q', AG, L'@');
    want(l.h, 'E', AG, 0x20AC);
    want(l.h, '7', AG, L'{'); want(l.h, '8', AG, L'['); want(l.h, '9', AG, L']'); want(l.h, '0', AG, L'}');
    want(l.h, 0xDB, AG, L'\\');
    vks(l.h, L'@', 'Q', 6);
    vks(l.h, 0x20AC, 'E', 6);
    xl_free(&l);
}
TEST(input_kbd_x4_de_shift_digits) {
    xl l;
    if (!xl_load(&l, L"00000407", 0x0407)) return;
    want(l.h, '2', SH, L'"'); want(l.h, '3', SH, 0xA7); want(l.h, '7', SH, L'/');
    want(l.h, '8', SH, L'('); want(l.h, '9', SH, L')'); want(l.h, '0', SH, L'=');
    want(l.h, '2', 0, L'2');
    xl_free(&l);
}
TEST(input_kbd_x4_fr_azerty) {
    xl l;
    if (!xl_load(&l, L"0000040C", 0x040C)) return;
    EXPECT_EQ(U(MapVirtualKeyExW)('A', MAPVK_VK_TO_VSC, l.h), 0x10);
    EXPECT_EQ(U(MapVirtualKeyExW)('Q', MAPVK_VK_TO_VSC, l.h), 0x1E);
    EXPECT_EQ(U(MapVirtualKeyExW)('Z', MAPVK_VK_TO_VSC, l.h), 0x11);
    EXPECT_EQ(U(MapVirtualKeyExW)('W', MAPVK_VK_TO_VSC, l.h), 0x2C);
    EXPECT_EQ(U(MapVirtualKeyExW)('M', MAPVK_VK_TO_VSC, l.h), 0x27);
    EXPECT_EQ(U(MapVirtualKeyExW)(0x10, MAPVK_VSC_TO_VK, l.h), 'A');
    want(l.h, 'A', 0, L'a'); want(l.h, 'Q', 0, L'q'); want(l.h, 'M', SH, L'M');
    xl_free(&l);
}
TEST(input_kbd_x4_fr_digit_row_needs_shift) {
    xl l;
    if (!xl_load(&l, L"0000040C", 0x040C)) return;
    want(l.h, '1', 0, L'&');
    want(l.h, '1', SH, L'1');
    want(l.h, '5', SH, L'5');
    vks(l.h, L'1', '1', 1);
    vks(l.h, L'&', '1', 0);
    xl_free(&l);
}
TEST(input_kbd_x4_uk_symbols) {
    xl l;
    if (!xl_load(&l, L"00000809", 0x0809)) return;
    want(l.h, '2', SH, L'"');
    want(l.h, '3', SH, 0xA3);
    want(l.h, 0xDE, 0, L'\'');
    want(l.h, 0xDE, SH, L'@');
    vks(l.h, 0xA3, '3', 1);
    vks(l.h, L'@', 0xDE, 1);
    xl_free(&l);
}
TEST(input_kbd_x4_ru_cyrillic) {
    xl l;
    SHORT r;
    if (!xl_load(&l, L"00000419", 0x0419)) return;
    want(l.h, 'Q', 0, 0x0439);
    want(l.h, 'Q', SH, 0x0419);
    want(l.h, 'A', 0, 0x0444);
    want(l.h, 'Z', 0, 0x044F);
    vks(l.h, 0x0439, 'Q', 0);
    vks(l.h, 0x0419, 'Q', 1);
    r = U(VkKeyScanExW)(L'q', l.h);
    CHECKF(r == -1, "Latin q on the Russian layout: got 0x%04x", (unsigned short)r);
    /* digits are shared with Latin */
    vks(l.h, L'1', '1', 0);
    xl_free(&l);
}
TEST(input_kbd_x4_explicit_hkl_leaves_active_layout) {
    xl l;
    HKL before, after;
    WCHAR c;
    if (!U(GetKeyboardLayout)) { t_skip("no GetKeyboardLayout"); return; }
    if (!xl_load(&l, L"00000407", 0x0407)) return;
    before = U(GetKeyboardLayout)(0);
    tu(l.h, 'Z', 0, &c);
    U(VkKeyScanExW)(L'z', l.h);
    U(MapVirtualKeyExW)('Z', MAPVK_VK_TO_VSC, l.h);
    after = U(GetKeyboardLayout)(0);
    EXPECT_EQ(after, before);
    xl_free(&l);
    EXPECT_EQ(U(GetKeyboardLayout)(0), before);
}
TEST(input_kbd_x4_layout_list_restored) {
    HKL a[64], b[64];
    int na, nb;
    xl l;
    if (!U(GetKeyboardLayoutList)) { t_skip("no GetKeyboardLayoutList"); return; }
    na = U(GetKeyboardLayoutList)(64, a);
    if (!xl_load(&l, L"00000419", 0x0419)) return;
    xl_free(&l);
    nb = U(GetKeyboardLayoutList)(64, b);
    EXPECT_EQ(nb, na);
    if (nb == na) CHECK(!memcmp(a, b, sizeof(HKL) * na));
}

#include <wchar.h>
#include <uchar.h>
#include <assert.h>

#define str L"\\"

int main() {
	assert((L"abc" "def")[0]);
	_Static_assert(sizeof (L"abc" "def")[0] == 4, "test");
	_Static_assert(sizeof ("abc" u"def")[0] == 2, "test2");
	_Static_assert(sizeof (L"abc") == 16, "");
	_Static_assert(sizeof (L"abc" "def") == 28, "");
	_Static_assert(sizeof (U"abc") == 16, "");
	_Static_assert(sizeof (u"abc") == 8, "");

	char nstr[] = {"abc" "def"};

	wchar_t wide_str[] = {L"abc" "def"};
	assert(sizeof(wide_str) == sizeof(L"abcdef"));

	wchar_t s2[] = L"ᚠᛇᚻ᛫ᛒᛦᚦ᛫ᚠᚱᚩᚠᚢᚱ";
	char32_t str32[] = U"𝒞𝔹";

	assert(str32[0] == 0x1d49e);
	assert(str32[1] == 0x1d539);
	assert(s2[0] == 0x16a0);
	assert(s2[sizeof s2 / sizeof *s2 - 2] == 0x16b1);

	assert((L"" "\355\300\300")[0] == 0355);

	return L'\0';
}

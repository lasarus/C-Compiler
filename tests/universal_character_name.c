#include <assert.h>
#include <string.h>

int func\u1234h(int a) {
	return a;
}

int func\u12341234h(int a) {
	return a;
}

int func\U00012213(int a) {
	return a;
}

int 𒈓(int a) {
	return a;
}

int main() {
	assert(func\u1234h(10) == 10);
	assert(func\u12341234h(10) == 10);
	assert(funcሴh(10) == 10);
	assert(funcሴ1234h(10) == 10);
	assert(func𒈓(10) == 10);
	assert(𒈓(10) == 10);
	// Who would have thought that unicode contained Cuneiform...

	assert(strcmp("\u16A8", "ᚨ") == 0);
	assert(strcmp("\U00012213", "𒈓") == 0);

	assert(L'\u16A8' == L'ᚨ');
	assert(L'\U00012213' == L'𒈓');
	assert(u'\u16A8' == u'ᚨ');
	assert(U'\U00012213' == U'𒈓');
}

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

int ð(int a) {
	return a;
}

int ð(int ð²) {
	int ð²ð² = ð² * ð²;
	return ð²ð²;
}

int main() {
	assert(ð(2) == 4);

	assert(func\u1234h(10) == 10);
	assert(func\u12341234h(10) == 10);
	assert(funcá´h(10) == 10);
	assert(funcá´1234h(10) == 10);
	assert(funcð(10) == 10);
	assert(ð(10) == 10);
	// Who would have thought that unicode contained Cuneiform...

	assert(strcmp("\u16A8", "á¨") == 0);
	assert(strcmp("\U00012213", "ð") == 0);

	assert(L'\u16A8' == L'á¨');
	assert(L'\U00012213' == L'ð');
	assert(u'\u16A8' == u'á¨');
	assert(U'\U00012213' == U'ð');
}

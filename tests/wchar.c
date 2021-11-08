// Support for wide characters is very limited.
// The compiler just converts them into
// normal characters with some padding.

#define str L"\\"

int main() {
	return L'\0';
}

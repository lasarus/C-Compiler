// DEFS A B
// Compile with -DA -DB -DC -DE -DF -DG -DH -DI

int main() {
#if defined(A)
	const int a = 0;
	a = 3;
#elif defined(B)
	int * const b = 0;
	b += 1;
#endif
	return 0;
}

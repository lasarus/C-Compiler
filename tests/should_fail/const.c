// DEFS A B C D E

#ifdef E
const unsigned int a = 0, b = (int)(a * 8);
#endif

int main() {
#if defined(A)
	const int a = 0;
	a = 3;
#elif defined(B)
	int * const b = 0;
	b += 1;
#elif defined(C)
	int arr[const 5];
#endif
	return 0;
}

#ifdef D
int func(int arr[const 11]) {
	arr += 3;
	return arr;
}
#endif

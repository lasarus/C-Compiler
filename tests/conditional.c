int main() {
	char *str1 = "Hello";
	const char *str2 = "Hello";
	const char *str3 = 0 ? str1 : str2;
}

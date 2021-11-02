
struct T {
	int n, arr[];
};

struct T ret_struct() {
	return (struct T) {1};
}

int main() {
	ret_struct();
}

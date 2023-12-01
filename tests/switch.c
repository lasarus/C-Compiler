int main(int argc, char **argv) {
	int i = 10;
	switch (i) {
	case 0: return 12;
	case 10: case 2:
		return 0;
	default: return 1;
	}
}

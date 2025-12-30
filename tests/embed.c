#include <assert.h>
#include <stdio.h>

int main() {
	int i = {
#embed "empty_data.txt"
	};
	assert(i == 10);
	int i2 =
#embed "empty_data.txt"
		;

	assert(i2 == i);

	{
		static const unsigned char array[] = {
#embed "data.txt"
		};

		_Static_assert(sizeof array == 7, "");
		assert(array[0] == 'A');
		assert(array[sizeof array - 2] == 'F');
	}

	/* 	{ */
	/* 		static const unsigned char array[] = { */
	/* #embed "data.txt" limit(3) */
	/* 		}; */

	/* 		_Static_assert(sizeof array == 3, ""); */
	/* 		assert(array[0] == 'A'); */
	/* 		assert(array[sizeof array - 1] == 'C'); */
	/* 	} */

	/* 	{ */
	/* 		static const unsigned char array[] = { */
	/* #embed "data.txt" prefix(0, 2, 3, ) */
	/* 		}; */

	/* 		_Static_assert(sizeof array == 10, ""); */
	/* 		assert(array[0] == 0); */
	/* 		assert(array[sizeof array - 2] == 'F'); */
	/* 	} */

	/* 	{ */
	/* 		static const unsigned char array[] = { */
	/* #embed "data.txt" suffix(, 0, 2, 3, ) */
	/* 		}; */

	/* 		_Static_assert(sizeof array == 10, ""); */
	/* 		assert(array[0] == 'A'); */
	/* 		assert(array[sizeof array - 2] == 2); */
	/* 	} */

	/* 	{ */
	/* 		static const unsigned char array[] = { */
	/* #embed "empty.txt" if_empty(79) */
	/* 		}; */

	/* 		_Static_assert(sizeof array == 1, ""); */
	/* 		assert(array[0] == 79); */
	/* 	} */

	/* 	{ */
	/* 		static const unsigned char array[] = { */
	/* #embed "/dev/urandom" limit(8) */
	/* 		}; */

	/* 		_Static_assert(sizeof array == 8, ""); */
	/* 	} */

	/* 	{ */
	/* 		static const unsigned char array[] = { */
	/* #embed "/dev/urandom" limit(0) if_empty(78) */
	/* 		}; */

	/* 		_Static_assert(sizeof array == 1, ""); */
	/* 		assert(array[0] == 78); */
	/* 	} */

	return 0;
}

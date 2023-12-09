_Static_assert(10ll, "error");
_Static_assert(sizeof (int) == 4, "error");

// Allow for static_assert without message.
_Static_assert(sizeof (int) == 4);

int main(void) {
}

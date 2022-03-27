#include <assert.h>

#define X 1
#define Y 2

int main() {
   assert(X == 1);
   assert(Y == 2);
   #define Y 3
#pragma push_macro("Y")
   #pragma push_macro("X")
   assert(X == 1);
   #define X 2
   assert(X == 2);
   #pragma pop_macro("X")
   assert(X == 1);
   #pragma pop_macro("Y")
   assert(Y == 3);

#pragma push_macro("Z")
   #undef Z
}

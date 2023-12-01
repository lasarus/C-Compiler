#include <stdio.h>
#include <assert.h>

int expect(int a, int b)
{
    int ret =0x123;
    if (!(a == b)) 
    {
        ret = 0x456;
       while(1)
	   {
		   a++;
	   }
    }

    return ret;
}


int t6a(int e, int x[][3])
{
    return expect(e, *(*(x + 1) + 1));
}

int t6()
{
    int a[2][3];
    int *p = a;
    *(p + 4) = 65;
    return t6a(65, a);
}

int main(void) {
	printf("%d\n", t6());
}

#include <stdio.h>

int main()
{
    #define _test(var, addr) do {    \
        __asm__ volatile (      \
            "movw %%dx, %0"     \
            "addw $0x01, %%dx"  \
            :   "=m" (*(addr)),   \
                "+d" (var)      \
            :                   \
            :   "cc", "memory"  \
        );                       \
    } while (0)
        
    int addr = 0;
    int var = 10;
    _test(var, &addr);
    printf("var is %d\n", var);

    return 0;
}

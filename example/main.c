#include <stdio.h>

struct Point { int x, y; };

int main(void)
{
    [[maybe_unused]]
    struct Point p = (struct Point) { .x = 1, .y = 2 };

    printf("%d + %d = %d\n :)\n", p.x, p.y, p.x + p.y);

    [[maybe_unused]]
    bool test = true;
    
    return 0;
}

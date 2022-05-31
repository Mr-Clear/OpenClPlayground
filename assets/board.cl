#include "Cell.h"

kernel
void board(write_only image2d_t out, __global struct Cell* board, int2 size)
{
    const int gx = get_global_id(0);
    const int gy = get_global_id(1);
    const struct Cell c = board[gx + size.x * gy];

    if (gx < size.x && gy < size.y)
    {

        float4 color;
        if (c.solid)
            color = (float4)(0, 0, 0, 0);
        else if (c.visited)
            color = (float4)(1, 1, 0, 0);
        else
            color = (float4)(0, 0.5f, 0, 0);

        write_imagef(out, (int2)(gx, gy), color);
    }
}

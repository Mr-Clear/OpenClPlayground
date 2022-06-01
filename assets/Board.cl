#include "Cell.h"

kernel
void board(write_only image2d_t out, __global struct Cell* board, int2 size)
{
    const int gx = get_global_id(0);
    const int gy = get_global_id(1);

    if (gx < size.x && gy < size.y)
    {
        struct Cell *c = &board[gx + size.x * gy];
        const struct Cell empty = {false, 0.f};

        const struct Cell *neighbors[9];
        if (gx > 1)
        {
            if (gy > 1)
                neighbors[0] = &board[(gx - 1) + size.x * (gy - 1)];
            else
                neighbors[0] = &empty;

            neighbors[3] = &board[(gx - 1) + size.x * gy];
            if (gy < size.y - 1)
                neighbors[6] = &board[(gx - 1) + size.x * (gy + 1)];
            else
                neighbors[6] = &empty;
        }
        else
        {
            neighbors[0] = &empty;
            neighbors[3] = &empty;
            neighbors[6] = &empty;
        }

        if (gy > 1)
            neighbors[1] = &board[gx + size.x * (gy - 1)];
        else
            neighbors[1] = &empty;

        neighbors[4] = c;
        if (gy < size.y - 1)
            neighbors[7] = &board[gx + size.x * (gy + 1)];
        else
            neighbors[7] = &empty;

        if (gx < size.x - 1)
        {
            if (gy > 1)
                neighbors[2] = &board[(gx + 1) + size.x * (gy - 1)];
            else
                neighbors[2] = &empty;

            neighbors[5] = &board[(gx + 1) + size.x * gy];
            if (gy < size.y - 1)
                neighbors[8] = &board[(gx + 1) + size.x * (gy + 1)];
            else
                neighbors[8] = &empty;
        }
        else
        {
            neighbors[2] = &empty;
            neighbors[5] = &empty;
            neighbors[8] = &empty;
        }

        const float p1 = 1.f / 64.f;
        const float p2 = 4.f / 64.f;
        const float p4 = 44.f / 64.f;
        c->trail = 0.99 * (neighbors[0]->trail * p1 + neighbors[1]->trail * p2 + neighbors[2]->trail * p1
                          + neighbors[3]->trail * p2 + neighbors[4]->trail * p4 + neighbors[5]->trail * p2
                          + neighbors[6]->trail * p1 + neighbors[7]->trail * p2 + neighbors[8]->trail * p1);


        float4 color;
        if (c->solid)
        {
            color = (float4)(.2, .2, .2, 0);
        }
        else
        {
            color = (float4)(0, 0, 0, 0);
        }


        const float4 trailColor = (float4)(1, 0, 0, 0);
        float trail = min(1.f, max(0.f, c->trail));
        color = color * (1.f - trail) + trailColor * trail;

        write_imagef(out, (int2)(gx, gy), color);
    }
}

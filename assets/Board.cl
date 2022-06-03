#include "Common.cl"

kernel
void board(write_only image2d_t out, __global struct Cell* board, int2 size)
{
    const int gx = get_global_id(0);
    const int gy = get_global_id(1);
    const int2 coords = (int2)(gx, gy);

    if (gx < size.x && gy < size.y)
    {
        struct Cell *c = cell(board, size, coords);

        const struct Cell *neighbors[9];
        neighbors[0] = cell(board, size, coords + (int2)(-1, -1));
        neighbors[1] = cell(board, size, coords + (int2)( 0, -1));
        neighbors[2] = cell(board, size, coords + (int2)( 1, -1));
        neighbors[3] = cell(board, size, coords + (int2)(-1,  0));
        neighbors[4] = c;
        neighbors[5] = cell(board, size, coords + (int2)( 1,  0));
        neighbors[6] = cell(board, size, coords + (int2)(-1,  1));
        neighbors[7] = cell(board, size, coords + (int2)( 0,  1));
        neighbors[8] = cell(board, size, coords + (int2)( 1,  1));

        const float p1 = 1.f / 128.f;
        const float p2 = 4.f / 128.f;
        const float p4 = 108.f / 128.f;
        const float fader = 0.99f;
        c->trail = fader * (neighbors[0]->trail * p1 + neighbors[1]->trail * p2 + neighbors[2]->trail * p1
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


        const float4 trailColor = (float4)(c->trail / 10.f, c->trail / 500.f, c->trail / 1000.f, 0);
        float trail = clamp(c->trail, 0.f, 1.f);
        color = color * (1.f - trail) + trailColor * trail;

        write_imagef(out, (int2)(gx, gy), color);
    }
}

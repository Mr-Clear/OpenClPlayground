#include "Actor.h"
#include "Cell.h"

kernel
void actor( __global struct Cell* board, int2 boardSize, __global struct Actor* actors, int actorSize)
{
    const int id = get_global_id(0);
    struct Actor *a = &actors[id];

    if (a->alive)
    {
        //printf("A %d: (%f,%f) - (%f,%f) %d\n", id, a->pos.x, a->pos.y, a->speed.x, a->speed.y, sizeof(struct Actor));

        float2 next = a->pos + a->speed;
        int2 nextI = convert_int2(next);

        if (nextI.x > boardSize.x - 1 || nextI.x < 0 || nextI.y > boardSize.y - 1 || nextI.y < 0)
        {
            a->alive = false;
            return;
        }

        a->pos = next;

        board[convert_int(a->pos.x) + boardSize.x * convert_int(a->pos.y)].visited = true;
    }
}


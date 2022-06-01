#include "Actor.h"
#include "Cell.h"

#include "Random.cl"

bool printSizeof = true;

kernel
void actor(__global struct Cell* board, int2 boardSize, __global struct Actor* actors, int actorSize,
           int generation)
{
    const int id = get_global_id(0);
    struct Actor *a = &actors[id];

    if (printSizeof && id == 0)
    {
        const int sc = sizeof(struct Cell);
        const int sa = sizeof(struct Actor);
        printf("OCL - sizeof(Cell) = %d, sizeof(Actor) = %d \n", sc, sa);
        printSizeof = false;
    }

    if (a->alive)
    {
        //printf("A %d: (%f,%f) - (%f,%f) %d\n", id, a->pos.x, a->pos.y, a->speed.x, a->speed.y, sizeof(struct Actor));

        a->direction = rndNormalF(generation * 31337 + id, a->direction, 0.05);
        a->speed = rndNormalF(generation * 7789 + id, a->speed * .99f + a->targetSpeed * .01f, 0.01);
        float2 speedVector = (float2)(cos(a->direction), sin(a->direction)) * a->speed;
        float2 next = a->pos + speedVector;
        int2 nextI = convert_int2(next);

        if (nextI.x > boardSize.x - 1 || nextI.x < 0 || nextI.y > boardSize.y - 1 || nextI.y < 0)
        {
            a->alive = false;
            return;
        }
        struct Cell *nextCell = &board[nextI.x + boardSize.x * nextI.y];
        if (nextCell->solid)
        {
            a->speed = 0;
        }
        else
        {
            a->pos = next;
        }

        board[convert_int(a->pos.x) + boardSize.x * convert_int(a->pos.y)].trail += a->speed * 2;
    }
}

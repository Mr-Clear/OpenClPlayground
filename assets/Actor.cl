#include "Actor.h"

#include "Common.cl"
#include "Random.cl"

bool printSizeof = true;

float evaluateCell(struct Cell *cell)
{
    return cell->solid * -10.f + cell->trail;
}

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

        const float2 directionVector = (float2)(cos(a->direction), sin(a->direction));

        const float maxTurn = 0.05;
        const int senseMin = 30;
        const int senseMax = 40;
        const float senseAngle = 90. * (float)M_PI / 180.;
        const int senseSteps = convert_int_rte(senseMax * senseAngle / 3);
        const float senseIncrement = senseAngle / senseSteps;
        float senseStart = -senseAngle / 2.f;
        float senseArray[1000];
        float senseDirArray[1000];
        for (int i = 0; i <= senseSteps; ++i)
        {
            const float dir = senseStart + i * senseIncrement;
            const float2 v = rotateVector(directionVector, dir);
            senseArray[i] = -fabs(dir);
            senseDirArray[i] = dir;
            for (int j = senseMin; j <= senseMax; j += 3)
            {
                const float2 vx = v * (float2)(j, j);
                senseArray[i] += evaluateCell(cellF(board, boardSize,  a->pos + vx));
            }
        }
        float maxSense = -INFINITY;
        float senseDir = 0;
        for (int i = 0; i <= senseSteps; ++i)
        {
            if (senseArray[i] > maxSense)
            {
                maxSense = senseArray[i];
                senseDir = senseDirArray[i];
            }
        }
        senseDir = clamp(senseDir, -maxTurn, maxTurn);

        struct Cell *ahead = cellF(board, boardSize, a->pos + rotateVector((float2)(50, 0), a->direction));

        a->direction = rndNormalF(generation * 31337 + id, a->direction + senseDir, 0.05);
        a->speed = rndNormalF(generation * 7789 + id, a->speed * .99f + a->targetSpeed * .01f, 0.01);
        float2 speedVector = (float2)(cos(a->direction), sin(a->direction)) * a->speed;
        float2 next = a->pos + speedVector;
        int2 nextI = toInt2(next);

        if (nextI.x > boardSize.x - 1 || nextI.x < 0 || nextI.y > boardSize.y - 1 || nextI.y < 0)
        {
            a->alive = false;
            return;
        }
        struct Cell *nextCell = cell(board, boardSize, nextI);
        if (nextCell->solid)
        {
            a->speed = 0;
        }
        else
        {
            a->pos = next;
        }

        cellF(board, boardSize, a->pos)->trail += a->speed * 2;
    }
}


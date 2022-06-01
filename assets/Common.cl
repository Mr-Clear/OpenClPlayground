#pragma once

#include "Cell.h"

static const struct Cell edgeCell = {true, 0.f};

struct Cell *cell(struct Cell* board, int2 boardSize, int2 coordinates)
{
    if (coordinates.x < 0 || coordinates.x >= boardSize.x || coordinates.y < 0 || coordinates.y >= boardSize.y)
        return &edgeCell;
    return &board[coordinates.x + boardSize.x * coordinates.y];
};

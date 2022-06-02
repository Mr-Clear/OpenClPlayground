#pragma once

#include "Cell.h"

int toInt(float v)
{
    return convert_int(round(v));
}

int2 toInt2(float2 v)
{
    return convert_int2(round(v));
}

static const struct Cell edgeCell = {true, 0.f};

struct Cell *cell(struct Cell* board, int2 boardSize, int2 coordinates)
{
    if (coordinates.x < 0 || coordinates.x >= boardSize.x || coordinates.y < 0 || coordinates.y >= boardSize.y)
        return &edgeCell;
    return &board[coordinates.x + boardSize.x * coordinates.y];
}

struct Cell *cellF(struct Cell* board, int2 boardSize, float2 coordinates)
{
    return cell(board, boardSize, toInt2(coordinates));
}

float2 rotateVector(float2 vec, float rad)
{
    return (float2)(vec.x * cos(rad) - vec.y * sin(rad), vec.x * sin(rad) + vec.y * cos(rad));
}

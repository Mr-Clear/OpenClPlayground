#pragma once

#include "assets/Cell.h"

#include <vector>

class Board
{
public:
    Board(int width, int height);

    [[nodiscard]] Cell &operator()(int x, int y);
    [[nodiscard]] const Cell &operator()(int x, int y) const;

    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;

    [[nodiscard]] std::vector<Cell> &cells();
    [[nodiscard]] const std::vector<Cell> &cells() const;

    [[nodiscard]] std::size_t dataSize() const;

private:
    const int m_width;
    const int m_height;
    std::vector<Cell> m_cells;
};


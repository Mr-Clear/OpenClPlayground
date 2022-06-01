#include "Board.h"

Board::Board(int width, int height) :
    m_cells(width * height, Cell{false, false}),
    m_width(width),
    m_height(height)
{ }

Cell &Board::operator()(int x, int y)
{
    return m_cells.at(x + y * m_width);
}

const Cell &Board::operator()(int x, int y) const
{
    return m_cells.at(x + y * m_width);
}

int Board::width() const
{
    return m_width;
}

int Board::height() const
{
    return m_height;
}

std::vector<Cell> &Board::cells()
{
    return m_cells;
}

const std::vector<Cell> &Board::cells() const
{
    return m_cells;
}

std::size_t Board::dataSize() const
{
    return cells().size() * sizeof(Cell);
}

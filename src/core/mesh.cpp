#include "mfe/core/mesh.hpp"

#include <stdexcept>

namespace mfe::core {

UniformMesh::UniformMesh(Rect die, std::size_t columns, std::size_t rows)
    : columns_(columns), rows_(rows), die_(die) {
    if (!die.valid() || die.width() <= 0 || die.height() <= 0 || columns == 0 || rows == 0 ||
        die.width() % static_cast<DbUnit>(columns) != 0 || die.height() % static_cast<DbUnit>(rows) != 0) {
        throw std::invalid_argument("mesh requires a positive, evenly divisible die and non-zero dimensions");
    }
    const auto width = die.width() / static_cast<DbUnit>(columns);
    const auto height = die.height() / static_cast<DbUnit>(rows);
    cells_.reserve(columns * rows);
    for (std::size_t row = 0; row < rows; ++row) {
        for (std::size_t column = 0; column < columns; ++column) {
            const auto left = die.left + static_cast<DbUnit>(column) * width;
            const auto bottom = die.bottom + static_cast<DbUnit>(row) * height;
            cells_.push_back({{left, bottom, left + width, bottom + height}, 0.0, 0.0});
        }
    }
}

std::size_t UniformMesh::index(std::size_t column, std::size_t row) const noexcept { return row * columns_ + column; }

const MeshCell& UniformMesh::cell(std::size_t column, std::size_t row) const {
    if (column >= columns_ || row >= rows_) throw std::out_of_range("mesh cell outside bounds");
    return cells_[index(column, row)];
}

MeshCell& UniformMesh::cell(std::size_t column, std::size_t row) {
    return const_cast<MeshCell&>(static_cast<const UniformMesh&>(*this).cell(column, row));
}

std::optional<std::size_t> UniformMesh::index_at(Point point) const noexcept {
    if (!die_.contains(point) || point.x == die_.right || point.y == die_.top) return std::nullopt;
    const auto cell_width = die_.width() / static_cast<DbUnit>(columns_);
    const auto cell_height = die_.height() / static_cast<DbUnit>(rows_);
    const auto column = static_cast<std::size_t>((point.x - die_.left) / cell_width);
    const auto row = static_cast<std::size_t>((point.y - die_.bottom) / cell_height);
    return index(column, row);
}

std::vector<std::size_t> UniformMesh::neighbors(std::size_t cell_index) const {
    if (cell_index >= cells_.size()) throw std::out_of_range("mesh cell outside bounds");
    const auto column = cell_index % columns_;
    const auto row = cell_index / columns_;
    std::vector<std::size_t> result;
    if (column > 0) result.push_back(index(column - 1, row));
    if (column + 1 < columns_) result.push_back(index(column + 1, row));
    if (row > 0) result.push_back(index(column, row - 1));
    if (row + 1 < rows_) result.push_back(index(column, row + 1));
    return result;
}

}  // namespace mfe::core

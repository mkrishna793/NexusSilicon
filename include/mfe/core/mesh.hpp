#pragma once

#include "mfe/core/geometry.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace mfe::core {

struct MeshCell {
    Rect bounds;
    double demand{0.0};
    double capacity{0.0};
};

class UniformMesh {
public:
    UniformMesh(Rect die, std::size_t columns, std::size_t rows);

    [[nodiscard]] std::size_t columns() const noexcept { return columns_; }
    [[nodiscard]] std::size_t rows() const noexcept { return rows_; }
    [[nodiscard]] std::size_t size() const noexcept { return cells_.size(); }
    [[nodiscard]] const MeshCell& cell(std::size_t column, std::size_t row) const;
    [[nodiscard]] MeshCell& cell(std::size_t column, std::size_t row);
    [[nodiscard]] std::optional<std::size_t> index_at(Point point) const noexcept;
    [[nodiscard]] std::vector<std::size_t> neighbors(std::size_t index) const;

private:
    std::size_t columns_{};
    std::size_t rows_{};
    Rect die_{};
    std::vector<MeshCell> cells_;
    [[nodiscard]] std::size_t index(std::size_t column, std::size_t row) const noexcept;
};

}  // namespace mfe::core

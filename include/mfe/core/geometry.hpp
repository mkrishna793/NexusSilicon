#pragma once

#include <algorithm>
#include <cstdint>

namespace mfe::core {

using DbUnit = std::int64_t;

struct Point {
    DbUnit x{};
    DbUnit y{};
};

struct Rect {
    DbUnit left{};
    DbUnit bottom{};
    DbUnit right{};
    DbUnit top{};

    [[nodiscard]] constexpr bool valid() const noexcept { return left <= right && bottom <= top; }
    [[nodiscard]] constexpr DbUnit width() const noexcept { return right - left; }
    [[nodiscard]] constexpr DbUnit height() const noexcept { return top - bottom; }
    [[nodiscard]] constexpr DbUnit area() const noexcept { return width() * height(); }

    [[nodiscard]] constexpr bool contains(Point point) const noexcept {
        return point.x >= left && point.x <= right && point.y >= bottom && point.y <= top;
    }

    [[nodiscard]] constexpr Rect intersection(const Rect& other) const noexcept {
        return {std::max(left, other.left), std::max(bottom, other.bottom),
                std::min(right, other.right), std::min(top, other.top)};
    }
};

}  // namespace mfe::core

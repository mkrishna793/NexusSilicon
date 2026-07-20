#pragma once

#include <array>
#include <optional>

namespace mfe::core {

// Positive-definite 2D tensor used to express directional routing/placement cost.
struct MetricTensor {
    double xx{1.0};
    double xy{0.0};
    double yy{1.0};

    [[nodiscard]] bool is_positive_definite() const noexcept;
    [[nodiscard]] double directional_cost(double dx, double dy) const noexcept;
    [[nodiscard]] std::optional<MetricTensor> inverse() const noexcept;
};

}  // namespace mfe::core

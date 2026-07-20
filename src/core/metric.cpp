#include "mfe/core/metric.hpp"

#include <cmath>

namespace mfe::core {

bool MetricTensor::is_positive_definite() const noexcept {
    return xx > 0.0 && yy > 0.0 && (xx * yy - xy * xy) > 0.0;
}

double MetricTensor::directional_cost(double dx, double dy) const noexcept {
    return xx * dx * dx + 2.0 * xy * dx * dy + yy * dy * dy;
}

std::optional<MetricTensor> MetricTensor::inverse() const noexcept {
    const double determinant = xx * yy - xy * xy;
    if (std::abs(determinant) < 1e-12) return std::nullopt;
    return MetricTensor{yy / determinant, -xy / determinant, xx / determinant};
}

}  // namespace mfe::core

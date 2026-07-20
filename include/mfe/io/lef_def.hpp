#pragma once

#include "mfe/core/geometry.hpp"
#include "mfe/pdk/technology.hpp"

#include <optional>
#include <string>
#include <vector>

namespace mfe::io {

struct ParseError {
    std::size_t line{};
    std::string message;
};

template <typename T>
struct ParseResult {
    std::optional<T> value;
    std::vector<ParseError> errors;
    [[nodiscard]] explicit operator bool() const noexcept { return value.has_value() && errors.empty(); }
};

struct Component {
    std::string name;
    std::string macro;
    core::Point location;
    bool fixed{false};
};

struct Net {
    std::string name;
    std::vector<std::string> connections;
};

struct Design {
    std::string name;
    core::DbUnit database_units_per_micron{};
    core::Rect die_area;
    std::vector<Component> components;
    std::vector<Net> nets;
    [[nodiscard]] bool validates() const noexcept;
};

// Bounded interchange readers. They accept routing-layer LEF and floorplan,
// component placement, and net connection DEF records.
[[nodiscard]] ParseResult<pdk::Technology> parse_lef(const std::string& text,
                                                       std::string technology_name = "lef-technology");
[[nodiscard]] ParseResult<Design> parse_def(const std::string& text);
[[nodiscard]] ParseResult<pdk::Technology> parse_lef_file(const std::string& path,
                                                            std::string technology_name = "lef-technology");
[[nodiscard]] ParseResult<Design> parse_def_file(const std::string& path);

// Deterministic DEF interchange writer for the bounded Design model. It emits
// die area, component placement, and logical net connectivity so downstream
// tools consume the same design state MFE parsed or produced.
[[nodiscard]] std::string write_def(const Design& design);
[[nodiscard]] bool write_def_file(const Design& design, const std::string& path);

}  // namespace mfe::io

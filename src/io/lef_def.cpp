#include "mfe/io/lef_def.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>

namespace mfe::io {
namespace {

std::string upper(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

std::vector<std::string> tokens(const std::string& line) {
    std::string normalized;
    normalized.reserve(line.size() * 2);
    for (const char ch : line) {
        normalized += (ch == '(' || ch == ')' || ch == ';' || ch == '+') ? ' ' : ch;
    }
    std::istringstream stream(normalized);
    std::vector<std::string> result;
    for (std::string token; stream >> token;) result.push_back(std::move(token));
    return result;
}

bool number(const std::string& value, core::DbUnit& result) {
    try {
        std::size_t used{};
        result = std::stoll(value, &used);
        return used == value.size();
    } catch (...) { return false; }
}

bool lef_dimension(const std::string& value, core::DbUnit dbu, core::DbUnit& result) {
    try {
        std::size_t used{};
        const auto microns = std::stold(value, &used);
        if (used != value.size() || microns <= 0.0L) return false;
        result = static_cast<core::DbUnit>(std::llround(microns * static_cast<long double>(dbu)));
        return result > 0;
    } catch (...) { return false; }
}

template <typename T>
ParseResult<T> unreadable(const std::string& path) {
    ParseResult<T> result;
    result.errors.push_back({0, "Unable to open file: " + path});
    return result;
}

}  // namespace

bool Design::validates() const noexcept {
    return !name.empty() && database_units_per_micron > 0 && die_area.valid() && die_area.width() > 0 && die_area.height() > 0;
}

ParseResult<pdk::Technology> parse_lef(const std::string& text, std::string technology_name) {
    ParseResult<pdk::Technology> result;
    core::DbUnit dbu{};
    std::vector<pdk::RoutingLayer> layers;
    pdk::RoutingLayer current;
    bool in_layer = false;
    std::istringstream input(text);
    std::size_t line_number{};
    for (std::string line; std::getline(input, line);) {
        ++line_number;
        const auto words = tokens(line);
        if (words.empty() || words.front().starts_with('#')) continue;
        const auto key = upper(words.front());
        if (key == "DATABASE" && words.size() >= 3 && upper(words[1]) == "MICRONS") {
            if (!number(words[2], dbu) || dbu <= 0) result.errors.push_back({line_number, "LEF DATABASE MICRONS must be positive."});
        } else if (key == "LAYER" && words.size() >= 2) {
            if (in_layer) result.errors.push_back({line_number, "Nested LEF LAYER is not supported."});
            current = {}; current.name = words[1]; in_layer = true;
        } else if (in_layer && key == "TYPE" && words.size() >= 2 && upper(words[1]) != "ROUTING") {
            current.name.clear();
        } else if (in_layer && key == "DIRECTION" && words.size() >= 2) {
            const auto direction = upper(words[1]);
            current.preferred_direction = direction == "HORIZONTAL" ? pdk::PreferredDirection::horizontal :
                                        direction == "VERTICAL" ? pdk::PreferredDirection::vertical : pdk::PreferredDirection::bidirectional;
        } else if (in_layer && key == "PITCH" && words.size() >= 2) {
            if (!lef_dimension(words[1], dbu, current.pitch)) result.errors.push_back({line_number, "Invalid LEF PITCH."});
        } else if (in_layer && key == "WIDTH" && words.size() >= 2) {
            if (!lef_dimension(words[1], dbu, current.minimum_width)) result.errors.push_back({line_number, "Invalid LEF WIDTH."});
        } else if (in_layer && key == "SPACING" && words.size() >= 2) {
            if (!lef_dimension(words[1], dbu, current.minimum_spacing)) result.errors.push_back({line_number, "Invalid LEF SPACING."});
        } else if (key == "END" && in_layer && words.size() >= 2 && words[1] == current.name) {
            if (!current.name.empty()) { current.level = static_cast<int>(layers.size() + 1); layers.push_back(current); }
            in_layer = false;
        }
    }
    if (dbu <= 0) result.errors.push_back({0, "LEF is missing UNITS DATABASE MICRONS."});
    result.value.emplace(std::move(technology_name), dbu);
    for (auto& layer : layers) result.value->add_routing_layer(std::move(layer));
    if (!result.value->validates()) result.errors.push_back({0, "LEF did not define valid routing layers."});
    return result;
}

ParseResult<Design> parse_def(const std::string& text) {
    ParseResult<Design> result;
    Design design;
    enum class Section { none, components, nets } section = Section::none;
    std::istringstream input(text);
    std::size_t line_number{};
    for (std::string line; std::getline(input, line);) {
        ++line_number;
        const auto words = tokens(line);
        if (words.empty() || words.front().starts_with('#')) continue;
        const auto key = upper(words.front());
        if (key == "DESIGN" && words.size() >= 2) design.name = words[1];
        else if (key == "UNITS" && words.size() >= 4 && upper(words[1]) == "DISTANCE" && upper(words[2]) == "MICRONS") {
            if (!number(words[3], design.database_units_per_micron)) result.errors.push_back({line_number, "Invalid DEF database units."});
        } else if (key == "DIEAREA" && words.size() >= 5) {
            if (!number(words[1], design.die_area.left) || !number(words[2], design.die_area.bottom) ||
                !number(words[3], design.die_area.right) || !number(words[4], design.die_area.top)) result.errors.push_back({line_number, "Invalid DEF DIEAREA."});
        } else if (key == "COMPONENTS") section = Section::components;
        else if (key == "NETS") section = Section::nets;
        else if (key == "END") section = Section::none;
        else if (key == "-" && words.size() >= 3 && section == Section::components) {
            Component component{words[1], words[2], {}, false};
            for (std::size_t i = 3; i + 2 < words.size(); ++i) {
                const auto status = upper(words[i]);
                if (status == "PLACED" || status == "FIXED") {
                    component.fixed = status == "FIXED";
                    if (!number(words[i + 1], component.location.x) || !number(words[i + 2], component.location.y))
                        result.errors.push_back({line_number, "Invalid component location."});
                    break;
                }
            }
            design.components.push_back(std::move(component));
        } else if (key == "-" && words.size() >= 2 && section == Section::nets) {
            Net net; net.name = words[1];
            for (std::size_t i = 2; i + 1 < words.size(); i += 2) net.connections.push_back(words[i] + "/" + words[i + 1]);
            design.nets.push_back(std::move(net));
        }
    }
    if (!design.validates()) result.errors.push_back({0, "DEF is missing a valid DESIGN, UNITS, or DIEAREA."});
    result.value = std::move(design);
    return result;
}

ParseResult<pdk::Technology> parse_lef_file(const std::string& path, std::string technology_name) {
    std::ifstream input(path);
    if (!input) return unreadable<pdk::Technology>(path);
    return parse_lef({std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()}, std::move(technology_name));
}

ParseResult<Design> parse_def_file(const std::string& path) {
    std::ifstream input(path);
    if (!input) return unreadable<Design>(path);
    return parse_def({std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()});
}

std::string write_def(const Design& design) {
    std::ostringstream output;
    output << "VERSION 5.8 ;\n"
           << "DIVIDERCHAR \"/\" ;\n"
           << "BUSBITCHARS \"[]\" ;\n"
           << "DESIGN " << design.name << " ;\n"
           << "UNITS DISTANCE MICRONS " << design.database_units_per_micron << " ;\n"
           << "DIEAREA ( " << design.die_area.left << ' ' << design.die_area.bottom << " ) ( "
           << design.die_area.right << ' ' << design.die_area.top << " ) ;\n\n";

    output << "COMPONENTS " << design.components.size() << " ;\n";
    for (const auto& component : design.components) {
        output << "- " << component.name << ' ' << component.macro << " + "
               << (component.fixed ? "FIXED" : "PLACED") << " ( "
               << component.location.x << ' ' << component.location.y << " ) N ;\n";
    }
    output << "END COMPONENTS\n\n";

    output << "NETS " << design.nets.size() << " ;\n";
    for (const auto& net : design.nets) {
        output << "- " << net.name;
        for (const auto& connection : net.connections) {
            const auto separator = connection.find('/');
            const auto instance = connection.substr(0, separator);
            const auto pin = separator == std::string::npos ? "" : connection.substr(separator + 1);
            output << " ( " << instance << ' ' << pin << " )";
        }
        output << " ;\n";
    }
    output << "END NETS\n\nEND DESIGN\n";
    return output.str();
}

bool write_def_file(const Design& design, const std::string& path) {
    std::ofstream output(path);
    if (!output) return false;
    output << write_def(design);
    return static_cast<bool>(output);
}

}  // namespace mfe::io

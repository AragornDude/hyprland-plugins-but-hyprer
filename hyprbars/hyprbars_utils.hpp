#pragma once

#include <optional>
#include <string>
#include <algorithm>
#include <cctype>

// Parse config-ish strings used by the plugin into an int (hex color or decimal).
// Returns std::nullopt on failure.
static inline std::optional<int> parseConfigInt(const std::string& s) {
    if (s.empty())
        return std::nullopt;

    // trim
    size_t b = s.find_first_not_of(" \t\n\r");
    size_t e = s.find_last_not_of(" \t\n\r");
    const std::string t = (b == std::string::npos) ? std::string() : s.substr(b, e - b + 1);

    try {
        // colors: rgb(ffffff) or rgba(ffffffff) (hex)
        if (t.rfind("rgb(", 0) == 0 || t.rfind("rgba(", 0) == 0) {
            auto start = t.find('(');
            auto end = t.find(')');
            if (start != std::string::npos && end != std::string::npos && end > start + 1) {
                std::string hex = t.substr(start + 1, end - start - 1);
                // remove possible prefixes/spaces
                hex.erase(std::remove_if(hex.begin(), hex.end(), ::isspace), hex.end());
                // If rgb with 6 hex digits, append alpha ff
                if (hex.size() == 6)
                    hex += "ff";
                unsigned long val = std::stoul(hex, nullptr, 16);
                return static_cast<int>(val);
            }
            return std::nullopt;
        }

        // numeric decimal
        if (std::all_of(t.begin(), t.end(), [](unsigned char c) { return std::isdigit(c) || c == '+' || c == '-'; })) {
            int v = std::stoi(t);
            return v;
        }

        // fallback: try parse as hex without parentheses
        bool allhex = !t.empty() && std::all_of(t.begin(), t.end(), [](unsigned char c) { return std::isxdigit(c); });
        if (allhex) {
            unsigned long val = std::stoul(t, nullptr, 16);
            return static_cast<int>(val);
        }
    } catch (...) {
        return std::nullopt;
    }

    return std::nullopt;
}

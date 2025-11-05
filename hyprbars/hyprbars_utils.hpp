#pragma once

#include <optional>
#include <string>
#include <cctype>
#include <sstream>
#include <algorithm>

// Minimal config string -> int parser used by the plugin.
// Supports:
//  - plain integers (decimal)
//  - hex in parentheses like "rgb(ff4040)", "rgba(33333388)"
// Returns std::optional<long long> (empty on parse failure).

inline std::optional<long long> parseConfigInt(const std::string& s) {
    // trim
    size_t b = 0, e = s.size();
    while (b < e && std::isspace((unsigned char)s[b])) ++b;
    while (e > b && std::isspace((unsigned char)s[e - 1])) --e;
    if (b >= e) return std::nullopt;
    const std::string t = s.substr(b, e - b);

    // If looks like rgb(...) or rgba(...)
    auto open = t.find('(');
    auto close = t.find(')');
    if (open != std::string::npos && close != std::string::npos && close > open) {
        std::string inner = t.substr(open + 1, close - open - 1);
        // remove 0x or # prefixes if present
        if (!inner.empty() && (inner.rfind("0x", 0) == 0 || inner.rfind("#", 0) == 0)) {
            if (inner.rfind("0x", 0) == 0)
                inner = inner.substr(2);
            else if (inner.rfind("#", 0) == 0)
                inner = inner.substr(1);
        }
        // remove possible spaces
        inner.erase(std::remove_if(inner.begin(), inner.end(), ::isspace), inner.end());
        // hex string should be 6 or 8 characters
        if (inner.size() == 6 || inner.size() == 8) {
            std::istringstream iss(inner);
            unsigned long long value = 0;
            iss >> std::hex >> value;
            if (!iss.fail()) {
                if (inner.size() == 6) {
                    // assume RGB, add full alpha
                    value = (value << 8) | 0xFFULL;
                }
                return static_cast<long long>(value);
            }
        }
        // fallback: try decimal inside
        try {
            long long v = std::stoll(inner);
            return v;
        } catch (...) {
            return std::nullopt;
        }
    }

    // Otherwise try plain decimal or hex (like 0x... or #...)
    try {
        if (t.rfind("0x", 0) == 0) {
            unsigned long long v = 0;
            std::istringstream iss(t.substr(2));
            iss >> std::hex >> v;
            if (!iss.fail())
                return static_cast<long long>(v);
            return std::nullopt;
        }
        if (t[0] == '#') {
            unsigned long long v = 0;
            std::istringstream iss(t.substr(1));
            iss >> std::hex >> v;
            if (!iss.fail())
                return static_cast<long long>(v);
            return std::nullopt;
        }
        long long v = std::stoll(t);
        return v;
    } catch (...) {
        return std::nullopt;
    }
}

// compat name used in older code
inline std::optional<long long> configStringToIntOpt(const std::string& s) {
    return parseConfigInt(s);
}

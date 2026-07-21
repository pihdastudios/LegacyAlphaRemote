#include "json_writer.h"

#include <stdio.h>

namespace legacyalpha {

std::string jsonEscape(const std::string &value) {
    std::string result;
    result.reserve(value.size() + 8);
    for (size_t i = 0; i < value.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        switch (c) {
            case '\"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buffer[7];
                    snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                    result += buffer;
                } else {
                    result += static_cast<char>(c);
                }
        }
    }
    return result;
}

std::string jsonString(const std::string &value) {
    return std::string("\"") + jsonEscape(value) + "\"";
}

}  // namespace legacyalpha

#include "util/Unicode.h"

namespace conduit::util {

int byteLength(unsigned char lead_byte) {
    // UTF-8 encoding 101:
    // 0xxxxxxx -> 1 byte  (ASCII, the easy case)
    // 110xxxxx -> 2 bytes
    // 1110xxxx -> 3 bytes
    // 11110xxx -> 4 bytes
    // anything else is a continuation byte or garbage
    if ((lead_byte & 0x80) == 0)    return 1;
    if ((lead_byte & 0xE0) == 0xC0) return 2;
    if ((lead_byte & 0xF0) == 0xE0) return 3;
    if ((lead_byte & 0xF8) == 0xF0) return 4;
    return 1; // broken byte, just skip it
}

size_t stringLength(const std::string& str) {
    size_t count = 0;
    size_t i = 0;
    while (i < str.size()) {
        i += byteLength(static_cast<unsigned char>(str[i]));
        count++;
    }
    return count;
}

std::string truncate(const std::string& str, size_t max_chars) {
    if (max_chars == 0) return "";

    size_t chars = 0;
    size_t i = 0;
    while (i < str.size() && chars < max_chars) {
        i += byteLength(static_cast<unsigned char>(str[i]));
        chars++;
    }

    // if we're already within the limit, no truncation needed
    if (i >= str.size()) return str;

    return str.substr(0, i);
}

size_t byteOffsetAt(const std::string& str, size_t char_index) {
    size_t chars = 0;
    size_t i = 0;
    while (i < str.size() && chars < char_index) {
        i += byteLength(static_cast<unsigned char>(str[i]));
        chars++;
    }
    return i;
}

bool isValidUtf8(const std::string& str) {
    size_t i = 0;
    while (i < str.size()) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        int len = byteLength(c);

        // make sure we have enough bytes remaining
        if (i + len > str.size()) return false;

        // verify continuation bytes (should all start with 10xxxxxx)
        for (int j = 1; j < len; j++) {
            unsigned char cont = static_cast<unsigned char>(str[i + j]);
            if ((cont & 0xC0) != 0x80) return false;
        }

        // reject overlong encodings (the classic UTF-8 security hole)
        if (len == 2 && c < 0xC2) return false;
        if (len == 3) {
            unsigned char b1 = static_cast<unsigned char>(str[i + 1]);
            if (c == 0xE0 && b1 < 0xA0) return false;
        }
        if (len == 4) {
            unsigned char b1 = static_cast<unsigned char>(str[i + 1]);
            if (c == 0xF0 && b1 < 0x90) return false;
            if (c > 0xF4) return false; // beyond unicode range
            if (c == 0xF4 && b1 > 0x8F) return false;
        }

        i += len;
    }
    return true;
}

} // namespace conduit::util

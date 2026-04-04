#pragma once
#include <string>
#include <cstddef>

namespace conduit::util {

// UTF-8 utilities for when std::string pretends multi-byte characters don't exist
// we're not implementing full ICU here, just enough to not butcher non-ASCII text

// how many bytes does this leading byte's character occupy?
// returns 1-4, or 1 for invalid bytes (we just treat them as single bytes and move on)
int byteLength(unsigned char lead_byte);

// count the number of unicode codepoints in a UTF-8 string
// (not the same as .size(), which gives you bytes, as you painfully learned)
size_t stringLength(const std::string& str);

// truncate a UTF-8 string to at most max_chars codepoints
// unlike str.substr() this won't slice a multi-byte character in half
std::string truncate(const std::string& str, size_t max_chars);

// get the byte offset of the Nth codepoint
// returns str.size() if n >= stringLength(str)
size_t byteOffsetAt(const std::string& str, size_t char_index);

// check if a string is valid UTF-8 (quick and dirty, not exhaustive)
bool isValidUtf8(const std::string& str);

} // namespace conduit::util

#pragma once
#include <string>
#include <unordered_map>

namespace conduit::render {

// maps slack emoji short names to their actual unicode UTF-8 sequences.
// covers the ~200 most common ones so chat doesn't look like a 90s IRC log.
// anything not in here falls back to :name: text, which is fine honestly.
const std::unordered_map<std::string, std::string>& getEmojiMap();

} // namespace conduit::render

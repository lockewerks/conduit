#include "input/CommandParser.h"
#include "util/Logger.h"
#include <sstream>
#include <algorithm>

namespace conduit::input {

void CommandParser::registerCommand(const std::string& name, const std::string& description,
                                     CommandHandler handler) {
    commands_[name] = {name, description, std::move(handler)};
}

std::optional<ParsedCommand> CommandParser::parse(const std::string& input) {
    if (input.empty() || input[0] != '/') return std::nullopt;

    ParsedCommand cmd;
    std::istringstream ss(input.substr(1)); // skip the /
    ss >> cmd.name;

    // grab the rest as args
    std::getline(ss >> std::ws, cmd.args);

    // split args into argv
    std::istringstream args_ss(cmd.args);
    std::string arg;
    while (args_ss >> arg) {
        cmd.argv.push_back(arg);
    }

    cmd.valid = commands_.count(cmd.name) > 0;
    return cmd;
}

bool CommandParser::execute(const ParsedCommand& cmd) {
    auto it = commands_.find(cmd.name);
    if (it == commands_.end()) {
        LOG_WARN("unknown command: /" + cmd.name);
        return false;
    }

    it->second.handler(cmd);
    return true;
}

bool CommandParser::execute(const std::string& input) {
    auto cmd = parse(input);
    if (!cmd) return false;
    return execute(*cmd);
}

std::vector<std::string> CommandParser::commandNames() const {
    std::vector<std::string> names;
    for (auto& [name, _] : commands_) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::string CommandParser::helpText(const std::string& name) const {
    auto it = commands_.find(name);
    if (it == commands_.end()) return "unknown command";
    return "/" + it->second.name + " - " + it->second.description;
}

std::string CommandParser::allHelp() const {
    std::string help;
    auto names = commandNames();
    for (auto& name : names) {
        help += helpText(name) + "\n";
    }
    return help;
}

} // namespace conduit::input

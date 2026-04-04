#pragma once
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <unordered_map>

namespace conduit::input {

struct ParsedCommand {
    std::string name;        // without the leading /
    std::string args;        // everything after the command name
    std::vector<std::string> argv; // args split by spaces
    bool valid = false;
};

// parses and routes /slash commands
class CommandParser {
public:
    using CommandHandler = std::function<void(const ParsedCommand& cmd)>;

    void registerCommand(const std::string& name, const std::string& description,
                         CommandHandler handler);

    // parse input text. returns nullopt if it's not a command (no leading /)
    std::optional<ParsedCommand> parse(const std::string& input);

    // execute a parsed command
    bool execute(const ParsedCommand& cmd);

    // execute raw input (parse + execute)
    bool execute(const std::string& input);

    // get all command names (for tab completion)
    std::vector<std::string> commandNames() const;

    // get help text for a command
    std::string helpText(const std::string& name) const;

    // get all help text
    std::string allHelp() const;

private:
    struct CommandInfo {
        std::string name;
        std::string description;
        CommandHandler handler;
    };
    std::unordered_map<std::string, CommandInfo> commands_;
};

} // namespace conduit::input

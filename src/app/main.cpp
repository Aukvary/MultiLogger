#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include "lib/Journal.hpp"

const char* TUTORIAL = "\n"
                       " ┌──────────────────────────────────────────────────────────┐\n"
                       " │  Options & Commands:                                     │\n"
                       " ├──────────────────────────────────────────────────────────┤\n"
                       " │  -h            Display this help message                 │\n"
                       " │  -f <path>     Set target file for writing logs          │\n"
                       " │  -dt <type>    Set default log level (Type)              │\n"
                       " │  -tl           Print list of all supported log types     │\n"
                       " ├──────────────────────────────────────────────────────────┤\n"
                       " │  ctrl+q        Safe exit from application                │\n"
                       " └──────────────────────────────────────────────────────────┘\n";

int main(int argc, char* argv[]) {
    std::cout << "-h for help" << std::endl;

    std::optional<std::string_view> startFile{std::nullopt};
    MultiLogger::LogType startType = MultiLogger::LogType::Message;

    for (int i = 0; i < argc; i++) {
        std::string_view arg{argv[i]};

        if (arg == "-f") {
            if (i == argc - 1) {
                std::cout << "lost arg -f";
                break;
            }

            startFile = argv[++i];
        } else if (arg == "-dt") {
            if (i == argc - 1) {
                std::cout << "lost arg -dt";
                break;
            }

            std::string_view type = argv[++i];

            if (type == "Message") {
                startType = MultiLogger::LogType::Message;
            } else if (type == "Warning") {
                startType = MultiLogger::LogType::Warning;
            } else if (type == "Error") {
                startType = MultiLogger::LogType::Error;
            } else {
                std::cout << "Undefined type" << type << std::endl;
            }
        }
    }

    std::optional<MultiLogger::FileWriter> fileWriter;

    try {
        fileWriter.emplace(startFile, startType);
    } catch (const std::runtime_error& err) {
        fileWriter.emplace(std::nullopt, startType);
        std::cout << err.what() << std::endl;
    }

    std::cout << fileWriter->File() << "> ";

    while (true) {
    }

    return 0;
}

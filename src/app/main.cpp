#include <iostream>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include "lib/Journal.hpp"

const char* TUTORIAL = " ┌──────────────────────────────────────────────────────────┐\n"
                       " │  Options & Commands:                                     │\n"
                       " ├──────────────────────────────────────────────────────────┤\n"
                       " │  -h            Display this help message                 │\n"
                       " │  -f <path>     Set target file for writing logs          │\n"
                       " │  -dt <type>    Set default log level (Type)              │\n"
                       " │  -tl           Print list of all supported log types     │\n"
                       " │  -l            log                                       │\n"
                       " ├──────────────────────────────────────────────────────────┤\n"
                       " │  exit        Safe exit from application                  │\n"
                       " └──────────────────────────────────────────────────────────┘";

int main(int argc, char* argv[]) {
    std::cout << TUTORIAL << std::endl;

    std::optional<std::string_view> startFile{std::nullopt};
    std::optional<MultiLogger::LogType> startType{MultiLogger::LogType::Message};

    for (int i = 0; i < argc; i++) {
        std::string_view arg{argv[i]};

        if (arg == "-f") {
            if (i == argc - 1) {
                std::cerr << "[Warning] lost arg -f";
                break;
            }

            startFile = argv[++i];
        } else if (arg == "-dt") {
            if (i == argc - 1) {
                std::cerr << "[Warning] lost arg -dt";
                break;
            }

            std::string_view type = argv[++i];
            startType = MultiLogger::StringToLogType(type);
            if (!startType) {
                std::cerr << "[Warning] Undefined type" << type << std::endl;
            }
        }
    }

    std::optional<MultiLogger::FileWriter> fileWriter;

    try {
        fileWriter.emplace(startFile, *startType);
    } catch (const std::runtime_error& err) {
        fileWriter.emplace(std::nullopt, *startType);
        std::cerr << err.what() << std::endl;
    }

    std::string input;

    while (std::cout << fileWriter->File() << ":("
                     << MultiLogger::LogTypeToStringView(fileWriter->DefaultType())
                     << ")"
                        "> "
                     << std::flush,
           std::getline(std::cin, input)) {
        if (input.empty()) {
            continue;
        }

        if (input == "exit") {
            break;
        }

        std::stringstream ss(input);
        std::string command;

        ss >> command;

        if (command == "-h") {
            std::cout << TUTORIAL << std::endl;
            continue;
        }
        if (command == "-tl") {
            std::cout << "Supported types: Message, Warning, Error" << std::endl;
            continue;
        }

        std::string arg;

        if (command == "-f") {
            if (!(ss >> arg)) {
                std::cerr << "[Warning] -f requires a file path" << std::endl;
                continue;
            }
            fileWriter->File(arg);
            continue;
        }

        if (command == "-dt") {
            if (!(ss >> arg)) {
                std::cerr << "[Warning] -dt requires a log type" << std::endl;
                continue;
            }

            auto type = MultiLogger::StringToLogType(arg);

            if (type)
                fileWriter->DefaultType(*type);
            else
                std::cerr << "[Warning] Undefined type" << arg << std::endl;
        } else if (command == "-l") {
            if (!(ss >> arg)) {
                std::cerr << "[Warning] -l requires a message" << std::endl;
                continue;
            }

            auto type = MultiLogger::StringToLogType(arg);

            if (type) {
                std::getline(ss >> std::ws, arg);
                fileWriter->Log(arg, *type);
            } else {
                std::string remain;
                std::getline(ss, remain);
                fileWriter->Log(arg + remain);
            }
        } else {
            std::cerr << "[Warning] undefined command" << std::endl;
        }
    }

    return 0;
}

#pragma once
/* github.com/sjoon-oh/soren
 * @author Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <memory>

/* 
 * This Logger class uses spdlog library, a simple header-only logging library for C++.
 */

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

namespace soren {

    /// @brief 
    class Logger {
        std::shared_ptr<spdlog::sinks::stdout_color_sink_mt>    console_lgr;    // Prints out to the console.
        std::shared_ptr<spdlog::sinks::basic_file_sink_mt>      file_lgr;       // Prints out to the file.

        std::unique_ptr<spdlog::logger>                         ms_lgr;

    public:
        Logger(std::string arg_lgr_name, std::string arg_filen = "soren.log") {
            const std::string formatted_log = "[%n:%^%l%$] %v";
            
            // Generate both file and console logger.
            console_lgr = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            file_lgr    = std::make_shared<spdlog::sinks::basic_file_sink_mt>(arg_filen, true);
            
            // File and Console logger uses the same format.
            console_lgr->set_pattern(formatted_log);
            file_lgr->set_pattern(formatted_log);

            console_lgr->set_level(spdlog::level::info);    // Make all visible.
            file_lgr->set_level(spdlog::level::info);        // Above info.

            // Use this.
            ms_lgr = std::unique_ptr<spdlog::logger>(new spdlog::logger(arg_lgr_name, {console_lgr, file_lgr}));
        }

        spdlog::logger* getLogger() { return ms_lgr.get(); }
    };

    class LoggerFileOnly {
        std::shared_ptr<spdlog::logger>     file_lgr;       // Prints out to the file.

    public:
        LoggerFileOnly(std::string arg_lgr_name, std::string arg_filen = "soren.log") {

            file_lgr = spdlog::basic_logger_mt(arg_lgr_name, arg_filen);

            file_lgr->set_level(spdlog::level::info);        // Above info.
        }

        spdlog::logger* getLogger() { return file_lgr.get(); }
    };

}

// Access loggers using the macros defined below.
#define SOREN_LOGGER_INFO(INSTANCE, ...)        do {(INSTANCE).getLogger()->info(__VA_ARGS__); } while(0)
#define SOREN_LOGGER_DEBUG(INSTANCE, ...)       do {(INSTANCE).getLogger()->debug(__VA_ARGS__); } while(0)
#define SOREN_LOGGER_ERROR(INSTANCE, ...)       do {(INSTANCE).getLogger()->error(__VA_ARGS__); } while(0)
#pragma once
/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <memory>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

namespace soren {

    class Logger {
        std::shared_ptr<spdlog::sinks::stdout_color_sink_mt>    console_lgr;
        std::shared_ptr<spdlog::sinks::basic_file_sink_mt>      file_lgr;

        std::unique_ptr<spdlog::logger>                         ms_lgr;

    public:
        Logger(std::string arg_lgr_name, std::string arg_filen = "soren.log") {
            const std::string formatted_log = "[%n:%^%l%$] %v";
            
            // Generate both file and console logger.
            console_lgr = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            file_lgr    = std::make_shared<spdlog::sinks::basic_file_sink_mt>(arg_filen, true);
            
            console_lgr->set_pattern(formatted_log);
            file_lgr->set_pattern(formatted_log);

            console_lgr->set_level(spdlog::level::info);    // Make all visible.
            file_lgr->set_level(spdlog::level::info);       // Above warning.

            // Use this.
            ms_lgr = std::unique_ptr<spdlog::logger>(new spdlog::logger(arg_lgr_name, {console_lgr, file_lgr}));
        }

        spdlog::logger* getLogger() { return ms_lgr.get(); }
    };

}

#define SOREN_LOGGER_INFO(INSTANCE, ...)        do {(INSTANCE).getLogger()->info(__VA_ARGS__); } while(0)
#define SOREN_LOGGER_ERROR(INSTANCE, ...)       do {(INSTANCE).getLogger()->error(__VA_ARGS__); } while(0)
#ifndef VIX_LOGGER_HPP
#define VIX_LOGGER_HPP

#include <string>
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/fmt/ostr.h>
#include <iostream>

namespace Vix
{
    class Logger
    {
    public:
        enum class Level
        {
            TRACE,
            DEBUG,
            INFO,
            WARN,
            ERROR,
            CRITICAL
        };

        static Logger &getInstance()
        {
            static Logger instance;
            return instance;
        }

        template <typename... Args>
        void log(Level level, const std::string &fmt, Args &&...args)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            switch (level)
            {
            case Level::TRACE:
                spd_->trace(fmt, std::forward<Args>(args)...);
                break;
            case Level::DEBUG:
                spd_->debug(fmt, std::forward<Args>(args)...);
                break;
            case Level::INFO:
                spd_->info(fmt, std::forward<Args>(args)...);
                break;
            case Level::WARN:
                spd_->warn(fmt, std::forward<Args>(args)...);
                break;
            case Level::ERROR:
                spd_->error(fmt, std::forward<Args>(args)...);
                break;
            case Level::CRITICAL:
                spd_->critical(fmt, std::forward<Args>(args)...);
                break;
            }
        }

        template <typename... Args>
        void logModule(const std::string &module, Level level, const std::string &fmt, Args &&...args)
        {
            log(level, "[{} {}]", module, fmt::format(fmt, std::forward<Args>(args)...));
        }

        [[noreturn]] void throwError(const std::string &msg)
        {
            log(Level::ERROR, msg);
            throw std::runtime_error(msg);
        }

    private:
        Logger()
        {
            try
            {
                auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                console_sink->set_level(spdlog::level::trace);

                auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("vix.log", 1024 * 1024 * 5, 3);
                file_sink->set_level(spdlog::level::trace);

                spd_ = std::make_shared<spdlog::logger>("vixLogger", spdlog::sinks_init_list{console_sink, file_sink});
                spd_->set_level(spdlog::level::info);
                spd_->flush_on(spdlog::level::warn);
            }
            catch (const spdlog::spdlog_ex &ex)
            {
                std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
            }
        }

        spdlog::level::level_enum toSpdLevel(Level level)
        {
            switch (level)
            {
            case Level::TRACE:
                return spdlog::level::trace;
            case Level::DEBUG:
                return spdlog::level::debug;
            case Level::INFO:
                return spdlog::level::info;
            case Level::WARN:
                return spdlog::level::warn;
            case Level::ERROR:
                return spdlog::level::err;
            case Level::CRITICAL:
                return spdlog::level::critical;
            default:
                return spdlog::level::info;
            }
        }

        std::shared_ptr<spdlog::logger> spd_;
        std::mutex mutex_;
    };
}

#endif
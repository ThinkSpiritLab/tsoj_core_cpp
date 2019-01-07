#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <sstream>
#include <boost/format.hpp>
#include <kerbal/utility/costream.hpp>
#include <kerbal/compatibility/chrono_suffix.hpp>
#include "db_typedef.hpp"

namespace costream_ns = kerbal::utility::costream;

extern costream_ns::costream<std::cerr> ccerr;

/**
 * @addtogroup log_level
 * @{
 */
enum class LogLevel
{
	LEVEL_FATAL = 0, LEVEL_WARNING = 1, LEVEL_INFO = 2, LEVEL_DEBUG = 3, LEVEL_PROFILE = 4
};

/**
 * 日志告警级别萃取器
 * @tparam level 日志告警级别
 */
template <LogLevel level>
struct Log_level_traits;

template <>
struct Log_level_traits<LogLevel::LEVEL_FATAL>
{
		static constexpr const char * str = "FATAL";
		static const costream_ns::costream<std::cout> outstream;
};

template <>
struct Log_level_traits<LogLevel::LEVEL_INFO>
{
		static constexpr const char * str = "INFO";
		static const costream_ns::costream<std::cout> outstream;
};

template <>
struct Log_level_traits<LogLevel::LEVEL_WARNING>
{
		static constexpr const char * str = "WARNING";
		static const costream_ns::costream<std::cout> outstream;
};

template <>
struct Log_level_traits<LogLevel::LEVEL_DEBUG>
{
		static constexpr const char * str = "DEBUG";
		static const costream_ns::costream<std::cout> outstream;
};

template <>
struct Log_level_traits<LogLevel::LEVEL_PROFILE>
{
		static constexpr const char * str = "PROF";
		static const costream_ns::costream<std::cout> outstream;
};

/**
 * @}
 */

std::string get_ymd_hms_in_local_time_zone(time_t time) noexcept;

template <typename Tp>
void multi_args_write(std::ostream & log_fp, Tp && arg0)
{
	log_fp << arg0;
}

template <typename Tp, typename ...Up>
void multi_args_write(std::ostream & log_fp, Tp && arg0, Up&& ...args)
{
	log_fp << arg0;
	multi_args_write(log_fp, std::forward<Up>(args)...);
}

namespace ts_judger
{
	namespace log
	{
		static boost::format templ("[%s] %s job:%d:%d [%s:%d]");
		/*           datetime logLevelStr type id srcFileName line */

		template <typename Type>
		Type & cptr_cast(Type & src) noexcept
		{
			return src;
		}

		template <typename Type>
		const Type & cptr_cast(const Type & src) noexcept
		{
			return src;
		}

		template <typename Type>
		Type && cptr_cast(Type && src) noexcept
		{
			return std::move(src);
		}

		template <size_t N>
		constexpr const char* cptr_cast(const char (&src)[N]) noexcept
		{
			return src;
		}

		template <LogLevel level, typename ...T>
		void __log_write(int type, int job_id, const char source_filename[], int line, std::ostream & log_file, T&& ... args) noexcept
		{
			try {
				if (!log_file) {
					std::cerr << "log file is not open!" << std::endl;
					return;
				}

				const time_t now = time(NULL);
				const std::string datetime = get_ymd_hms_in_local_time_zone(now);

				std::ostringstream buffer;

				multi_args_write(buffer, log::templ % datetime % (const char *) Log_level_traits<level>::str % type % job_id % source_filename % line, std::forward<T>(args)...);

				Log_level_traits<level>::outstream << buffer.str() << std::endl;
				log_file << buffer.str() << std::endl;

				if (log_file.fail()) { //http://www.cplusplus.com/reference/ios/ios/fail/
					std::cerr << "write error!" << std::endl;
					log_file.clear();
					return;
				}
			} catch (...) {
			}
		}

	} /* namespace log */

} /* namespace ts_judger */

template <LogLevel level, typename ...T>
void log_write(int type, ojv4::s_id_type job_id, const char source_filename[], int line, std::ostream & log_file, T&& ... args) noexcept
{
	ts_judger::log::__log_write<level>(type, job_id.to_literal(), source_filename, line, log_file, ts_judger::log::cptr_cast(std::forward<T>(args))...);
}

template <LogLevel level, typename ...T>
void log_write(int type, ojv4::s_id_type job_id, const char source_filename[], int line, std::ofstream & log_file, T&& ... args) noexcept
{
	ts_judger::log::__log_write<level>(type, job_id.to_literal(), source_filename, line, (std::ostream&)(log_file), ts_judger::log::cptr_cast(std::forward<T>(args))...);
}

template <LogLevel level, typename ...T>
void log_write(int type, int job_id, const char source_filename[], int line, std::ostream & log_file, T&& ... args) noexcept
{
	ts_judger::log::__log_write<level>(type, job_id, source_filename, line, log_file, ts_judger::log::cptr_cast(std::forward<T>(args))...);
}

template <LogLevel level, typename ...T>
void log_write(int type, int job_id, const char source_filename[], int line, std::ofstream & log_file, T&& ... args) noexcept
{
	ts_judger::log::__log_write<level>(type, job_id, source_filename, line, (std::ostream&)(log_file), ts_judger::log::cptr_cast(std::forward<T>(args))...);
}

#define UNKNOWN_EXCEPTION_WHAT (const char*)("unknown exception")

#ifdef LOG_DEBUG
#	undef LOG_DEBUG
#endif
#ifdef DEBUG
#	define LOG_DEBUG(type, job_id, log_fp, x...) \
	log_write<LogLevel::LEVEL_DEBUG>(type, job_id, __FILE__, __LINE__, log_fp, ##x)
#else
#	define LOG_DEBUG(type, job_id, log_fp, x...)
#endif

#ifdef LOG_INFO
#	undef LOG_INFO
#endif
#define LOG_INFO(type, job_id, log_fp, x...) \
	log_write<LogLevel::LEVEL_INFO>(type, job_id, __FILE__, __LINE__, log_fp, ##x)

#ifdef LOG_WARNING
#	undef LOG_WARNING
#endif
#define LOG_WARNING(type, job_id, log_fp, x...)	 \
	log_write<LogLevel::LEVEL_WARNING>(type, job_id, __FILE__, __LINE__, log_fp, ##x)

#ifdef LOG_FATAL
#	undef LOG_FATAL
#endif
#define LOG_FATAL(type, job_id, log_fp, x...) \
	log_write<LogLevel::LEVEL_FATAL>(type, job_id, __FILE__, __LINE__, log_fp, ##x)

#ifdef LOG_PROFILE
#	undef LOG_PROFILE
#	undef PROFILE_HEAD
#	undef PROFILE_TAIL
#endif
#ifdef PROFILE
#	define LOG_PROFILE(type, job_id, log_fp, args...) \
		log_write<LogLevel::LEVEL_PROFILE>(type, job_id, __FILE__, __LINE__, log_fp, ##args)
#	define PROFILE_HEAD \
		using namespace kerbal::compatibility::chrono_suffix; \
		auto __profile_start = std::chrono::system_clock::now();
#	define PROFILE_TAIL(type, job_id, log_fp, args...) \
		LOG_PROFILE(type, job_id, log_fp, BOOST_CURRENT_FUNCTION, ": consume: ", \
		std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - __profile_start).count(), " ms ", ##args);

#	define PROFILE_WARNING_TAIL(type, job_id, log_fp, consume_threshold, args...) \
		auto __profile_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - __profile_start); \
		if (__profile_ms > consume_threshold) { \
			PROFILE_TAIL(type, job_id, log_fp, ##args); \
		}

#else
#	define LOG_PROFILE(type, job_id, log_fp, args...)
#	define PROFILE_HEAD
#	define PROFILE_WARNING_TAIL(type, job_id, log_fp, consume_threshold, args...)
#endif

#ifdef EXCEPT_WARNING
#	undef EXCEPT_WARNING
#endif
#define EXCEPT_WARNING(type, job_id, log_fp, events, exception, x...)	LOG_WARNING(type, job_id, log_fp, events, \
																		" Error information: ", exception.what(), "  Exception type: ", typeid(exception).name(), ##x)
#ifdef UNKNOWN_EXCEPT_WARNING
#	undef UNKNOWN_EXCEPT_WARNING
#endif
#define UNKNOWN_EXCEPT_WARNING(type, job_id, log_fp, events, x...)	LOG_WARNING(type, job_id, log_fp, events, \
																		" Error information: ", UNKNOWN_EXCEPTION_WHAT, ##x)

#ifdef EXCEPT_FATAL
#	undef EXCEPT_FATAL
#endif
#define EXCEPT_FATAL(type, job_id, log_fp, events, exception, x...)	    LOG_FATAL(type, job_id, log_fp, events, \
																		" Error information: ", exception.what(), "  Exception type: ", typeid(exception).name(), ##x)
#ifdef UNKNOWN_EXCEPT_FATAL
#	undef UNKNOWN_EXCEPT_FATAL
#endif
#define UNKNOWN_EXCEPT_FATAL(type, job_id, log_fp, events, x...)	    LOG_FATAL(type, job_id, log_fp, events, \
																		" Error information: ", UNKNOWN_EXCEPTION_WHAT, ##x)

#endif //LOGGER_H


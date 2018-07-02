#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <sstream>

#include <kerbal/utility/costream.hpp>

namespace costream_ns = kerbal::utility::costream;

extern costream_ns::costream<std::cerr> ccerr;

enum class LogLevel
{
	LEVEL_FATAL = 0, LEVEL_WARNING = 1, LEVEL_INFO = 2, LEVEL_DEBUG = 3
};

template <LogLevel level>
struct Log_level_traits;

template <>
struct Log_level_traits<LogLevel::LEVEL_FATAL>
{
#	if __cplusplus < 201703L
		static constexpr const char * str = "FATAL";
#	else
		static constexpr char str[] = "FATAL";
#	endif
		static const costream_ns::costream<std::cout> outstream;
};

template <>
struct Log_level_traits<LogLevel::LEVEL_INFO>
{
#	if __cplusplus < 201703L
		static constexpr const char * str = "INFO";
#	else
		static constexpr char str[] = "INFO";
#	endif
		static const costream_ns::costream<std::cout> outstream;
};

template <>
struct Log_level_traits<LogLevel::LEVEL_WARNING>
{
#	if __cplusplus < 201703L
		static constexpr const char * str = "WARNING";
#	else
		static constexpr char str[] = "WARNING";
#	endif
		static const costream_ns::costream<std::cout> outstream;
};

template <>
struct Log_level_traits<LogLevel::LEVEL_DEBUG>
{
#	if __cplusplus < 201703L
		static constexpr const char * str = "DEBUG";
#	else
		static constexpr char str[] = "DEBUG";
#	endif
		static const costream_ns::costream<std::cout> outstream;
};

#include <boost/format.hpp>


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
	multi_args_write(log_fp, args...);
}

template <LogLevel level, typename ...T>
void log_write(int type, int job_id, const char source_filename[], int line, std::ostream & log_file, T&& ... args)
{
	if (!log_file) {
		std::cerr << "log file is not open!" << std::endl;
		return;
	}

	const time_t now = time(NULL);
	const std::string datetime = get_ymd_hms_in_local_time_zone(now);

	std::ostringstream buffer;

	boost::format templ("[%s] %s job:%d:%d [%s:%d]");
	/*           datetime logLevelStr type id srcFileName line */

	multi_args_write(buffer, templ % datetime % (const char*) Log_level_traits<level>::str % type % job_id % source_filename % line, args...);

	Log_level_traits<level>::outstream << buffer.str() << std::endl;
	log_file << buffer.str() << std::endl;

	if (log_file.fail()) { //http://www.cplusplus.com/reference/ios/ios/fail/
		std::cerr << "write error!" << std::endl;
		return;
	}
}

#ifdef LOG_DEBUG
#	undef LOG_DEBUG
#endif
#ifdef DEBUG
#	define LOG_DEBUG(type, job_id, log_fp, x...)	log_write<LogLevel::LEVEL_DEBUG>(type, job_id, __FILE__, __LINE__, log_fp, ##x)
#else
#	define LOG_DEBUG(type, job_id, log_fp, x...)
#endif

#ifdef LOG_INFO
#	undef LOG_INFO
#endif
#define LOG_INFO(type, job_id, log_fp, x...)		log_write<LogLevel::LEVEL_INFO>(type, job_id, __FILE__, __LINE__, log_fp, ##x)

#ifdef LOG_WARNING
#	undef LOG_WARNING
#endif
#define LOG_WARNING(type, job_id, log_fp, x...)	log_write<LogLevel::LEVEL_WARNING>(type, job_id, __FILE__, __LINE__, log_fp, ##x)


#ifdef LOG_FATAL
#	undef LOG_FATAL
#endif
#define LOG_FATAL(type, job_id, log_fp, x...)		log_write<LogLevel::LEVEL_FATAL>(type, job_id, __FILE__, __LINE__, log_fp, ##x)

#endif //LOGGER_H


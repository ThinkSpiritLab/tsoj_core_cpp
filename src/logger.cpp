#define _POSIX_SOURCE

#include <time.h>
#include <unistd.h>
#include <sys/file.h>

#include "logger.hpp"

const costream_ns::costream<std::cout> Log_level_traits<LogLevel::LEVEL_FATAL>::outstream(costream_ns::LIGHT_RED);
const costream_ns::costream<std::cout> Log_level_traits<LogLevel::LEVEL_INFO>::outstream(costream_ns::LAKE_BLUE);
const costream_ns::costream<std::cout> Log_level_traits<LogLevel::LEVEL_WARNING>::outstream(costream_ns::LIGHT_YELLOW);
const costream_ns::costream<std::cout> Log_level_traits<LogLevel::LEVEL_DEBUG>::outstream(costream_ns::LIGHT_PURPLE);

std::string get_ymd_hms_in_local_time_zone(time_t time) noexcept
{
	static char datetime[100];

	strftime(datetime, 99, "%Y-%m-%d %H:%M:%S", localtime(&time));

	return datetime;
}


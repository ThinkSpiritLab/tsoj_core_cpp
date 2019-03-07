/*
 * ProtectedProcess.hpp
 *
 *  Created on: 2018年12月7日
 *      Author: peter
 */

#ifndef SRC_SLAVE_PROTECTEDPROCESS_HPP_
#define SRC_SLAVE_PROTECTEDPROCESS_HPP_

#include <string>
#include <tuple>

#include <boost/filesystem.hpp>
#include <kerbal/data_struct/optional/optional.hpp>

#include "db_typedef.hpp"
#include "ExecuteArgs.hpp"

class ProtectedProcessConfig
{
	public:

		boost::filesystem::path input_path;
		boost::filesystem::path output_path;
		boost::filesystem::path error_path;

		template <typename Type>
		using optional = kerbal::data_struct::optional<Type>;

		using raw_max_cpu_time_type = ojv4::s_time_in_milliseconds;
		using max_cpu_time_type = optional<raw_max_cpu_time_type>;
		max_cpu_time_type max_cpu_time; ///< 最大 CPU 时间限制

		using raw_max_real_time_type = ojv4::s_time_in_milliseconds;
		using max_real_time_type = optional<raw_max_real_time_type>;
		max_real_time_type max_real_time; ///< 最大墙上时间

		using raw_max_memory_type = ojv4::s_mem_in_byte;
		using max_memory_type = optional<raw_max_memory_type>;
		max_memory_type max_memory; ///< 储存空间的最大字节长度

		using raw_max_stack_type = ojv4::s_mem_in_byte;
		using max_stack_type = optional<raw_max_stack_type>;
		max_stack_type max_stack; ///< 栈的最大字节长度

		using raw_max_process_number_type = int;
		using max_process_number_type = optional<raw_max_process_number_type>;
		max_process_number_type max_process_number; ///< 程序最大子进程数

		using raw_max_output_size_type = ojv4::s_mem_in_byte;
		using max_output_size_type = optional<raw_max_output_size_type>;
		max_output_size_type max_output_size; ///< 可创建的文件的最大字节长度，即为输出字节的最大长度


		ProtectedProcessConfig(const boost::filesystem::path & input_path, const boost::filesystem::path & output_path, const boost::filesystem::path & error_path) :
				input_path(input_path), output_path(output_path), error_path(error_path)
		{
		}

		/// set max cpu time
		ProtectedProcessConfig& set_max_cpu_time(const max_cpu_time_type & max_cpu_time)
		{
			this->max_cpu_time = max_cpu_time;
			return *this;
		}

		ProtectedProcessConfig& set_max_cpu_time(const raw_max_cpu_time_type & max_cpu_time)
		{
			this->max_cpu_time = max_cpu_time;
			return *this;
		}

		/// set max real time
		ProtectedProcessConfig& set_max_real_time(const max_real_time_type & max_real_time)
		{
			this->max_real_time = max_real_time;
			return *this;
		}

		ProtectedProcessConfig& set_max_real_time(const raw_max_real_time_type & max_real_time)
		{
			this->max_real_time = max_real_time;
			return *this;
		}

		/// set max memory
		ProtectedProcessConfig& set_max_memory(const max_memory_type & max_memory)
		{
			this->max_memory = max_memory;
			return *this;
		}

		ProtectedProcessConfig& set_max_memory(const raw_max_memory_type & max_memory)
		{
			this->max_memory = max_memory;
			return *this;
		}

		/// set max stack
		ProtectedProcessConfig& set_max_stack(const max_stack_type & max_stack)
		{
			this->max_stack = max_stack;
			return *this;
		}

		ProtectedProcessConfig& set_max_stack(const raw_max_stack_type & max_stack)
		{
			this->max_stack = max_stack;
			return *this;
		}

		/// set max process number
		ProtectedProcessConfig& set_max_process_number(const max_process_number_type & max_process_number)
		{
			this->max_process_number = max_process_number;
			return *this;
		}

		ProtectedProcessConfig& set_max_process_number(const raw_max_process_number_type & max_process_number)
		{
			this->max_process_number = max_process_number;
			return *this;
		}

		/// set max output size
		ProtectedProcessConfig& set_max_output_size(const max_output_size_type & max_output_size)
		{
			this->max_output_size = max_output_size;
			return *this;
		}

		ProtectedProcessConfig& set_max_output_size(const raw_max_output_size_type & max_output_size)
		{
			this->max_output_size = max_output_size;
			return *this;
		}

};

enum class ProtectedProcessResult
{
	ACCEPTED = 0,
	CPU_TIME_LIMIT_EXCEEDED = 1, ///< CPU 时间超时
	REAL_TIME_LIMIT_EXCEEDED = 2, ///< 墙上时间超时
	MEMORY_LIMIT_EXCEEDED = 3, ///< 超内存
	RUNTIME_ERROR = 4, ///< 运行时错误
	SYSTEM_ERROR = 5, ///< 系统错误
	OUTPUT_LIMIT_EXCEEDED = 6, ///< 输出文件大小超过允许值
};

std::ostream& operator<<(std::ostream& out, ProtectedProcessResult result)
{
	switch (result) {
		case ProtectedProcessResult::ACCEPTED:
			return out << "ACCEPTED";
		case ProtectedProcessResult::CPU_TIME_LIMIT_EXCEEDED:
			return out << "CPU_TIME_LIMIT_EXCEEDED";
		case ProtectedProcessResult::REAL_TIME_LIMIT_EXCEEDED:
			return out << "REAL_TIME_LIMIT_EXCEEDED";
		case ProtectedProcessResult::MEMORY_LIMIT_EXCEEDED:
			return out << "MEMORY_LIMIT_EXCEEDED";
		case ProtectedProcessResult::RUNTIME_ERROR:
			return out << "RUNTIME_ERROR";
		case ProtectedProcessResult::SYSTEM_ERROR:
			return out << "SYSTEM_ERROR";
		case ProtectedProcessResult::OUTPUT_LIMIT_EXCEEDED:
			return out << "OUTPUT_LIMIT_EXCEEDED";
	}
	return out;
}

class ProtectedProcessDetails:
		public std::tuple<
		ProtectedProcessResult,
		std::chrono::milliseconds,
		std::chrono::milliseconds,
		kerbal::utility::Byte,
		int>
{
	private:
		using supper_t = std::tuple<
				ProtectedProcessResult,
				std::chrono::milliseconds,
				std::chrono::milliseconds,
				kerbal::utility::Byte,
				int>;
	public:
		using supper_t::supper_t;

		const auto& running_result() const
		{
			return std::get<0>(*this);
		}

		const auto& real_time() const
		{
			return std::get<1>(*this);
		}

		const auto& cpu_time() const
		{
			return std::get<2>(*this);
		}

		const auto& memory() const
		{
			return std::get<3>(*this);
		}

		const auto& exit_code() const
		{
			return std::get<4>(*this);
		}
};

ProtectedProcessDetails
protected_process(const ExecuteArgs & args, const ProtectedProcessConfig & config, const ExecuteArgs & env);

class Dup2FailedExcetion: public std::runtime_error
{
	public:

		enum DupFileType
		{
			INPUT_FILE, OUTPUT_FILE, ERROR_FILE
		};

	private:

		static std::string make_exception_description(DupFileType file_type, const char * file_path)
		{
			constexpr const char * s[] = { "input file", "output file", "error file" };
			int i = 0;
			switch (file_type) {
				case DupFileType::INPUT_FILE:
					i = 0;
					break;
				case DupFileType::OUTPUT_FILE:
					i = 1;
					break;
				case DupFileType::ERROR_FILE:
					i = 2;
					break;
			}
			using namespace std::string_literals;
			return "Dup "s + s[i] + " to [" + file_path + "] failed";
		}

	public:
		const DupFileType file_type;
		const std::string file_path;

		Dup2FailedExcetion(DupFileType file_type, const char * file_path) :
			std::runtime_error(make_exception_description(file_type, file_path)), file_type(file_type), file_path(file_path)
		{
		}

		Dup2FailedExcetion(DupFileType file_type, const std::string & file_path) :
			std::runtime_error(make_exception_description(file_type, file_path.c_str())), file_type(file_type), file_path(file_path)
		{
		}

		Dup2FailedExcetion(DupFileType file_type, const boost::filesystem::path & file_path) :
			std::runtime_error(make_exception_description(file_type, file_path.c_str())), file_type(file_type), file_path(file_path.c_str())
		{
		}
};

class SetRlimitFailedException: public std::runtime_error
{
	public:
		SetRlimitFailedException() :
				std::runtime_error("set rlimit failed")
		{
		}
};

class ThreadFailedException: public std::runtime_error
{
	public:
		ThreadFailedException() :
				std::runtime_error("thread failed")
		{
		}
};


#endif /* SRC_SLAVE_PROTECTEDPROCESS_HPP_ */

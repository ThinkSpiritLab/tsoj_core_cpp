/*
 * LanguageStrategy.hpp
 *
 *  Created on: 2018年12月7日
 *      Author: peter
 */

#ifndef SRC_SLAVE_LANGUAGESTRATEGY_HPP_
#define SRC_SLAVE_LANGUAGESTRATEGY_HPP_

#include "united_resource.hpp"
#include "ProtectedProcess.hpp"

#include <boost/format.hpp>
#include <boost/filesystem.hpp>

class LanguageStrategyInterface
{
	public:

		virtual const char * default_suffix() const noexcept = 0;

		virtual
		void
		save_source_code(const boost::filesystem::path & directory, const std::string & file_name, const char * source_code)
		{
			boost::filesystem::create_directories(directory);
			boost::filesystem::path file_path = directory / (file_name + '.' + this->default_suffix());
			std::ofstream fout(file_path.native(), std::ios::out);
			fout << source_code;
		}

		virtual ProtectedProcessDetails
		compile(const char * source_file, const ProtectedProcessConfig & config) = 0;

		virtual ProtectedProcessDetails
		compile(const std::string & source_file, const ProtectedProcessConfig & config) final
		{
			return this->compile(source_file.c_str(), config);
		}

		virtual ProtectedProcessDetails
		compile(const boost::filesystem::path & source_file, const ProtectedProcessConfig & config) final
		{
			return this->compile(source_file.c_str(), config);
		}

		virtual ProtectedProcessDetails
		running(const char * binary_file, const ProtectedProcessConfig & config) = 0;

		virtual ProtectedProcessDetails
		running(const std::string & binary_file, const ProtectedProcessConfig & config) final
		{
			return this->running(binary_file.c_str(), config);
		}

		virtual ProtectedProcessDetails
		running(const boost::filesystem::path & binary_file, const ProtectedProcessConfig & config) final
		{
			return this->running(binary_file.c_str(), config);
		}

		virtual ~LanguageStrategyInterface() noexcept = default;
};

template <Language lang>
class LanguageStrategy;


template <>
class LanguageStrategy<Language::C> : public LanguageStrategyInterface
{
	public:
		virtual const char * default_suffix() const noexcept
		{
			return "c";
		}

		virtual
		ProtectedProcessDetails
		compile(const char * source_file, const ProtectedProcessConfig & config) override
		{
			static const char c_compiler[] = "/usr/bin/gcc";
			static const std::string PATH_EQUAL = "PATH=";
			ExecuteArgs args = { c_compiler, "-g", "-O2", "-Werror=main", "-std=gnu11", "-static", source_file, "-lm", "-o", "Main" };
			ExecuteArgs env = { PATH_EQUAL + getenv("PATH") };
			return protected_process(args, config, env);
		}

		virtual
		ProtectedProcessDetails
		running(const char * binary_file, const ProtectedProcessConfig & config) override
		{
			static const std::string PATH_EQUAL = "PATH=";
			ExecuteArgs env = { PATH_EQUAL + getenv("PATH") };
			return protected_process( { binary_file }, config, env);
		}
};


template <>
class LanguageStrategy<Language::Cpp> : public LanguageStrategyInterface
{
	public:
		virtual const char * default_suffix() const noexcept
		{
			return "cpp";
		}

		virtual
		ProtectedProcessDetails
		compile(const char * source_file, const ProtectedProcessConfig & config) override
		{
			static const char cpp_compiler[] = "/usr/bin/g++";
			static const std::string PATH_EQUAL = "PATH=";
			ExecuteArgs args = { cpp_compiler, "-g", "-O2", "-std=gnu++11", "-static", source_file, "-o", "Main" };
			ExecuteArgs env = { PATH_EQUAL + getenv("PATH") };
			return protected_process(args, config, env);
		}

		virtual
		ProtectedProcessDetails
		running(const char * binary_file, const ProtectedProcessConfig & config) override
		{
			static const std::string PATH_EQUAL = "PATH=";
			ExecuteArgs env = { PATH_EQUAL + getenv("PATH") };
			return protected_process( { binary_file }, config, env);
		}
};


template <>
class LanguageStrategy<Language::Cpp14> : public LanguageStrategyInterface
{
	public:
		virtual const char * default_suffix() const noexcept
		{
			return "cpp";
		}

		virtual
		ProtectedProcessDetails
		compile(const char * source_file, const ProtectedProcessConfig & config) override
		{
			static constexpr const char cpp_compiler[] = "/usr/bin/g++";
			static const std::string PATH_EQUAL = "PATH=";
			ExecuteArgs args = { cpp_compiler, "-g", "-O2", "-std=gnu++14", "-static", source_file, "-o", "Main" };
			ExecuteArgs env = { PATH_EQUAL + getenv("PATH") };
			return protected_process(args, config, env);
		}

		virtual
		ProtectedProcessDetails
		running(const char * binary_file, const ProtectedProcessConfig & config) override
		{
			static const std::string PATH_EQUAL = "PATH=";
			ExecuteArgs env = { PATH_EQUAL + getenv("PATH") };
			return protected_process( { binary_file }, config, env);
		}
};


template <>
class LanguageStrategy<Language::Java> : public LanguageStrategyInterface
{
	public:
		virtual const char * default_suffix() const noexcept
		{
			return "java";
		}

		virtual
		ProtectedProcessDetails
		compile(const char * source_file, const ProtectedProcessConfig & config) override
		{
			static const char java_compiler[] = "/usr/bin/javac";
			static boost::format java_xms("-J-Xms%dm");
			static boost::format java_xmx("-J-Xmx%dm");
			static const std::string PATH_EQUAL = "PATH=";
			ExecuteArgs args = { java_compiler, (java_xms % 64).str(), (java_xmx % 512).str(), "-encoding", "UTF-8", "-sourcepath", ".", "-d", ".", source_file };
			ExecuteArgs env = { PATH_EQUAL + getenv("PATH") };
			return protected_process(args, config, env);
		}

		virtual
		ProtectedProcessDetails
		running(const char * binary_file, const ProtectedProcessConfig & config) override
		{
			static const std::string PATH_EQUAL = "PATH=";
			ExecuteArgs env = { PATH_EQUAL + getenv("PATH") };
			return protected_process( { binary_file }, config, env);
		}

};




#endif /* SRC_SLAVE_LANGUAGESTRATEGY_HPP_ */

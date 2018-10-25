/*
 * config_exception.hpp
 *
 * Author:ZhangBinjie@Penguin
 * Created on: 2018年10月24日
 */

#ifndef CONFIG_EXCEPTION_HPP
#define CONFIG_EXCEPTION_HPP

#include <exception>
#include <string>

class ConfigException : public std::exception {
    protected:
        const std::string reason;

    public:
        ConfigException(const std::string & reason) : reason(reason) {}

        virtual const char * what() const noexcept {
            return reason.c_str();
        }
};

#endif
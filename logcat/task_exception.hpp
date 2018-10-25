/*
 * task_exception.hpp
 *
 * Author:ZhangBinjie@Penguin
 * Created on: 2018年10月25日
 */

#ifndef TASK_EXCEPTION_HPP
#define TASK_EXCEPTION_HPP

#include <exception>
#include <string>

class TaskException : public std::exception {
    protected:
        const std::string reason;

    public:
        TaskException(const std::string & reason) : reason(reason) {}

        virtual const char * what() const noexcept {
            return reason.c_str();
        }
};

#endif
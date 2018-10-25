/*
 * taskbase.hpp
 *
 * Author:ZhangBinjie@Penguin
 * Created on: 2018年10月24日
 */

#ifndef TASK_BASE_HPP
#define TASK_BASE_HPP

#include "config.hpp"

class TaskBase{
protected:
public:
    virtual void handle() = 0;
    virtual ~TaskBase() = 0;
};

std::unique_ptr<TaskBase> make_task(const Config & config);

#endif
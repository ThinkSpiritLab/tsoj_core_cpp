/*
 * helptask.hpp
 *
 * Author:ZhangBinjie@Penguin
 * Created on: 2018年10月24日
 */

#ifndef HELP_TASK_HPP
#define HELP_TASK_HPP

#include <memory>
#include "taskbase.hpp"

class HelpTask final : public TaskBase {
protected:
    friend std::unique_ptr<TaskBase> make_task(const Config & config);
    virtual void handle() override;
};

#endif
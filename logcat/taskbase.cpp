/*
 * taskbase.cpp
 *
 * Author:ZhangBinjie@Penguin
 * Created on: 2018年10月24日
 */

#include <memory>
#include "taskbase.hpp"
#include "task_exception.hpp"
#include "united_resource.hpp"
#include "helptask.hpp"
#include "versiontask.hpp"
#include "showtask.hpp"

std::unique_ptr<TaskBase> make_task(const Config & config) {
    TaskBase *task = nullptr;
    if (config.choosed_action == Action::HELPINFO) {
        task = new HelpTask();
    } else if (config.choosed_action == Action::VERSION) {
        task = new VersionTask();
    } else if (config.choosed_action == Action::SHOWLOG) {
        task = new ShowTask(config);
    } else {
        throw TaskException("Undefined or invalid action is choosed.");
    }
    return std::unique_ptr<TaskBase>(task);
}

TaskBase::~TaskBase() {}
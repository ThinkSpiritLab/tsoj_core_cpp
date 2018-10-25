/*
 * versiontask.hpp
 *
 * Author:ZhangBinjie@Penguin
 * Created on: 2018年10月25日
 */

#ifndef VERSION_TASK_HPP
#define VERSION_TASK_HPP

#include <memory>
#include "taskbase.hpp"

class VersionTask final : public TaskBase {
protected:
    friend std::unique_ptr<TaskBase> make_task(const Config & config);
    virtual void handle() override;
};

#endif
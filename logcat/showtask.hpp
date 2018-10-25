/*
 * showtask.hpp
 *
 * Author:ZhangBinjie@Penguin
 * Created on: 2018年10月25日
 */

#ifndef SHOW_TASK_HPP
#define SHOW_TASK_HPP

#include <memory>
#include <fstream>
#include "taskbase.hpp"
#include "task_exception.hpp"
#include "config.hpp"
#include "kerbal/utility/costream.hpp"

namespace KUS = kerbal::utility::costream;

class ShowTask final : public TaskBase {
private:
    const Config & config;
    KUS::costream<std::cout>datetime_stream;
    KUS::costream<std::cout>LEVEL_FATAL_stream;
    KUS::costream<std::cout>LEVEL_WARNING_stream;
    KUS::costream<std::cout>LEVEL_INFO_stream;
    KUS::costream<std::cout>LEVEL_DEBUG_stream;
    KUS::costream<std::cout>type_stream;
    KUS::costream<std::cout>id_stream;
    KUS::costream<std::cout>src_file_name_stream;
    KUS::costream<std::cout>line_stream;
    //std::unique_ptr<FILE, decltype(fclose)>in_stream;
    std::ifstream in_file_stream;
    void show_head_lines();
    void show_tail_lines();
    void show_all_lines();
    void print_colorful_string(const std::string &s);
protected:
    friend std::unique_ptr<TaskBase> make_task(const Config & config);
    explicit ShowTask(const Config & config);
    virtual void handle() override;
    virtual ~ShowTask();
};

#endif
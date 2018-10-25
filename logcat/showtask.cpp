/*
 * showtask.cpp
 *
 * Author:ZhangBinjie@Penguin
 * Created on: 2018年10月25日
 */

#include <iostream>
#include <queue>
#include "showtask.hpp"

ShowTask::ShowTask(const Config & config) : config(config), datetime_stream(config.datetime_color),
    LEVEL_FATAL_stream(config.LEVEL_FATAL_color), LEVEL_WARNING_stream(config.LEVEL_WARNING_color),
    LEVEL_INFO_stream(config.LEVEL_INFO_color), LEVEL_DEBUG_stream(config.LEVEL_DEBUG),
    type_stream(config.type_color), id_stream(config.id_color), src_file_name_stream(config.src_file_name_color),
    line_stream(config.line_color), in_file_stream(config.filename, std::ifstream::in) {}

void ShowTask::print_colorful_string(const std::string &s) {
    static int num = 0;
    if (config.number_all_lines) {
        std::cout << ++num << "  ";
    }
    if (s.find("FATAL") != s.npos) {
        LEVEL_FATAL_stream << s << std::endl;
    } else if (s.find("WARNING") != s.npos) {
        LEVEL_WARNING_stream << s << std::endl;
    } else if (s.find("INFO") != s.npos) {
        LEVEL_INFO_stream << s << std::endl;
    } else if (s.find("DEBUG") != s.npos) {
        LEVEL_DEBUG_stream << s << std::endl;
    } else {
        std::cout << s << std::endl;
    }
}

void ShowTask::show_head_lines() {
    int cnt = 0;
    std::string line;
    while (cnt++ < config.head_lines_num && std::getline(in_file_stream, line)) {
        print_colorful_string(line);
    }
}

void ShowTask::show_tail_lines() {
    std::queue<std::string> q;
    std::string line;
    while (std::getline(in_file_stream, line)) {
        q.push(line);
        if (q.size() > config.tail_lines_num) {
            q.pop();
        }
    }
    while (!q.empty()) {
        print_colorful_string(q.front());
        q.pop();
    }
}

void ShowTask::show_all_lines() {
    std::string line;
    while (std::getline(in_file_stream, line)) {
        print_colorful_string(line);
    }
}

void ShowTask::handle() {
    if (!in_file_stream.is_open()) {
        throw TaskException("can not open file: " + config.filename + ".");
    }

    if (config.alllines == true && config.head_lines_num == 0 && config.tail_lines_num == 0) {
        show_all_lines();
    } else if (config.alllines == false && config.head_lines_num != 0 && config.tail_lines_num == 0) {
        show_head_lines();
    } else if (config.alllines == false && config.head_lines_num == 0 && config.tail_lines_num != 0) {
        show_tail_lines();
    } else {
        throw TASK_EXCEPTION_HPP("invalid output requirement.");
    }
}

ShowTask::~ShowTask() {
    in_file_stream.close();
}
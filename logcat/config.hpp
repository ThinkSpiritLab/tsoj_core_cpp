/*
 * config.hpp
 *
 * Author:ZhangBinjie@Penguin
 * Created on: 2018年10月24日
 */

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <iostream>
#include "united_resource.hpp"
#include "kerbal/utility/costream.hpp"

namespace KUS = kerbal::utility::costream;

class Config {
private:
    bool check_parameters_valid();
public:
    std::string filename;
    Action choosed_action;
    unsigned long long head_lines_num;
    unsigned long long tail_lines_num;
    bool alllines;
    bool number_all_lines;
    KUS::Color_t datetime_color;
    KUS::Color_t type_color;
    KUS::Color_t id_color;
    KUS::Color_t src_file_name_color;
    KUS::Color_t line_color;
    KUS::Color_t LEVEL_FATAL_color;
    KUS::Color_t LEVEL_WARNING_color;
    KUS::Color_t LEVEL_INFO_color;
    KUS::Color_t LEVEL_DEBUG;
    
    Config();
    void load_parameters(int argc, const char * argv[]);                     
};

#endif
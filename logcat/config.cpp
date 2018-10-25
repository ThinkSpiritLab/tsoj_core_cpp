/*
 * config.cpp
 *
 * Author:ZhangBinjie@Penguin
 * Created on: 2018年10月24日
 */

#include <config.hpp>
#include <set>
#include "config_exception.hpp"

namespace parameter_kind {
    const std::set<std::string>unary_parameter = {"--help", "-H", "--version", "-V", "-n", "-a"};
    const std::set<std::string>ninary_parameter = {"-h", "-t"};
}

Config::Config() :choosed_action(Action::NONCHOOSE), head_lines_num(0), tail_lines_num(50), alllines(false),
    number_all_lines(false), datetime_color(KUS::WHITE), type_color(KUS::WHITE), id_color(KUS::WHITE),
    src_file_name_color(KUS::LAKE_BLUE), line_color(KUS::LAKE_BLUE), LEVEL_FATAL_color(KUS::RED),
    LEVEL_WARNING_color(KUS::PURPLE), LEVEL_INFO_color(KUS::YELLOW), LEVEL_DEBUG(KUS::GREEN) {}

bool Config::check_parameters_valid() {
    if ((alllines == true && head_lines_num == 0 && tail_lines_num == 0) ||
        (alllines == false && head_lines_num != 0 && tail_lines_num == 0) ||
        (alllines == false && head_lines_num == 0 && tail_lines_num != 0)) {
        return true;
    }
    return false;    
}

void Config::load_parameters(int argc, const char * argv[]) {
    if (argc == 1) {
        throw ConfigException("do not have enough parameters !");
    }

    std::string key;
    std::string val;
    unsigned long long valnum;

    key = argv[1];
    if (key[0] != '-') {
        filename = key;
        choosed_action = Action::SHOWLOG;
    } else if (parameter_kind::unary_parameter.count(key)) {
        if (key == "--help" || key == "-H") {
            choosed_action = Action::HELPINFO;
        } else if (key == "--version" || key == "-V") {
            choosed_action = Action::VERSION;
        }
    } else {
        throw ConfigException("invalid parameters !");
    }
    
    for (int i = 2; i < argc; i++) {
        key = argv[i];
        if (parameter_kind::unary_parameter.count(key)) {
            if (key == "-a") {
                alllines = true;
                tail_lines_num = 0;
                head_lines_num = 0;
            } else if (key == "-n") {
                number_all_lines = true;
            }
        } else if (parameter_kind::ninary_parameter.count(key)) {
            i++;
            if (i < argc) {
                val = argv[i];
                try {
                    valnum = std::stoull(val);
                    if (key == "-h") {
                        head_lines_num = valnum;
                        tail_lines_num = 0;
                        alllines = false;
                    } else if (key == "-t") {
                        tail_lines_num = valnum;
                        head_lines_num = 0;
                        alllines = false;
                    }
                } catch (...) {
                    throw ConfigException("invalid use of " + key + " !");
                }  
            } else {
                throw ConfigException("lack parameter for " + key + " !");
            }
        } else {
            throw ConfigException("invalid parameters !");
        }    
    }

    if (check_parameters_valid() == false) {
        throw ConfigException("invalid parameters !");
    }
}
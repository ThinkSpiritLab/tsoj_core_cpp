/*
 * helptask.cpp
 *
 * Author:ZhangBinjie@Penguin
 * Created on: 2018年10月24日
 */

#include <iostream>
#include "helptask.hpp"

void HelpTask::handle() {
    std::cout << "Usage: ./logcat <tools> | <file {options}>" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "<tools>:" << std::endl;
    std::cout << "  --help or -H            show help manual." << std::endl;
    std::cout << "  --version or -V         show logcat version." << std::endl;
    std::cout << "" << std::endl;
    std::cout << "file:                     log file name." << std::endl;
    std::cout << "" << std::endl;
    std::cout << "{options}:" << std::endl;
    std::cout << "  -n                      number all output lines" << std::endl;
    std::cout << "  -a                      show all lines." << std::endl;
    std::cout << "                          (the default setting is to show last 50 lines)" << std::endl;
    std::cout << "  -h num                  show num lines in head." << std::endl;
    std::cout << "  -t num                  show num lines in tail." << std::endl;
}
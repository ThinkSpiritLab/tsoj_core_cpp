/*
 * ExecuteArgs.hpp
 *
 *  Created on: 2018年7月1日
 *      Author: peter
 */

#ifndef SRC_EXECUTEARGS_HPP_
#define SRC_EXECUTEARGS_HPP_

#include <vector>
#include <string>
#include <memory>

/**
 * @brief 执行 exec 族的命令行参数。在原本的 Unix 要求中，exec 族函数的命令行参数末尾必须以一个空指针结尾，
 * 这显然十分丑陋不够优雅，也晦涩难读。基于此目的，本处使用了一个类将它封装了起来。
 */
class ExecuteArgs
{
	private:
		std::vector<std::string> args;

	public:
		/**
		 * @brief 普通构造函数，一个空列表
		 */
		ExecuteArgs();

		/**
		 * @brief 模板式构造函数，根据参数类型生产对应的构造函数。如 vector 等。
		 * 将其内部内容复制到本类的 args 中作为参数
		 */
		template<typename ForwardInteger>
		ExecuteArgs(ForwardInteger begin, ForwardInteger end) :
				args(begin, end)
		{
		}

		/**
		 * @brief 适配初始化列表样式的构造函数
		 */
		ExecuteArgs(std::initializer_list<std::string> list);

		/**
		 * @brief 重载 = 运算符，以赋值操作更新 args
		 * @return 新的 ExecuteArgs 的引用
		 */
		ExecuteArgs& operator=(std::initializer_list<std::string> list);

		/**
		 * @brief 返回命令行参数列表
		 * @return 指向 char * 数组的指针，符合 Unix 的 exec 族函数的参数规范
		 */
		std::unique_ptr<char*[]> getArgs() const;
};

#endif /* SRC_EXECUTEARGS_HPP_ */

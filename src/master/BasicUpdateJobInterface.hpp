/*
 * BasicUpdateJobInterface.hpp
 *
 *  Created on: 2019年3月11日
 *      Author: peter
 */

#ifndef SRC_MASTER_BASICUPDATEJOBINTERFACE_HPP_
#define SRC_MASTER_BASICUPDATEJOBINTERFACE_HPP_

#include "UpdateJobInterface.hpp"

class BasicUpdateJobInterface : virtual public UpdateJobBase
{
	public:
		BasicUpdateJobInterface()
		{
			// TODO Auto-generated constructor stub
			
		}

		/**
		 * @brief 将提交代码更新至 mysql
		 * @warning 仅规定 update source_code 表的接口, 具体操作需由子类实现
		 */
		virtual void update_source_code(mysqlpp::Connection & mysql_conn) = 0;

};

#endif /* SRC_MASTER_BASICUPDATEJOBINTERFACE_HPP_ */

//
// Created by peter on 18-9-4.
//

#ifndef CORE_MYSQL_EMPTY_RES_EXCEPTION_HPP
#define CORE_MYSQL_EMPTY_RES_EXCEPTION_HPP

#include <stdexcept>

class MysqlEmptyResException : public std::logic_error
{
	protected:
		typedef std::logic_error base;

		int _errnum;

	public:
		MysqlEmptyResException(int errnum, const char * errstr) : base(errstr), _errnum(errnum)
		{
		}

		int errnum() const
		{
			return _errnum;
		}
};


#endif //CORE_MYSQL_EMPTY_RES_EXCEPTION_HPP

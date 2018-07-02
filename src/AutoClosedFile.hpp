/*
 * AutoClosedFile.hpp
 *
 *  Created on: 2018年6月8日
 *      Author: peter
 */

#ifndef SRC_AUTOCLOSEDFILE_HPP_
#define SRC_AUTOCLOSEDFILE_HPP_

#include <memory>
#include <cstdio>

class AutoClosedFile: public std::unique_ptr<FILE, int (*)(FILE *)>
{
	protected:
		typedef std::unique_ptr<FILE, int (*)(FILE *)> supper_t;

	public:
		AutoClosedFile(const std::string & fileName, const std::string & mode) :
				supper_t(std::fopen(fileName.c_str(), mode.c_str()), std::fclose)
		{
		}

		int fgetc()
		{
			return std::fgetc(get());
		}

		int fgetNextNoSpace()
		{
			int c;
			do {
				c = this->fgetc();
			} while (std::isspace(c));
			return c;
		}
};



#endif /* SRC_AUTOCLOSEDFILE_HPP_ */

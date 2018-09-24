/*
 * compare.cpp
 *
 *  Created on: 2018年7月1日
 *      Author: peter
 */

#include "compare.hpp"

#include <cstdio>
#include <memory>

void find_next_nonspace(int &c1, int &c2,
		std::unique_ptr<FILE, int (*)(FILE*)> & f1,
		std::unique_ptr<FILE, int (*)(FILE*)> & f2, UnitedJudgeResult & ret)
{
	// Find the next non-space character or \n.
	while ((isspace(c1)) || (isspace(c2))) {
		if (c1 != c2) {
			if (c2 == EOF) {
				do {
					c1 = fgetc(f1.get());
				} while (isspace(c1));
				continue;
			} else if (c1 == EOF) {
				do {
					c2 = fgetc(f2.get());
				} while (isspace(c2));
				continue;

			} else if ((c1 == '\r' && c2 == '\n')) {
				c1 = fgetc(f1.get());
			} else if ((c2 == '\r' && c1 == '\n')) {
				c2 = fgetc(f2.get());
			} else {
				ret = UnitedJudgeResult::PRESENTATION_ERROR;
			}
		}
		if (isspace(c1)) {
			c1 = fgetc(f1.get());
		}
		if (isspace(c2)) {
			c2 = fgetc(f2.get());
		}
	}
}

UnitedJudgeResult compare(const char *file1, const char *file2)
{
	std::unique_ptr<FILE, int (*)(FILE*)> f1(fopen(file1, "re"), fclose);
	std::unique_ptr<FILE, int (*)(FILE*)> f2(fopen(file2, "re"), fclose);
	if (f1 == nullptr || f2 == nullptr) {
		return UnitedJudgeResult::SYSTEM_ERROR;
	}

	UnitedJudgeResult ret = UnitedJudgeResult::ACCEPTED;
	int c1, c2;
	while (true) {
		// Find the first non-space character at the beginning of line.
		// Blank lines are skipped.
		c1 = fgetc(f1.get());
		c2 = fgetc(f2.get());
		find_next_nonspace(c1, c2, f1, f2, ret);
		// Compare the current line.
		while (true) {
			// Read until 2 files return a space or 0 together.
			while ((!isspace(c1) && c1) || (!isspace(c2) && c2)) {
				if (c1 == EOF && c2 == EOF) {
					return ret;
				}
				if (c1 == EOF || c2 == EOF) {
					break;
				}
				if (c1 != c2) {
					// Consecutive non-space characters should be all exactly the same
					return UnitedJudgeResult::WRONG_ANSWER;
				}
				c1 = fgetc(f1.get());
				c2 = fgetc(f2.get());
			}
			find_next_nonspace(c1, c2, f1, f2, ret);
			if (c1 == EOF && c2 == EOF) {
				return ret;
			}
			if (c1 == EOF || c2 == EOF) {
				return UnitedJudgeResult::WRONG_ANSWER;
			}

			if ((c1 == '\n' || !c1) && (c2 == '\n' || !c2)) {
				break;
			}
		}
	}
	return ret;
}


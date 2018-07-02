/*
 * compare.cpp
 *
 *  Created on: 2018年7月1日
 *      Author: peter
 */

#include "compare.hpp"
#include <fstream>

#include <iostream>

std::string get_filename_from_path(const std::string & path)
{
	size_t it = path.find_last_of('/');
	if (it != std::string::npos) {
		return path.data() + it + 1;
	}
	return path;
}
//
//void find_next_nonspace(int & c1, int & c2, std::ifstream & f1, std::ifstream & f2, UnitedJudgeResult &ret)
//{
//	// Find the next non-space character or \n.
//	while ((isspace(c1)) || (isspace(c2))) {
//		if (c1 != c2) {
//			if (c2 == EOF) {
//				do {
//					c1 = f1.get();
//				} while (isspace(c1));
//				continue;
//			} else if (c1 == EOF) {
//				do {
//					c2 = f2.get();
//				} while (isspace(c2));
//				continue;
//
//			} else if ((c1 == '\r' && c2 == '\n')) {
//				c1 = f1.get();
//			} else if ((c2 == '\r' && c1 == '\n')) {
//				c2 = f2.get();
//			} else {
//				ret = UnitedJudgeResult::PRESENTATION_ERROR;
//			}
//		}
//
//		if (isspace(c1)) {
//			c1 = f1.get();
//		}
//		if (isspace(c2)) {
//			c2 = f2.get();
//		}
//	}
//}
//
//UnitedJudgeResult compare(const std::string & file1, const std::string & file2)
//{
//	UnitedJudgeResult ret = UnitedJudgeResult::ACCEPTED;
//	int c1, c2;
//
//	std::ifstream f1(file1, std::ios::in); //re
//	std::ifstream f2(file1, std::ios::in); //re
//
//	if (!f1 || !f2) {
//		return UnitedJudgeResult::SYSTEM_ERROR;
//	}
//
//	for (; true;) {
//		// Find the first non-space character at the beginning of line.
//		// Blank lines are skipped.
//		c1 = f1.get();
//		c2 = f2.get();
//		find_next_nonspace(c1, c2, f1, f2, ret);
//		// Compare the current line.
//		for (; true;) {
//			// Read until 2 files return a space or 0 together.
//			while ((!isspace(c1) && c1) || (!isspace(c2) && c2)) {
//				if (c1 == EOF && c2 == EOF) {
//					return ret;
//				}
//				if (c1 == EOF || c2 == EOF) {
//					break;
//				}
//				if (c1 != c2) {
//					// Consecutive non-space characters should be all exactly the same
//					return UnitedJudgeResult::WRONG_ANSWER;
//				}
//				c1 = f1.get();
//				c2 = f2.get();
//			}
//			find_next_nonspace(c1, c2, f1, f2, ret);
//			if (c1 == EOF && c2 == EOF) {
//				return ret;
//			}
//			if (c1 == EOF || c2 == EOF) {
//				return UnitedJudgeResult::WRONG_ANSWER;
//			}
//
//			if ((c1 == '\n' || !c1) && (c2 == '\n' || !c2)) {
//				break;
//			}
//		}
//	}
//	return ret;
//}


void find_next_nonspace(int *c1, int *c2, FILE *f1, FILE *f2, UnitedJudgeResult *ret) {
    // Find the next non-space character or \n.
    while ((isspace(*c1)) || (isspace(*c2))) {
        if (*c1 != *c2) {
            if (*c2 == EOF) {
                do {
                    *c1 = fgetc(f1);
                } while (isspace(*c1));
                continue;
            } else if (*c1 == EOF) {
                do {
                    *c2 = fgetc(f2);
                } while (isspace(*c2));
                continue;

            } else if ((*c1 == '\r' && *c2 == '\n')) {
                *c1 = fgetc(f1);
            } else if ((*c2 == '\r' && *c1 == '\n')) {
                *c2 = fgetc(f2);
            } else {
                *ret = UnitedJudgeResult::PRESENTATION_ERROR;
            }
        }
        if (isspace(*c1)) {
            *c1 = fgetc(f1);
        }
        if (isspace(*c2)) {
            *c2 = fgetc(f2);
        }
    }
}

UnitedJudgeResult compare(const char *file1, const char *file2) {
	UnitedJudgeResult ret = UnitedJudgeResult::ACCEPTED;
    int c1, c2;
    FILE * f1, *f2;
    f1 = fopen(file1, "re");
    f2 = fopen(file2, "re");
    if (!f1 || !f2) {
        ret = UnitedJudgeResult::SYSTEM_ERROR;
    } else
        for (;;) {
            // Find the first non-space character at the beginning of line.
            // Blank lines are skipped.
            c1 = fgetc(f1);
            c2 = fgetc(f2);
            find_next_nonspace(&c1, &c2, f1, f2, &ret);
            // Compare the current line.
            for (;;) {
                // Read until 2 files return a space or 0 together.
                while ((!isspace(c1) && c1) || (!isspace(c2) && c2)) {
                    if (c1 == EOF && c2 == EOF) {
                        goto end;
                    }
                    if (c1 == EOF || c2 == EOF) {
                        break;
                    }
                    if (c1 != c2) {
                        // Consecutive non-space characters should be all exactly the same
                        ret = UnitedJudgeResult::WRONG_ANSWER;
                        goto end;
                    }
                    c1 = fgetc(f1);
                    c2 = fgetc(f2);
                }
                find_next_nonspace(&c1, &c2, f1, f2, &ret);
                if (c1 == EOF && c2 == EOF) {
                    goto end;
                }
                if (c1 == EOF || c2 == EOF) {
                    ret = UnitedJudgeResult::WRONG_ANSWER;
                    goto end;
                }

                if ((c1 == '\n' || !c1) && (c2 == '\n' || !c2)) {
                    break;
                }
            }
        }
    end:

    if (f1)
        fclose(f1);
    if (f2)
        fclose(f2);
    return ret;
}


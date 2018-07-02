#ifndef JUDGER_SECCOMP_RULES_H
#define JUDGER_SECCOMP_RULES_H

#include "../Config.hpp"
#include "../united_resource.hpp"

RunnerError c_cpp_seccomp_rules(const Config & _config);
RunnerError general_seccomp_rules(const Config & _config);

#endif //JUDGER_SECCOMP_RULES_H

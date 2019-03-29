
CONFIGURATION := debug
COMPILER := g++
COMPILER_NAME := "GNU C++ Compiler"
COMPILE_OPTION := "-std=c++17 -O2 -Wall -DDEBUG -g3 -c -fmessage-length=0 -MMD -MP"
LINKER := g++
LINKER_NAME := "GNU C++ Linker"
SLAVE_LINK_OPTION := " \
				-lseccomp \
				-lhiredis \
				-lboost_filesystem \
				-pthread"
MASTER_LINK_OPTION := " \
				-lhiredis \
				-lmysqlclient \
				../../dependence/libmysqlpp.3.2.4.a \
				../../dependence/libboost_filesystem.1.69.0.a \
                /usr/lib/gcc/x86_64-linux-gnu/8/libstdc++.a \
                -pthread"

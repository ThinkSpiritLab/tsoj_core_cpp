
CONFIGURATION := release
COMPILER := g++
COMPILER_NAME := "GNU C++ Compiler"
COMPILE_OPTION := " \
				-std=c++17 \
				-O2 -Wall \
				-I\"$(shell pwd)/project_home/dependence/include\" \
				-c \
				-fmessage-length=0 \
				-MMD \
				-MP"
LINKER := g++
LINKER_NAME := "GNU C++ Linker"
SLAVE_LINK_OPTION := " \
				-lseccomp \
				-lhiredis \
				$(shell pwd)/project_home/dependence/libboost_filesystem.1.69.0.a \
				-pthread"
MASTER_LINK_OPTION := " \
				-lhiredis \
				-lmysqlclient \
				$(shell pwd)/project_home/dependence/libmysqlpp.3.2.4.a \
				$(shell pwd)/project_home/dependence/libboost_filesystem.1.69.0.a \
                -static-libstdc++ \
                -pthread"

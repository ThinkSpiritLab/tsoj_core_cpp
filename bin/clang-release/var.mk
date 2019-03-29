
CONFIGURATION := release
COMPILER := clang++
COMPILER_NAME := "Clang++ Compiler"
COMPILE_OPTION := " \
				-std=c++17 \
				-O2 -Wall \
				-I\"$(shell pwd)/project_home/dependence/include\" \
				-c \
				-fmessage-length=0 \
				-MMD \
				-MP"
LINKER := clang++
LINKER_NAME := "Clang++ Linker"
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

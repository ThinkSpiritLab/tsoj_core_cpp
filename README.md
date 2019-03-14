
[![Platform](https://img.shields.io/badge/platform-linux-blue.svg)]()
[![Language](https://img.shields.io/badge/language-C%2B%2B17-blue.svg)]()
[![Documentation](https://img.shields.io/badge/docs-doxygen-blue.svg)](https://wentsingnee.github.io/tsoj_core_cpp)
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)]()

#### 项目描述:
TSOJ v4 C++ 版评测内核.

#### 一些问答:
为什么要用 C++ 重构？

* C 语言版评测内核暴露出很严重的内存泄露及缓冲区溢出等问题. 即使现在我们能做到质量可控, 但无法保证未来会有具有足够安全素养的学弟接手. 使用 C++ 改写以后, 不必再担心资源回收的问题, 开发更加方便, 也更易书写出质量较高的代码.
* C 语言版评测内核 updater 部分代码逻辑混乱, 不成体系. 各模块耦合严重因此无法进行单元测试. 多次发生增加功能出现线上 bug 的事故.
* C 语言版评测内核未考虑高并发下 update 乱序导致提交数通过数错误的问题, C++ 版内核已修复此 bug.
* C 语言版评测内核不方便做多线程异步 update. C++ 版内核现已支持多线程异步处理 update 业务, 使得 update 效率提高了 60 倍以上.

#### 语言及编译器要求:
[![Language](https://img.shields.io/badge/language-C%2B%2B17-blue.svg)]() 编译器需支持 C++17 及以上标准.
[![Build Cmake Test](https://img.shields.io/badge/build-cmake%20%28still%20on%20test%29-orange.svg)]() 正在实验性支持通过 cmake 产生 Makefile 文件.
[![Build GCC](https://img.shields.io/badge/build-gcc%208.2%20pass-green.svg)]() gcc-8.2.0 下通过编译.
[![Build Clang](https://img.shields.io/badge/build-clang%206.0%20pass-green.svg)]() clang++-6.0 下通过编译.

#### 编译时依赖:
* **共用**
	* [![Library Dependence](https://img.shields.io/badge/boost-v1.69.0-blue.svg)](https://www.boost.org/) 可使用 `sudo apt install libboost-dev` 完成安装. 下面列举一些使用到的重要子库:
		* [![format](https://img.shields.io/badge/boost.format-blue.svg)](https://www.boost.org/doc/libs/1_69_0/libs/format/doc/format.html) 类型及长度安全的 C++ 格式化库. 可取代 sprintf 减少缓冲区越界风险.
		* [![filesystem](https://img.shields.io/badge/boost.filesystem-blue.svg)](https://www.boost.org/doc/libs/1_69_0/libs/filesystem/doc/index.htm) (仅 JudgeServer 使用) 提供 C++ 风格的文件系统 API, 比 UNIX API 更方便使用.
	* [![Library Dependence](https://img.shields.io/badge/cmdline-Latest-blue.svg)](https://github.com/tanakh/cmdline) C++ 命令行解析库.
	* [![Library Dependence](https://img.shields.io/badge/nlohmann%20json-v3.5.0-blue.svg)](https://github.com/nlohmann/json/releases/tag/v3.5.0) C++ JSON 解析库.
	* [![Library Dependence](https://img.shields.io/badge/hiredis-v0.13.3--2.2-blue.svg)](https://github.com/redis/hiredis/releases/tag/v0.13.3)  C 语言 redis 连接库. 可使用 `sudo apt install libhiredis-dev` 完成安装, 内核编译链接时需加 -lhiredis 选项.
	* [![Library Dependence](https://img.shields.io/badge/Kerbal-Latest-blue.svg)](https://github.com/WentsingNee/Kerbal) 项目地址:  [https://github.com/WentsingNee/Kerbal](https://github.com/WentsingNee/Kerbal)  或 [https://dev.tencent.com/u/WentsingNee/p/Kerbal](https://dev.tencent.com/u/WentsingNee/p/Kerbal) (双远程仓库). 无需编译即可使用, 编译评测内核时只需提供 kerbal/include 文件夹的地址作为包含文件的搜索目录. 下面列举一些使用到的重要子库:
		* [![redis_v2](https://img.shields.io/badge/redis_v2-blue.svg)]() 提供 C++ 风格的 redis 操作类及函数.
		* [![utility.costream](https://img.shields.io/badge/utility.costream-blue.svg)]() 服务于日志模块, 提供输出彩色文字的功能.
		* [![utility.memory_storage](https://img.shields.io/badge/utility.memory_storage-blue.svg)]() 提供统一内存大小计量衡.
        
* **updater**
	* [![Library Dependence](https://img.shields.io/badge/MySQL++-≥%20v3.2.4-blue.svg)](https://tangentsoft.com/mysqlpp/home) C++ MySQL 连接库.

* **JudgeServer**
	* [![Library Dependence](https://img.shields.io/badge/seccomp-v2.3.1--2.1ubuntu4-blue.svg)]()  为评测代码的编译及运行提供安全机制的库. 可使用 `sudo apt install libseccomp-dev` 完成安装. JudgeServer 编译链接时需加 -lseccomp 选项.


#### 运行时依赖:
* **JudgeServer**
	* [![Runtime Dependence](https://img.shields.io/badge/similarity%20tester-v3.0-blue.svg)](https://github.com/lambdafu/similarity-tester) 提供代码查重功能的库. 只需在运行 JudgeServer 的环境上配置 sim. Updater 端不需配置 sim.


#### 项目描述:
    - TSOJ v4 C++ 版评测内核

#### 一些问答:
    - 为什么要用 C++ 重构？
		> C 语言版评测内核暴露出很严重的内存泄露及缓冲区溢出等问题. 即使现在我们能做到质量可控, 但无法保证未来会有具有足够安全素养的学弟接手. 使用 C++ 改写以后, 不必再担心资源回收的问题, 开发更加方便, 也更易书写出质量较高的代码

		> C 语言版评测内核代码逻辑混乱, 不成体系. 增加功能已出过多次线上 bug
		
		> C 语言版评测内核未考虑高并发下 update 乱序导致提交数通过数错误的问题, C++ 版内核已修复此 bug
		
		> C 语言版评测内核不方便做多线程异步 update, 使用 C++ 会更方便地处理这里的业务逻辑, 评测效率会有更大的改善

#### 语言及编译器要求:
    - 支持 C++11 及以上标准的编译器
    - g++-5, 6.3, 8.1, 8.3, clang++-6.0 下均测试通过

#### 编译时所依赖的库:
    - hiredis:  C语言的 redis 连接库, 链接时需加 -lhiredis 选项
    - seccomp:  为评测代码的编译及运行提供安全机制的库, 链接时需加 -lseccomp 选项
    - boost.format:  取代 sprintf 减少缓冲区越界风险. sudo apt install libboost-dev 安装完成即可, 无需链接即可使用
    - Kerbal 项目地址:  https://git.coding.net/WentsingNee/Kerbal.git (内核项目部分分支下 Kerbal 已成为一个子模块), 无需编译即可使用, 编译评测内核时只需提供 kerbal/include 文件夹的地址作为包含文件的搜索目录
        - redis 子库:  提供 C++ 风格的 redis 操作类及函数
        - utility.costream 子库:  彩色输出流
        - utility.memory_storage 子库:  统一内存大小计量衡

#### 运行时所依赖的库:
    - sim: 提供了代码查重的功能. judge 端与 update 端实现分离以后, 只需在 judge 端上配置. update 端不需要配置 sim

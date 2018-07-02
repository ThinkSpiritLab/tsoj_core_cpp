所依赖的库：
  - hiredia C语言的 redis 连接库
  - seccomp 为评测代码的编译及运行提供安全机制的库
  - boost::format 取代 sprintf 减少缓冲区越界风险
  - kerbal 项目地址 https://git.coding.net/WentsingNee/Kerbal.git 
	-- redis 子库 提供 C++ 风格的 redis 操作类及函数
    -- utility.costream子库 彩色输出流
    -- utility.memory_storage 统一内存大小计量衡
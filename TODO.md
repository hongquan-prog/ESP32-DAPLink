1、编写CMAKE自动扫描algorithm目录，将该文件夹下的FLM文件通过脚本转换成C语言代码，并将生成的代码添加到构建中，生成的代码中通过模块自动初始化机制将每个算法中的数据结构放入指定的段中，系统启动后，从该段中自动加载可用算法。

2、编写一个状态机，该状态机用于执行烧录相关的动作，烧录有两种模式，第一种是在线烧录模式，过程类似于GDB控制DAPLink进行烧录，不会存储固件到Flash中，第二种是是文件烧录模式，可以通过指定烧录算法和程序，可以控制其进行自动烧录。

3、添加配网功能，station模式下如果未连接到网络每间隔30S扫描并尝试连接一次，如果未扫描到则休眠。

在线烧录请求：
{
    "program_mode": "online",
    "flash_addr":0x8000000,
    "ram_addr":0x2000000,
    "algorithm":"xxxxx"
}

文件烧录请求：
{
    "program_mode": "offline",
    "flash_addr":0x8000000,
    "ram_addr":0x2000000,
    "algorithm":"xxxxx",
    "program":"xxxxx"
}


# ESP32-DAPLink Program 模块代码规范

## 1. 文件头注释

所有源文件使用标准文件头：

```c
/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   Initial version
 * 2026-3-16     Refactor     Improved interface documentation
 */
```

- 头文件使用 `#pragma once` 而非 `#ifndef` 保护
- 第三方代码保留原始版权声明

## 2. 命名规范

### 2.1 类名

- **大驼峰命名** (PascalCase)
- 示例：`TargetFlash`, `FlashAccessor`, `StreamProgrammer`

### 2.2 成员变量

- **私有和保护变量下划线前缀 + 蛇形命名** (`_member_name`)，公有变量使用蛇形命名无下划线
- 示例：`_swd`, `_flash_cfg`, `_current_sector_addr`
- 常量成员使用 `static constexpr`：`static constexpr uint32_t _page_size = 1024;`


### 2.3 局部变量命名

- **全小写 + 下划线** (snake_case)
- 示例：`write_size`, `flash_algo`, `current_sector_addr`
- 静态局部变量使用 `s_` 前缀：`s_buffer`, `s_initialized`
- 全局变量使用 `g_` 前缀：`g_state`, `g_config`

### 2.4 函数命名

- **蛇形命名** (snake\_case)
- **模块前缀**：函数应使用模块名作为前缀，避免命名冲突，便于识别所属模块
  - 格式：`模块名_功能描述()`
  - 示例：`flash_init()`, `uart_send()`, `tcp_connect()`
- **动词开头**：函数名应以动词开头，表明执行的操作
  - 常见动词前缀：`init_`, `deinit_`, `create_`, `destroy_`, `get_`, `set_`, `is_`, `has_`, `read_`, `write_`, `enable_`, `disable_`, `start_`, `stop_`
- 私有和保护辅助函数同样使用蛇形命名，前缀添加下划线 `_`：`_validate_address()`, `_calculate_checksum()`

**示例：**

```c
// flash 模块 - 公共接口
void flash_init(void);
int flash_read(uint32_t addr, uint8_t *buf, size_t len);
int flash_write(uint32_t addr, const uint8_t *data, size_t len);
void flash_deinit(void);

// uart 模块 - 公共接口
void uart_init(uint32_t baudrate);
int uart_send(const uint8_t *data, size_t len);
int uart_receive(uint8_t *buf, size_t len);
bool uart_is_ready(void);

// 私有/内部函数
static int _validate_checksum(const uint8_t *data);
static void _process_internal_state(void);
```

### 2.5 类型定义

- **后缀** **`_t`**
- `typedef enum { ... } err_t;`
- `typedef struct { ... } target_cfg_t;`
- 结构体和枚举定义在类内部时使用 `typedef`

### 2.6 宏定义

- **全大写 + 下划线**
- 示例：`ERR_NONE`, `FLASH_STATE_CLOSED`, `LOG_COLOR_RED`
- 宏函数：`ROUND_UP(value, boundary)`

### 2.7 枚举值

- **全大写 + 下划线前缀**
- 示例：`ERR_NONE`, `ERR_FAILURE`, `FLASH_FUNC_NOP`
- 枚举名使用 `_def` 后缀：`enum transfer_err_def`

## 3. 文件组织

```
components/Program/
├── inc/           # 头文件目录
│   └── *.h
├── src/           # 源文件目录
│   ├── *.cpp
│   └── *.c
└── CMakeLists.txt
```

### 3.1 头文件结构

**C 语言头文件：**

```c
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 函数简要描述
 */
void module_init(void);

#ifdef __cplusplus
}
#endif
```

**C++ 头文件：**

```cpp
#pragma once

#include <cstdint>
#include "flash_iface.h"

/**
 * @brief 类/函数简要描述
 *
 * 详细描述...
 */
class ClassName
{
private:
    // 私有成员

public:
    // 公共接口
};
```

### 3.2 源文件结构

```cpp
#include "target_flash.h"   // 对应头文件在前
#include "log.h"            // 项目头文件
#include <cstring>          // 标准库在后

#define TAG "target_flash"  // 日志标签定义

// 实现代码...
```

## 4. 文档注释

使用 Doxygen 风格注释：

```cpp
/**
 * @brief 简要描述
 * @param param_name 参数说明
 * @return 返回值说明
 */
```

类成员使用行尾注释：

```cpp
SWDIface *_swd;                        ///< SWD interface instance
const target_cfg_t *_flash_cfg;        ///< Flash configuration
```

<br />

## 5. 代码风格

### 5.1 缩进

- 使用 **4 空格缩进**，不使用 Tab

### 5.2 花括号风格

- **左括号独立成行**（Allman 风格）
- 适用于函数定义、类定义、控制语句等所有场景
- **右括号** **`}`** **后必须空一行**，除非后面紧跟 `else`、`while`（do-while）或 `}` 自身

```cpp
// 函数定义
void example_function(void)
{
    // code
}

// 类定义
class ExampleClass
{
public:
    int value;
};

// 控制语句 - if/else
if (condition)
{
    do_something();
}
else
{
    do_other();
}

// 控制语句 - 右括号后空一行
if (value > 0)
{
    process(value);
}

next_function();

// 循环语句
for (int i = 0; i < 10; i++)
{
    process(i);
}

// do-while 例外：右括号后紧跟 while
do
{
    update();
}
while (running);

// switch 语句
switch (state)
{
case STATE_A:
    handle_a();
    break;

case STATE_B:
    handle_b();
    break;

default:
    handle_default();
    break;
}
```

### 5.3 空行规则

- **代码行之间不留空行**，保持紧凑
- 空行用于以下场景：
  - 右括号 `}` 后（除非紧跟 `else`、`while` 等）
  - 函数之间
  - 独立的语句块之间（如多个 `if` 语句、独立的逻辑步骤）
  - 变量定义块与代码之间
  - **return 语句前**

**重要规则：右括号 `}` 后的处理**

- `}` 后紧跟 `else`、`while`（do-while）或 `}` 自身时，**不空行**
- `}` 后还有其他代码时，**必须空一行**

**推荐写法：**

```cpp
void example_function(void)
{
    int value = 0;
    int result = calculate(value);

    if (result > 0)
    {
        process(result);
    }

    if (value > 10)
    {
        handle_large(value);
    }

    cleanup();
}
```

**不推荐写法：**

```cpp
void example_function(void)
{
    int value = 0;

    int result = calculate(value);

    if (result > 0)
    {

        process(result);

    }

    cleanup();
}
```

### 5.4 局部变量定义

- **局部变量统一定义在函数开头**
- **尽量在定义时初始化**，包括使用函数调用初始化
- 变量定义块与后续代码之间**必须空一行隔开**
- 变量按类型分组，同类型变量放在一起

**推荐写法：**

```cpp
void example_function(void)
{
    int value = get_input();
    int result = calculate(value);
    char buffer[64] = {0};

    /* 开始处理逻辑 */
    if (result > 0)
    {
        process(result);
    }
}

void another_function(int param)
{
    /* 局部变量定义 */
    int flag = fcntl(sockfd, F_GETFL, 0);
    int count = param * 2;
    bool success = false;

    /* 设置非阻塞模式 */
    if (flag < 0)
    {
        return;
    }

    if (fcntl(sockfd, F_SETFL, flag | O_NONBLOCK) < 0)
    {
        return;
    }
}
```

### 5.5 空格规则

- 关键字后加空格：`if (`, `for (`, `while (`
- 运算符两侧加空格：`a + b`, `i = 0`
- 指针声明：`SWDIface *_swd;`（星号靠近变量名）

### 5.6 初始化列表

```cpp
FlashAccessor::FlashAccessor()
    : TargetFlash(),
      _flash_state(FLASH_STATE_CLOSED),
      _current_sector_valid(false)
{
}
```

### 5.7 注释位置

- **注释优先放在代码上一行**，而非行尾
- 便于阅读和统一代码风格
- 多行注释使用 `/* ... */`，单行注释使用 `//`

**推荐写法：**

```cpp
// 检查当前扇区是否已设置
if (!_current_sector_valid)
{
    // 设置下一个扇区
    status = setup_next_sector(packet_addr);
}

/* 初始化变量 */
memset(_page_buffer, 0xFF, sizeof(_page_buffer));
_page_buf_empty = true;
_current_sector_valid = false;
```

**不推荐写法：**

```cpp
if (!_current_sector_valid)  // 检查当前扇区是否已设置
{
    status = setup_next_sector(packet_addr);  // 设置下一个扇区
}

memset(_page_buffer, 0xFF, sizeof(_page_buffer));  // 初始化变量
_page_buf_empty = true;
_current_sector_valid = false;
```

**例外情况：**

- 类成员变量声明可使用行尾注释（Doxygen 风格 `///<`）
- 短小的 `#include` 或 `#define` 可使用行尾注释

## 6. 内存分配

对于大小超过 **64 字节** 的数组或结构体，应考虑以下优化方案：

### 6.1 优先级顺序

1. **静态成员** - 适用于生命周期与程序相同的对象
2. **智能指针** - C++ 中动态分配的首选方案
3. **malloc/free** - 仅在 C 代码中使用

### 6.2 示例

**静态成员方式：**

```cpp
class Example
{
private:
    static uint8_t _buffer[1024];  // 静态成员，避免栈溢出
};
```

**智能指针方式（C++）：**

```cpp
#include <memory>

void process_data(void)
{
    // 使用 unique_ptr 自动管理内存
    auto buffer = std::make_unique<uint8_t[]>(1024);

    // 使用 buffer.get() 获取原始指针
    read_data(buffer.get(), 1024);

    // 无需手动释放，离开作用域自动释放
}
```

**malloc/free 方式（仅 C 代码）：**

```c
void process_data(void)
{
    uint8_t *buffer = (uint8_t *)malloc(1024);

    if (buffer == NULL)
    {
        return;
    }

    read_data(buffer, 1024);

    free(buffer);  // 必须手动释放
}
```

### 6.3 注意事项

- 避免在栈上分配大数组，防止栈溢出
- 智能指针优先使用 `std::unique_ptr`，需要共享所有权时使用 `std::shared_ptr`
- C++ 代码禁止直接使用 `malloc/free`，应使用 `new/delete` 或智能指针

***

## 7. 架构设计

### 7.1 避免全局变量

- **尽可能不要使用全局变量**，全局变量会导致代码耦合度高、难以测试、线程安全问题
- **采用依赖注入的方式传递依赖**，通过函数参数、构造函数参数等方式传递所需对象

### 7.2 推荐做法

**C++ 依赖注入：**

```cpp
// 不推荐：使用全局变量
SWDIface *g_swd = nullptr;

void flash_init(void)
{
    g_swd->connect();
}

// 推荐：依赖注入
class TargetFlash
{
public:
    void swd_init(SWDIface &swd)
    {
        _swd = &swd;  // 通过参数注入依赖
    }

private:
    SWDIface *_swd;
};
```

**C 语言依赖注入：**

```c
// 不推荐：使用全局变量
static int g_sock = -1;

void send_data(const char *buf, int len)
{
    send(g_sock, buf, len, 0);
}

// 推荐：通过参数传递
void send_data(int sock, const char *buf, int len)
{
    send(sock, buf, len, 0);
}

// 或使用上下文结构体
typedef struct
{
    int sock;
    uint8_t state;
} tcp_context_t;

void send_data(tcp_context_t *ctx, const char *buf, int len)
{
    send(ctx->sock, buf, len, 0);
}
```

### 7.3 例外情况

以下情况可以谨慎使用全局变量：
- 单例模式的实现
- 硬件资源抽象（如日志系统）
- 兼容旧代码时的过渡方案

***

## 8. 规范要点速查表

| 类别        | 要点                                               |
| --------- | ------------------------------------------------ |
| **文件头**   | 标准版权声明 + Change Logs，头文件用 `#pragma once`         |
| **类名**    | 大驼峰命名：`TargetFlash`, `StreamProgrammer`          |
| **C++类成员变量**  | 私有/保护变量下划线前缀蛇形：`_swd`，公有变量蛇形无前缀             |
| **C语言变量命名**  | 全小写 + 下划线，静态变量 `s_` 前缀，全局 `g_` 前缀                 |
| **函数命名**  | 蛇形命名+模块前缀：`flash_init()`，私有函数下划线前缀：`_validate()` |
| **类型定义**  | 后缀 `_t`：`err_t`, `target_cfg_t`                  |
| **宏定义**   | 全大写 + 下划线：`ERR_NONE`, `FLASH_STATE_CLOSED`       |
| **枚举值**   | 全大写 + 下划线前缀，枚举名用 `_def` 后缀                       |
| **文件组织**  | `inc/` 存头文件，`src/` 存源文件，标准库在前项目头文件在后             |
| **文档注释**  | Doxygen 风格：`@brief`, `@param`, `@return`, `///<` |
| **缩进**    | 4 空格，不用 Tab                                      |
| **花括号**   | 左括号独立成行，右括号后空一行                                  |
| **空行规则**  | 代码紧凑不留空行，`}` 后非 `else`/`while` 空一行，return 前空一行 |
| **局部变量**  | 统一定义在函数开头，尽量在定义时初始化，定义块后空一行                    |
| **初始化列表** | 构造函数初始化列表，每行一个成员                                 |
| **空格规则**  | 关键字后加空格，运算符两侧加空格，指针星号靠近变量名                       |
| **注释位置**  | 优先放在代码上一行，而非行尾                                   |
| **内存分配**  | >64字节优先静态成员或智能指针，C++ 禁用 `malloc/free`            |
| **架构设计**  | 避免全局变量，采用依赖注入方式传递依赖                       |

***

*本规范适用于 ESP32-DAPLink 项目开发，用于代码审查和编码参考。*

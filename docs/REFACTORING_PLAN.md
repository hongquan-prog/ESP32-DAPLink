# USBIP 协议栈重构计划

## 目标

1. 分析内存占用严重的地方，减少冗余
2. 解耦合，消除全局变量滥用
3. 用多态方式统一 TCP/KCP/NetConn 三种传输实现

---

## 一、当前架构问题分析

### 1. 全局变量滥用

`extern int g_k_sock` 在 **5 个文件** 中被直接使用：

- `main/DAP_handle.c:67`
- `main/kcp_server.c:33`
- `main/usbip_server.c:29`
- `components/elaphureLink/elaphureLink_protocol.c:18`
- `components/USBIP/usb_handle.c:25`

`elaphureLink_protocol.c` 还直接 extern 了多个函数：

```c
extern void malloc_dap_ringbuf(void);
extern void free_dap_ringbuf(void);
extern uint32_t DAP_ExecuteCommand(...);
```

### 2. 三种网络实现的重复代码

| 文件 | 传输方式 | 重复逻辑 |
|------|----------|----------|
| tcp_server.c | BSD Socket | 状态机、DAP重启信号、连接管理 |
| kcp_server.c | KCP over UDP | 状态机、DAP重启信号、连接管理 |
| tcp_netconn.c | LwIP Netconn | 状态机、DAP重启信号、连接管理 |

**核心重复代码** (出现在 3 个文件中):

```c
switch (usbip_get_state()) {
case USBIP_STATE_ACCEPTING:
    usbip_set_state(USBIP_STATE_ATTACHING);
    // fallthrough
case USBIP_STATE_ATTACHING:
    usbip_attach(buffer, len);
    break;
case USBIP_STATE_EMULATING:
    usbip_emulate(buffer, len);
    break;
}
// 连接断开时的清理代码也重复
dap_set_restart_signal(DAP_SIGNAL_RESET);
dap_notify_task();
```

### 3. 条件编译选择实现

```c
// main.cpp:192-196
#if (USBIP_USE_TCP_NETCONN == 1)
    xTaskCreate(tcp_netconn_task, ...);
#else
    xTaskCreate(tcp_server_task, ...);
#endif
```

编译时选择，无法动态切换，且代码分散。

### 4. 内存占用问题

| 缓冲区 | 大小 | 问题 |
|--------|------|------|
| DAP Ring Buffer | ~10KB | DAP_BUFFER_NUM=10 过多 |
| tcp_rx_buffer | 1500 | 栈上分配 |
| s_kcp_buffer | 1500 | 静态分配 |
| s_el_process_buffer | 1500 | 动态分配 |
---

## 二、重构架构设计

### 核心思路：传输接口抽象

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                         │
│  ┌─────────────┐  ┌─────────────┐  ┌──────────────────────┐ │
│  │ USBIP Server│  │ elaphureLink│  │ DAP Handle           │ │
│  └──────┬──────┘  └──────┬──────┘  └──────────┬───────────┘ │
│         │                │                    │              │
│         └────────────────┼────────────────────┘              │
│                          ▼                                   │
│              ┌───────────────────────┐                       │
│              │ transport_context_t   │  <-- 上下文结构       │
│              │ ├─ transport_ops_t    │  <-- 操作接口         │
│              │ ├─ state              │  <-- 状态机           │
│              │ └─ buffer_pool        │  <-- 共享缓冲区       │
│              └───────────┬───────────┘                       │
└──────────────────────────┼───────────────────────────────────┘
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                   Transport Layer (Interface)                │
│                                                              │
│  transport_ops_t:                                            │
│  ├─ init(ctx)           // 初始化传输                        │
│  ├─ send(ctx, buf, len) // 发送数据                          │
│  ├─ recv(ctx, buf, len) // 接收数据                          │
│  ├─ close(ctx)          // 关闭连接                          │
│  └─ destroy(ctx)        // 销毁资源                          │
│                                                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ tcp_ops     │  │ kcp_ops     │  │ netconn_ops         │  │
│  │ (BSD)       │  │ (KCP/UDP)   │  │ (LwIP)              │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### 新增文件结构

```
main/
├── transport/
│   ├── transport.h          # 传输接口定义
│   ├── transport.c          # 传输上下文管理
│   ├── transport_tcp.c      # BSD Socket 实现
│   ├── transport_tcp.h
│   ├── transport_kcp.c      # KCP 实现
│   ├── transport_kcp.h
│   ├── transport_netconn.c  # LwIP Netconn 实现
│   └── transport_netconn.h
├── usbip_server.c           # 简化，只处理协议
├── usbip_context.h          # USBIP 上下文结构
└── DAP_handle.c             # 简化，移除 fast_reply
```

---

## 三、详细设计

### 3.1 传输接口定义 (transport.h)

```c
#pragma once

#include <stdint.h>
#include <stddef.h>

// 前向声明
typedef struct transport_context transport_context_t;
typedef struct transport_ops transport_ops_t;

// 传输操作接口 (类似虚函数表)
struct transport_ops {
    int (*init)(transport_context_t *ctx);
    int (*send)(transport_context_t *ctx, const void *buf, size_t len);
    int (*recv)(transport_context_t *ctx, void *buf, size_t len);
    int (*close)(transport_context_t *ctx);
    void (*destroy)(transport_context_t *ctx);
};

// 传输上下文
struct transport_context {
    const transport_ops_t *ops;    // 操作接口
    void *private_data;            // 私有数据 (socket, netconn 等)
    uint8_t *rx_buffer;            // 接收缓冲区 (共享)
    size_t rx_buffer_size;
    int state;                     // 连接状态
};

// 创建传输上下文
transport_context_t *transport_create(const transport_ops_t *ops);

// 统一 API (调用 ops 实现)
int transport_send(transport_context_t *ctx, const void *buf, size_t len);
int transport_recv(transport_context_t *ctx, void *buf, size_t len);
int transport_close(transport_context_t *ctx);
void transport_destroy(transport_context_t *ctx);
```

### 3.2 USBIP 上下文结构 (usbip_context.h)

```c
#pragma once

#include "transport.h"
#include "usbip_defs.h"

typedef struct {
    transport_context_t *transport;  // 传输层
    usbip_state_t state;             // USBIP 状态机
    uint8_t *rx_buffer;              // 共享接收缓冲区
    size_t rx_buffer_size;
} usbip_context_t;

// 初始化/销毁
usbip_context_t *usbip_context_create(const transport_ops_t *ops);
void usbip_context_destroy(usbip_context_t *ctx);

// 协议处理 (不再依赖全局变量)
int usbip_process_data(usbip_context_t *ctx, void *data, size_t len);
```

### 3.3 TCP 传输实现示例 (transport_tcp.c)

```c
#include "transport_tcp.h"

typedef struct {
    int listen_sock;
    int client_sock;
    struct sockaddr_in client_addr;
} tcp_private_t;

static int tcp_init(transport_context_t *ctx) {
    tcp_private_t *priv = ctx->private_data;
    // 创建监听 socket...
}

static int tcp_send(transport_context_t *ctx, const void *buf, size_t len) {
    tcp_private_t *priv = ctx->private_data;
    return send(priv->client_sock, buf, len, 0);
}

static int tcp_recv(transport_context_t *ctx, void *buf, size_t len) {
    tcp_private_t *priv = ctx->private_data;
    return recv(priv->client_sock, buf, len, 0);
}

static int tcp_close(transport_context_t *ctx) {
    tcp_private_t *priv = ctx->private_data;
    close(priv->client_sock);
    return 0;
}

static void tcp_destroy(transport_context_t *ctx) {
    free(ctx->private_data);
}

// 导出操作表
const transport_ops_t tcp_transport_ops = {
    .init = tcp_init,
    .send = tcp_send,
    .recv = tcp_recv,
    .close = tcp_close,
    .destroy = tcp_destroy,
};
```

### 3.4 简化后的 usbip_server.c

```c
// 原来的全局变量消失
// extern int g_k_sock;  <-- 删除

// 使用上下文
void usbip_server_task(void *arg) {
    const transport_ops_t *ops = (const transport_ops_t *)arg;
    usbip_context_t *ctx = usbip_context_create(ops);

    while (1) {
        int len = transport_recv(ctx->transport, ctx->rx_buffer, ctx->rx_buffer_size);
        if (len <= 0) {
            // 连接关闭
            transport_close(ctx->transport);
            usbip_reset_state(ctx);
            continue;
        }

        usbip_process_data(ctx, ctx->rx_buffer, len);
    }
}
```

### 3.5 简化后的 elaphureLink_protocol.c

```c
// 不再使用 extern
// extern int g_k_sock;           <-- 删除
// extern void malloc_dap_ringbuf <-- 删除

// 通过参数传入上下文
int el_dap_data_process(usbip_context_t *ctx, void *buffer, size_t len) {
    int res = DAP_ExecuteCommand(buffer, ctx->dap_buffer);
    res &= 0xFFFF;
    transport_send(ctx->transport, ctx->dap_buffer, res);
    return 0;
}
```

---

## 四、内存优化

### 4.1 减少 DAP Ring Buffer

```c
// DAP_handle.c
#if (USBIP_ENABLE_WINUSB == 1)
#define DAP_BUFFER_NUM 4   // WinUSB: 4 * 516 = 2KB
#else
#define DAP_BUFFER_NUM 6   // HID: 6 * 255 = 1.5KB
#endif
```

**节省: ~3KB**

### 4.2 共享接收缓冲区

```c
// 所有传输实现共享一个接收缓冲区
// 在 usbip_context_t 中分配
usbip_context_t *usbip_context_create(...) {
    ctx->rx_buffer = malloc(1500);  // 只分配一次
}
```

**节省: 2 * 1500 = 3KB (移除 kcp_buffer, el_process_buffer)**

---

## 五、重构步骤

### Phase 1: 创建传输抽象层

1. 创建 `main/transport/transport.h` - 接口定义
2. 创建 `main/transport/transport.c` - 上下文管理
3. 创建 `main/transport/transport_tcp.c` - TCP 实现
4. 创建 `main/transport/transport_netconn.c` - Netconn 实现
5. 创建 `main/transport/transport_kcp.c` - KCP 实现

### Phase 2: 重构 USBIP 服务

1. 创建 `main/usbip_context.h` - USBIP 上下文
2. 重构 `main/usbip_server.c` - 移除全局变量，使用上下文
3. 移动 `fast_reply()` 到 `usbip_server.c`

### Phase 3: 重构 DAP Handle

1. 移除 `extern int g_k_sock`
2. 移除静态缓冲区 `s_dap_data_processed`
3. 减少 `DAP_BUFFER_NUM`

### Phase 4: 重构 elaphureLink

1. 移除所有 extern 声明
2. 通过上下文参数传递

### Phase 5: 更新主程序

1. 修改 `main.cpp` 使用新接口
2. 选择传输实现

---

## 六、关键文件修改清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `main/transport/transport.h` | 新建 | 传输接口定义 |
| `main/transport/transport.c` | 新建 | 上下文管理 |
| `main/transport/transport_tcp.c` | 新建 | TCP 实现 |
| `main/transport/transport_netconn.c` | 新建 | Netconn 实现 |
| `main/transport/transport_kcp.c` | 新建 | KCP 实现 |
| `main/usbip_context.h` | 新建 | USBIP 上下文 |
| `main/usbip_server.c` | 重构 | 移除全局变量，使用上下文 |
| `main/DAP_handle.c` | 重构 | 移除 extern，减少缓冲区 |
| `components/elaphureLink/elaphureLink_protocol.c` | 重构 | 移除 extern |
| `components/USBIP/usb_handle.c` | 重构 | 移除 extern |
| `main/main.cpp` | 修改 | 使用新接口 |

---

## 七、验证方法

### 7.1 编译验证

每次重构阶段完成后都要编译通过：

```bash
source /home/lhq/Software/esp-idf/export.sh
idf.py build
```

### 7.2 功能测试矩阵

| 测试场景 | TCP | Netconn | KCP |
|----------|-----|---------|-----|
| Keil MDK (HID 模式) | ✓ | ✓ | ✓ |
| Keil MDK (WinUSB/elaphureLink) | ✓ | ✓ | - |
| OpenOCD | ✓ | ✓ | ✓ |
| 连接断开重连 | ✓ | ✓ | ✓ |

### 7.3 内存验证

在重构前后对比内存使用：

```c
// 在 main.cpp 中添加
#include "esp_heap_caps.h"

void print_heap_info(const char *label) {
    printf("[%s] Free heap: %lu, Largest block: %lu\n",
           label,
           esp_get_free_heap_size(),
           heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}
```

在关键位置调用：

- 启动时
- USBIP 连接建立后
- DAP 任务运行时

### 7.4 回归测试流程

```
1. 烧录固件
   idf.py flash monitor

2. 连接 WiFi，确认 IP 地址

3. 在 PC 上安装 usbip-win 驱动

4. 绑定设备
   usbip attach -r <ESP32_IP> -b 1-1

5. 打开 Keil MDK，配置调试器
   - HID 模式：选择 CMSIS-DAP HID
   - WinUSB 模式：选择 CMSIS-DAP WinUSB

6. 测试调试功能
   - 连接目标板
   - 下载程序
   - 单步调试
   - 查看变量

7. 测试断开重连
   - 断开 TCP 连接
   - 重新绑定
   - 确认能正常恢复
```

### 7.5 分阶段验证

| 阶段 | 验证点 |
|------|--------|
| Phase 1: 传输抽象层 | 编译通过，单元测试 ops 接口 |
| Phase 2: USBIP 重构 | 编译通过，状态机逻辑不变 |
| Phase 3: DAP Handle | 编译通过，ring buffer 正常 |
| Phase 4: elaphureLink | WinUSB 模式能正常工作 |
| Phase 5: 集成测试 | 全功能测试矩阵 |

---

## 八、预期收益

| 改进项 | 内存节省 | 代码质量 |
|--------|----------|----------|
| 减少 DAP_BUFFER_NUM | ~3KB | - |
| 共享接收缓冲区 | ~3KB | - |
| 移除静态缓冲区 | ~1KB | - |
| 传输接口抽象 | - | 解耦，可扩展 |
| 移除 extern | - | 模块化 |

**总预估内存节省: ~7KB RAM**

**架构改进: 消除全局变量，实现模块解耦**
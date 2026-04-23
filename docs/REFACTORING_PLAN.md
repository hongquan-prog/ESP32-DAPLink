# USBIP 协议栈重构计划 (已完成)

> **注意**: 本文档描述的是历史重构计划。USBIP 协议栈已完成重构，新的实现位于 `components/usbipd/` 目录，基于 usbip-server 架构。

## 重构结果

USBIP 实现已完全替换为新的模块化架构：

```
components/usbipd/
├── include/                # 公共头文件
│   ├── usbip_server.h      # 服务器主接口
│   ├── usbip_protocol.h    # USBIP 协议定义
│   ├── usbip_devmgr.h      # 设备驱动管理
│   ├── usbip_control.h     # 控制传输框架
│   ├── usbip_hid.h         # HID 设备基类
│   ├── usbip_common.h      # 通用定义
│   └── hal/                # 硬件抽象层
│       ├── usbip_osal.h    # OS 抽象层
│       ├── usbip_transport.h # 传输抽象层
│       └── usbip_log.h     # 日志系统
└── src/
    ├── server/             # 服务器核心
    │   ├── usbip_server.c  # 连接管理
    │   ├── usbip_urb.c     # URB 处理（生产者-消费者）
    │   ├── usbip_devmgr.c  # 设备管理
    │   └── usbip_control.c # 控制传输
    ├── device/             # 设备基类
    │   ├── usbip_hid.c     # HID 设备实现
    │   └── usbip_bulk.c    # Bulk 设备实现
    ├── hal/                # HAL 实现
    │   ├── usbip_osal.c    # OSAL 包装函数
    │   ├── usbip_mempool.c # 内存池
    │   └── usbip_transport.c # 传输接口
    ├── platform/espidf/    # ESP-IDF 平台
    │   ├── osal_espidf.c   # FreeRTOS 实现
    │   └── transport_espidf.c # lwIP TCP 传输
    └── device_drivers/     # DAP 设备驱动
        ├── hid_dap.c       # HID CMSIS-DAP (busid: 2-1)
        └── bulk_dap.c      # Bulk CMSIS-DAP (busid: 2-2)
```

## 关键改进

### 1. 消除全局变量
- 使用上下文结构传递状态
- 设备驱动通过 `usbip_register_driver()` 注册
- 传输层通过 `TRANSPORT_REGISTER()` 宏注册

### 2. 传输层抽象
- 统一的传输接口 `usbip_transport`
- ESP-IDF 平台使用 lwIP socket 实现
- 支持 TCP_NODELAY 和 SO_KEEPALIVE

### 3. OS 抽象层 (OSAL)
- 跨平台支持：POSIX 和 FreeRTOS
- 提供线程、互斥锁、条件变量、内存管理抽象

### 4. URB 处理
- 生产者-消费者模式处理 URB
- 独立线程处理 URB 队列
- 支持超时和 unlink 操作

### 5. 设备驱动架构
- HID 和 Bulk 两种 DAP 设备
- 自动注册机制（使用 `__attribute__((constructor))`）
- 支持 MS OS 2.0 描述符

## 原架构问题（已解决）

原架构存在以下问题，新架构已解决：

| 问题 | 原架构 | 新架构 |
|------|--------|--------|
| 全局变量滥用 | `g_k_sock` 等全局变量 | 上下文结构传递 |
| 传输实现重复 | TCP/KCP/Netconn 各自实现状态机 | 统一传输接口 |
| 缓冲区冗余 | 多个静态缓冲区 | 动态分配，内存池管理 |
| 耦合严重 | extern 函数声明混乱 | 模块化，接口清晰 |

## 配置选项

新架构的配置选项（`idf.py menuconfig`）：

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_USBIP_SERVER_ENABLED` | 启用 USBIP 服务器 | y |
| `CONFIG_USBIP_SERVER_PORT` | 服务器端口 | 3240 |
| `CONFIG_URB_THREAD_STACK_SIZE` | URB 线程栈大小 | 4096 |
| `CONFIG_URB_THREAD_PRIORITY` | URB 线程优先级 | 14 |

## 使用方法

### 无线调试

1. 配置 WiFi 连接（通过 menuconfig 或 web 界面）
2. USBIP 服务器自动启动在端口 3240
3. Linux 主机连接：
   ```bash
   # 列出设备
   usbip list -r <esp32-ip>
   
   # 连接 HID 设备
   sudo usbip attach -r <esp32-ip> -b 2-1
   
   # 连接 Bulk 设备
   sudo usbip attach -r <esp32-ip> -b 2-2
   ```

## 性能数据

参见 [USBIP_DAP_RESPONSE_FIX.md](USBIP_DAP_RESPONSE_FIX.md) 中的性能分析部分。

## 历史文档

本文档的原始内容（重构计划详情）已归档，如需查看请参考 git 历史。

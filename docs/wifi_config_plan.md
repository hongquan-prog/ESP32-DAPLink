# WiFi 配网页面实现计划

## Context

当前 ESP32-DAPLink 的 WiFi 使用 sdkconfig 中硬编码的 SSID/PASSWORD，通过 `example_connect()` 连接。缺少动态配网能力。

本功能：当 NVS 中 `WIFI_SSID` / `WIFI_PASSWORD` 为空时，设备进入 AP 模式（SSID 基于 MAC 地址动态生成），用户通过 Web 页面输入 WiFi 凭据后，存入 NVS 并调用 `esp_restart()` 重启，重启后进入 Station 模式连接。

## NVS 存储格式

- Namespace: `wifi_config`
- Keys: `WIFI_SSID`, `WIFI_PASSWORD`

## 实现方案

### 整体流程

1. 启动时从 NVS 读取 `WIFI_SSID`
2. 若读取失败或值为空 → 启动 AP 模式 + Web 服务器（提供配网页面）
3. 用户提交 SSID/PASSWORD → 存入 NVS → 调用 `esp_restart()` 重启
4. 若已有有效 SSID → 正常 Station 模式启动

### 新增文件

| 文件 | 作用 |
|------|------|
| `html/config.html` | 配网页面（风格与 root.html 保持一致：深色渐变背景、霓虹按钮） |
| `main/wifi.c` | WiFi 检查/AP 启动/NVS 读写/重启 |
| `main/wifi.h` | 对外接口声明 |

### 修改文件

| 文件 | 修改内容 |
|------|------|
| `main/main.cpp` | 替换 `example_connect()` 为 `wifi_init()`，实现分支逻辑 |
| `main/web/web_handler.cpp` | 新增 `web_wifi_config_handler`(GET) 和 `web_wifi_settings_handler`(POST)，注册 config_html 资源 |
| `main/web/web_handler.h` | 新增 handler 声明 |
| `main/web/web_server.c` | 注册 `/settings` 和 `/api/wifi-set` URI |
| `main/CMakeLists.txt` | 添加 `config.html` 和 `wifi.c` |

### 关键实现

#### `wifi.c` — 核心逻辑

```c
// 1. wifi_init() — app_main() 调用
//    - 读取 NVS WIFI_SSID
//    - 为空 → wifi_start_ap()
//    - 有效 → wifi_connect_sta()

// 2. wifi_start_ap()
//    - 基于 MAC 地址生成 SSID，如 "DAPLink-XXXX"
//    - 配置 AP 无密码 (open)
//    - 启动 softAP + TCP server (web_server)

// 3. wifi_connect_sta() — 仅在 Station 模式启动时调用
//    - 从 NVS 读取 WIFI_SSID/WIFI_PASSWORD
//    - 配置 Station + 启动 WiFi
//    - 连接成功后启动 web_server
//    - 复用 main.cpp 中已有的 connect_handler/disconnect_handler

// 4. NVS 读写
//    - nvs_open("wifi_config", NVS_READWRITE)
//    - nvs_set_str("WIFI_SSID", ssid) / nvs_set_str("WIFI_PASSWORD", pass)
//    - nvs_get_str("WIFI_SSID") 读取
```

#### `web_wifi_settings_handler` 逻辑

```c
// 1. 解析 JSON {"ssid": "...", "password": "..."}  (使用 cJSON)
// 2. 写入 NVS (namespace "wifi_config", keys "WIFI_SSID"/"WIFI_PASSWORD")
// 3. 写入 NVS 后调用 esp_restart() 重启
// 4. 返回 {"status": "ok"}
// 访问 URL: GET /settings
```

#### AP 模式 SSID 格式

`DAPLink-XXXX`，其中 XXXX 是 MAC 地址后 4 位（如 `DAPLink-1A2B`）。

### 复用现有组件

- **cJSON** (`main/web/web_handler.cpp`) — 已有
- **NVS** (`nvs_flash.h`) — 已初始化
- **WiFi event handlers** — 复用 `connect_handler`/`disconnect_handler` 逻辑
- **Web 服务器框架** — 复用 `httpd_handle_t` 模式
- **root.html** — 当前导航页保持不变，配网页面独立

### 验证方案

1. **清除 NVS 后启动**：`idf.py erase_flash` 后上电 → 检查日志进入 AP 模式
2. **连接 AP**：手机/电脑连接 `DAPLink-XXXX`（无密码）
3. **访问配网页面**：浏览器打开 `http://192.168.4.1/settings`
4. **提交表单**：输入目标 WiFi SSID/密码 → 提交
5. **观察重启**：设备自动重启，串口日志确认进入 Station 模式并连接成功
6. **再次上电**：NVS 已有凭据，直接 Station 模式连接，不进入 AP

# CEQP 项目总览

这是一个由两个主要部分组成的工程：

- `imgui控制程序/`：基于 Win32 + DirectX 11 + Dear ImGui 的图形化客户端，用于连接 CE 插件并进行内存相关操作。
- `plugin/TCP_UDP/`：Cheat Engine 插件（服务端），实现 CEQP 协议，通过 TCP/UDP 与外部客户端通信。

两者通过 CEQP 协议互通，完成模块基址查询、内存读写、指针链读写、日志与共享数据展示等功能。客户端支持覆盖层、窗口跟随、禁用屏幕截图（黑屏）与完整的国际化。

---

## 目录结构

- `imgui控制程序/`：UI、通信、渲染与设置持久化
- `plugin/TCP_UDP/`：CE 插件及协议服务端实现
- `Language/`：翻译文件（如 `zh-cn.po`）
- `README.md`：项目总览（本文件）

详细见各目录下的 README。

---

## 技术栈与实现原则

- **客户端**：Win32 窗口、DX11 渲染、Dear ImGui UI；C++17。
- **服务端**：Cheat Engine 插件 API、Winsock；C/C++。
- **协议**：自定义 CEQP，帧头 + TLV 负载，小端编码。统一错误码与消息返回，便于跨语言客户端实现。

设计原则：
- 明确的协议边界，前后端职责清晰（解析在客户端、执行在服务端）。
- 高容错但严格约束：如模块+偏移允许引号但会自动清理，偏移仅十六进制。
- 安全优先：内存页属性检查、长度限制、错误路径清晰可见。

---

## 架构与数据流

- 客户端：
  1. 渲染覆盖层 UI，提供连接、内存、指针链、日志与设置等面板。
  2. 连接到 CE 插件，按需发送 CEQP 请求，解析返回并展示。
  3. 支持“禁止截图（黑屏）”，通过 DWM 与 Display Affinity 双重实现。

- 服务端（插件）：
  1. CE 启动时加载插件，开启 TCP 监听（默认 9178）。
  2. 接收请求，解码 TLV，根据类型执行内存或模块操作。
  3. 返回结果或错误码与错误消息。

---

## 协议要点（CEQP）

- 帧头：版本、长度、校验等；负载为 TLV 列表。
- 重用 TLV 类型：`ADDR(u64)`、`LEN(u32)`、`MODNAME(string)`、`OFFSET(s64)`、`OFFSETS(s64[])`、`DTYPE(string)`、`DATA(bytes)`、`ERRCODE(u32)`、`ERRMSG(string)`。
- 请求类型：`PING`、`READ_MEM`、`WRITE_MEM`、`GET_MODULE_BASE`、`READ_PTR_CHAIN`、`WRITE_PTR_CHAIN`。
- 错误统一返回 `ERROR_RESP`，含错误码与消息，便于客户端准确提示。

---

## 地址与偏移解析策略（客户端）

- 支持两类输入：
  - 纯地址：`0x7FF6ABC01234` 或 `1407357742`（`CETCP::parseAddress` 支持）。
  - 模块+偏移：`libEngine.dll+013781B0`（偏移仅十六进制，允许 `0x` 前缀）。

- 容错处理：
  - 模块名前后空白与包裹引号（`'"'`）自动去除：例如 `"libEngine.dll" + 013781B0` 正常解析。
  - 模块+偏移的偏移部分只允许十六进制字符；含非十六进制字符会报错并给出指引。
  - 指针链的偏移链项允许十进制或十六进制，两者可混用（与“基址里的 + 偏移”规则不同）。

---

## UI 与交互（客户端）

- 面板：`Connection`、`Memory`、`Pointer Chain`、`Shared Data`、`Log`、`Settings`。
- 连接：`Host/Port`、`Connect/Disconnect`、`Test Connection`（心跳）。
- 内存：读取十六进制展示，写入支持数值与十六进制字节序列。
- 指针链：自动检测指针大小（`ptr32/ptr64`），构造 `dtype`（如 `ce noderef ptr64 u32`）。
- 设置：语言、主题、字体、热键与“禁止截图（黑屏）”。
- 国际化：所有文案使用 `I18N::tr`；中文翻译集中在 `Language/zh-cn.po`。

---

## 兼容性与限制

- Windows 10/11；DX11；VS2019/VS2022 构建。
- CE 7.x；插件位数需与 CE 一致；客户端位数可独立。
- 截图禁用适用于主流系统 API；个别注入类工具可能绕过。
- 单帧最大负载约 1MB；网络错误会触发断开与日志记录。

---

## 构建与运行

- 客户端：打开 `imgui控制程序/*.sln`，选择 `Release|x64` 构建并运行。
- 插件：在 `plugin/TCP_UDP/` 构建 DLL，将其加载到 CE 中（可用脚本或手动）。
- 连接：在客户端设置 `Host=127.0.0.1`、`Port=9178`，点击 `Connect`。

---

## 国际化（I18N）与翻译维护

- 新增文案时务必使用 `I18N::tr("...")`；动态消息按可翻译片段拼接。
- 新增语言：复制 `po` 文件并按语言选择框绑定；测试 UI 全覆盖。
- 中文：`zh-cn.po` 已包含核心文案，如 `Server:`、`Memory`、`Tips:`、`Exclude from screen capture` 等。

---

## 常见问题（FAQ）

- 连接失败：检查服务端是否加载、端口是否被占用、防火墙规则。
- 模块+偏移解析错误：确认偏移为十六进制；模块名是否存在于目标进程。
- 指针链读写异常：指针大小与进程不匹配，或中间节点不可读。

---

## 许可证与鸣谢

- 许可证：MIT。
- 作者：Lun（QQ: 1596534228，GitHub: Lun-OS）。
- 鸣谢：Dear ImGui、Microsoft DX11、Cheat Engine 社区。


# CEQP 控制程序（ImGui 客户端）

基于 Win32 + DirectX 11 + Dear ImGui 的图形化控制程序，用于通过 CEQP 协议与 Cheat Engine 插件通信，实现内存读写、模块操作、指针链读取/写入、UI 覆盖层等功能。客户端作为覆盖层（Layered TopMost 窗口）工作，可跟随目标进程窗口移动，并支持禁用屏幕截图（黑屏）。

---

## 总览与架构

- **核心组件**：
  - `CETCP.h/.cpp`：CEQP 客户端通信实现（Winsock），TLV 编码/解码。
  - `ui.h/.cpp`：ImGui UI 渲染与交互逻辑（连接、内存、指针链、日志、设置等）。
  - `dx11_renderer.*`：Win32 + DX11 初始化、渲染循环、窗口过程。
  - `i18n.*`、`Language/zh-cn.po`：国际化系统与中文翻译。
  - `settings.*`：设置持久化（`settings.ini`）。
  - `globals.*`：全局状态（如 `g_mainHwnd`、`g_excludeFromCapture`）。

- **工作流程**：
  1. 启动后创建顶置、分层透明的覆盖层窗口（DX11 进行渲染）。
  2. 通过 UI 面板连接到 CE 插件服务端（TCP，默认端口 `9178`）。
  3. 按需发起 CEQP 指令：读写内存、查询模块基址、读取/写入指针链等。
  4. 在日志/共享数据区展示结果和解析。

---

## 关键实现原理

- **CEQP 协议**（客户端侧）：
  - 帧头固定 16 字节，负载为 TLV 列表（小端）。常用 TLV：`ADDR(u64)`、`LEN(u32)`、`MODNAME(string)`、`OFFSET(s64)`、`OFFSETS(s64[])`、`DATA(bytes)`、`DTYPE(string)`、`ERRCODE(u32)`、`ERRMSG(string)`。
  - 客户端通过 `sendFrame/receiveFrame` 进行请求/应答，错误统一用 `ERROR_RESP` 返回。

- **地址与偏移解析**：
  - 纯地址使用 `CETCP::parseAddress`（支持 `0x` 前缀或纯数字）。
  - 模块+偏移格式：支持 `libEngine.dll+013781B0`，容错去除包裹引号（`"`/`'`），修剪首尾空白。偏移仅支持十六进制（可选 `0x` 前缀），遇到非十六进制字符将报错。
  - 指针链的“偏移链”每项可选十进制或十六进制；与“基址里的 + 偏移”规则区分。

- **指针链读取/写入**：
  - UI 构造 `offsets` 与 `dtype`，`dtype` 包含 `ptr32/ptr64`、`ce`、`noderef` 等。
  - 自动检测指针大小：优先根据目标进程（示例为 `windows-test.exe`）判断，否则以基址大小推断。
  - 读取后展示路径与数据预览，写入支持多种数值与十六进制字节序列。

- **屏幕截图禁用**：
  - 通过 `DwmSetWindowAttribute(DWMWA_EXCLUDED_FROM_CAPTURE)` 与 `SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)` 让窗口在系统截图中显示黑屏（大多数常见截图工具适用）。
  - 设置项“禁止截图（黑屏）”默认开启，可在设置页切换，且持久化到 `settings.ini`。

---

## UI 结构与交互

- **Connection 面板**：
  - 输入 `Host` 与 `Port`，`Connect/Disconnect`，`Test Connection`（Ping）。
  - 内置心跳：每 2 秒自动 Ping，失败自动断开并记录日志。

- **Memory 标签页**：
  - `Address` 支持纯地址或 `模块+偏移`（仅十六进制偏移，容错去引号）。
  - `Length` 支持十进制或十六进制（带 `0x` 前缀）。
  - `Read Memory` 将十六进制结果填入 `Value`；`Write Memory` 支持：
    - 十六进制字节序列（忽略空白、偶数长度）。
    - 数值类型：`u8/u16/u32/u64`、`i8/i16/i32/i64`、`float/double`（基数可选 10/16）。

- **Pointer Chain 标签页**：
  - `Base Address` 支持纯地址或 `模块名+偏移`（偏移仅十六进制）。
  - `Offset Chain` 通过滑块设定数量，每项可选十进制或十六进制。
  - `Read Pointer Chain` 显示路径与数据预览；`Write Pointer Chain` 同步支持多类型写入。

- **Shared Data / Log 标签页**：
  - 共享数据显示最近结果并解析为常见数值类型。
  - 日志支持清空、自动滚动，显示时间戳与分类颜色。

- **Settings 设置**：
  - 语言（`Language`）、主题（`Theme`）、字体大小等。
  - 热键设置（如 `Toggle Overlay`）。
  - “禁止截图（黑屏）”开关，启用后窗口在截图中显示黑屏。

---

## 国际化（I18N）

- 机制：所有用户可见文案用 `I18N::tr("...")` 包裹；翻译集中在 `Language/*.po`。
- 已扩展键：`Server:`、`Address:`、`Length:`、`Write Type:`、`Language:`、`Theme:`、`Hotkey Settings:`、`Toggle Overlay`、`Not connected to server`、`Exclude from screen capture` 等。
- 动态消息构造：前缀（如 `Read `、` bytes`）可翻译后拼接，确保日志消息本地化。
- 新增语言步骤：新增 `po` 文件、绑定到语言选择；确保所有文案调用 `I18N::tr()`。

---

## 兼容性与限制

- Windows 10/11；DX11 渲染；VS2019/VS2022 构建。
- CE 插件位数与 CE 一致（x64/x86）；客户端可独立位数（通过 TCP 通信）。
- 截图禁用依赖较新的 Windows/DWM；个别截屏或注入类工具可能绕过。
- 单次读写负载最大 1MB；网络异常会触发心跳断开与错误日志。

---

## 构建与运行

1. 打开解决方案：`imgui控制程序/imgui-test.sln`。
2. 选择配置：`Release|x64`（或其他）。
3. 生成并运行。

运行与对接 CE 插件：
- 在 CE 中加载插件服务端（参考 `plugin/TCP_UDP/README.md` 或执行 `run.lua`）。
- 在客户端连接面板填写 `Host`（如 `127.0.0.1`）、`Port`（默认 `9178`），点击 `Connect`。

---

## 代码参考与扩展点

- `CETCP::parseAddress/parseOffset`：统一解析逻辑，十六进制支持与负数处理。
- `RenderPointerChainTab`：偏移链与 `dtype` 构造；路径与数据预览。
- `ApplyScreenshotExclusion`：窗口截图黑屏实现；启动时自动应用。
- 可扩展：点击穿透、窗口跟随策略、进程名配置、更多写入类型、批量指令等。

---

## 版权与作者

- 作者：Lun（QQ: 1596534228，GitHub: Lun-OS）
- 开源协议：MIT；请遵循相关法律法规。


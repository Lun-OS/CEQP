// tcp_udp_plugin.cpp: CE plugin providing TCP packet transceiver and CEQP service Lua API
// Author: Lun.  QQ:1596534228   github:Lun-OSun.  QQ:1596534228   github:Lun-OS
// 完全支持 32 位和 64 位进程的跨架构内存访问
#include "pch.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <TlHelp32.h>
#include <string>
#include <mutex>
#include "cepluginsdk.h"
#include <thread>
#include <atomic>
#include <deque>
#include <vector>
#include <cctype>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <shlobj.h>
#pragma comment(lib, "Ws2_32.lib")
#include <tchar.h>

// ===================== WOW64 跨架构支持 =====================
typedef long NTSTATUS;
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

// NtWow64ReadVirtualMemory64: 32位进程读取64位进程内存
typedef NTSTATUS(WINAPI* PFN_NtWow64ReadVirtualMemory64)(
    HANDLE ProcessHandle,
    PVOID64 BaseAddress,
    PVOID Buffer,
    ULONG64 Size,
    PULONG64 NumberOfBytesRead);

// NtWow64WriteVirtualMemory64: 32位进程写入64位进程内存
typedef NTSTATUS(WINAPI* PFN_NtWow64WriteVirtualMemory64)(
    HANDLE ProcessHandle,
    PVOID64 BaseAddress,
    PVOID Buffer,
    ULONG64 Size,
    PULONG64 NumberOfBytesWritten);

// NtWow64QueryInformationProcess64: 查询64位进程信息
typedef NTSTATUS(WINAPI* PFN_NtWow64QueryInformationProcess64)(
    HANDLE ProcessHandle,
    ULONG ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength);

static PFN_NtWow64ReadVirtualMemory64 g_NtWow64ReadVirtualMemory64 = nullptr;
static PFN_NtWow64WriteVirtualMemory64 g_NtWow64WriteVirtualMemory64 = nullptr;
static PFN_NtWow64QueryInformationProcess64 g_NtWow64QueryInformationProcess64 = nullptr;

// 前向声明
static void debug_logf(const char* fmt, ...);
static std::string tchar_to_utf8(const TCHAR* p);
static int lua_CEQPSetTestEnv(lua_State* L);

static ExportedFunctions Exported;
static bool g_wsa_inited = false;
static std::mutex g_net_mtx;
static SOCKET g_tcp = INVALID_SOCKET;
static bool g_test_env = false;

// 日志相关
static std::ofstream g_log_file;
static std::mutex g_log_mtx;
static std::string g_log_path;

// ===================== 日志系统 =====================
static bool init_log_file() {
    char appdata[MAX_PATH] = { 0 };
    if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appdata) != S_OK) {
        return false;
    }

    std::string log_dir = std::string(appdata) + "\\QAQ";
    CreateDirectoryA(log_dir.c_str(), NULL);

    g_log_path = log_dir + "\\test.log";
    g_log_file.open(g_log_path, std::ios::out | std::ios::app);

    if (!g_log_file.is_open()) {
        return false;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    char time_buf[64];
    sprintf_s(time_buf, "[%04d-%02d-%02d %02d:%02d:%02d]",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    g_log_file << "\n========================================\n";
    g_log_file << time_buf << " Plugin started\n";
    g_log_file << "========================================\n";
    g_log_file.flush();

    return true;
}

static void close_log_file() {
    if (g_log_file.is_open()) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char time_buf[64];
        sprintf_s(time_buf, "[%04d-%02d-%02d %02d:%02d:%02d]",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        g_log_file << time_buf << " Plugin stopped\n";
        g_log_file.close();
    }
}

static void debug_logf(const char* fmt, ...) {
    if (!g_test_env) return;

    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    SYSTEMTIME st;
    GetLocalTime(&st);
    char time_buf[64];
    sprintf_s(time_buf, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    std::string log_msg = std::string(time_buf) + buf;

    OutputDebugStringA(log_msg.c_str());

    std::lock_guard<std::mutex> lock(g_log_mtx);
    if (g_log_file.is_open()) {
        g_log_file << log_msg;
        g_log_file.flush();
    }
}

// ===================== WOW64 初始化 =====================
static void ceqp_init_wow64_symbols() {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) {
        debug_logf("[WOW64] Failed to get ntdll.dll handle\n");
        return;
    }

    g_NtWow64ReadVirtualMemory64 = (PFN_NtWow64ReadVirtualMemory64)
        GetProcAddress(hNtdll, "NtWow64ReadVirtualMemory64");
    g_NtWow64WriteVirtualMemory64 = (PFN_NtWow64WriteVirtualMemory64)
        GetProcAddress(hNtdll, "NtWow64WriteVirtualMemory64");
    g_NtWow64QueryInformationProcess64 = (PFN_NtWow64QueryInformationProcess64)
        GetProcAddress(hNtdll, "NtWow64QueryInformationProcess64");

    if (g_NtWow64ReadVirtualMemory64) {
        debug_logf("[WOW64] NtWow64ReadVirtualMemory64 available\n");
    }
    if (g_NtWow64WriteVirtualMemory64) {
        debug_logf("[WOW64] NtWow64WriteVirtualMemory64 available\n");
    }
    if (g_NtWow64QueryInformationProcess64) {
        debug_logf("[WOW64] NtWow64QueryInformationProcess64 available\n");
    }

    if (!g_NtWow64ReadVirtualMemory64 && !g_NtWow64WriteVirtualMemory64) {
        debug_logf("[WOW64] WOW64 APIs not available (running on 64-bit system or native 32-bit)\n");
    }
}

// ===================== 进程架构检测 =====================
enum ProcessArch {
    ARCH_UNKNOWN = 0,
    ARCH_X86 = 1,    // 32位进程
    ARCH_X64 = 2     // 64位进程
};

static ProcessArch get_process_arch(HANDLE hProcess) {
    BOOL isWow64 = FALSE;
    if (!IsWow64Process(hProcess, &isWow64)) {
        debug_logf("[ARCH] IsWow64Process failed, error=%d\n", GetLastError());
        return ARCH_UNKNOWN;
    }

    // 在 64 位系统上：
    // - 32位进程：isWow64 = TRUE
    // - 64位进程：isWow64 = FALSE
    // 在 32 位系统上：
    // - 32位进程：isWow64 = FALSE

#ifdef _WIN64
    // 64位插件
    return isWow64 ? ARCH_X86 : ARCH_X64;
#else
    // 32位插件
    if (isWow64) {
        // 32位进程在64位系统上运行 (WOW64)
        return ARCH_X86;
    }
    else {
        // 可能是真正的32位系统，或者是64位进程（需要WOW64 API访问）
        BOOL isSystemWow64 = FALSE;
        typedef BOOL(WINAPI* LPFN_ISWOW64PROCESS)(HANDLE, PBOOL);
        LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)
            GetProcAddress(GetModuleHandleA("kernel32"), "IsWow64Process");

        if (fnIsWow64Process && fnIsWow64Process(GetCurrentProcess(), &isSystemWow64) && isSystemWow64) {
            // 我们是32位进程运行在64位系统上，目标进程是64位
            return ARCH_X64;
        }
        return ARCH_X86;
    }
#endif
}

static int lua_CEQPSetTestEnv(lua_State* L) {
    int enable = (int)luaL_optinteger(L, 1, 1);
    g_test_env = !!enable;
    debug_logf("[CEQP] TestEnv set to %s\n", g_test_env ? "true" : "false");
    lua_pushboolean(L, TRUE);
    return 1;
}

// ===================== 网络相关 =====================
static std::thread g_tcp_rx_thread;
static std::atomic<bool> g_tcp_rx_running(false);
static std::mutex g_tcp_rx_q_mtx;
static std::deque<std::string> g_tcp_rx_q;

static bool net_init() {
    if (!g_wsa_inited) {
        debug_logf("[NET] Initializing Winsock...\n");
        WSADATA wsaData;
        int r = WSAStartup(MAKEWORD(2, 2), &wsaData);
        g_wsa_inited = (r == 0);
        debug_logf("[NET] Winsock init %s (code=%d)\n", g_wsa_inited ? "success" : "failed", r);
    }
    return g_wsa_inited;
}

static void net_close_socket(SOCKET& s) {
    if (s != INVALID_SOCKET) {
        debug_logf("[NET] Closing socket %llu\n", (unsigned long long)s);
        shutdown(s, SD_BOTH);
        closesocket(s);
        s = INVALID_SOCKET;
    }
}

static bool tcp_connect(const char* host, int port) {
    debug_logf("[TCP] Connecting to %s:%d...\n", host, port);
    if (!net_init()) {
        debug_logf("[TCP] Network init failed\n");
        return false;
    }

    std::lock_guard<std::mutex> lock(g_net_mtx);
    net_close_socket(g_tcp);

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo* result = nullptr;
    char portstr[32];
    sprintf_s(portstr, "%d", port);

    int ret = getaddrinfo(host, portstr, &hints, &result);
    if (ret != 0) {
        debug_logf("[TCP] getaddrinfo failed: %d\n", ret);
        return false;
    }

    SOCKET s = INVALID_SOCKET;
    for (addrinfo* p = result; p; p = p->ai_next) {
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == INVALID_SOCKET) continue;

        if (connect(s, p->ai_addr, (int)p->ai_addrlen) == 0) {
            g_tcp = s;
            int flag = 1;
            setsockopt(g_tcp, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));
            freeaddrinfo(result);
            debug_logf("[TCP] Connected successfully, socket=%llu\n", (unsigned long long)g_tcp);
            return true;
        }
        closesocket(s);
    }
    freeaddrinfo(result);
    debug_logf("[TCP] All connection attempts failed\n");
    return false;
}

static int tcp_send(const char* data, int len) {
    debug_logf("[TCP] Sending %d bytes...\n", len);
    std::lock_guard<std::mutex> lock(g_net_mtx);
    if (g_tcp == INVALID_SOCKET) {
        debug_logf("[TCP] Socket invalid\n");
        return -1;
    }
    int sent = send(g_tcp, data, len, 0);
    debug_logf("[TCP] Sent %d bytes\n", sent);
    return sent;
}

static std::string tcp_recv(int maxlen, int timeout_ms) {
    std::string out;
    if (maxlen <= 0) return out;

    SOCKET s;
    {
        std::lock_guard<std::mutex> lock(g_net_mtx);
        s = g_tcp;
    }
    if (s == INVALID_SOCKET) return out;

    fd_set rfds; FD_ZERO(&rfds); FD_SET(s, &rfds);
    timeval tv{}; tv.tv_sec = timeout_ms / 1000; tv.tv_usec = (timeout_ms % 1000) * 1000;
    int rv = select(0, &rfds, nullptr, nullptr, &tv);
    if (rv <= 0 || !FD_ISSET(s, &rfds)) return out;

    char buf[8192];
    int to_read = (maxlen > 8192) ? 8192 : maxlen;
    int r = recv(s, buf, to_read, 0);
    if (r > 0) out.assign(buf, buf + r);
    return out;
}

static void tcp_close() {
    debug_logf("[TCP] Closing connection\n");
    std::lock_guard<std::mutex> lock(g_net_mtx);
    net_close_socket(g_tcp);
}

static void tcp_rx_loop() {
    debug_logf("[TCP-RX] Async receive thread started\n");
    while (g_tcp_rx_running) {
        SOCKET s;
        {
            std::lock_guard<std::mutex> lock(g_net_mtx);
            s = g_tcp;
        }
        if (s == INVALID_SOCKET) { Sleep(10); continue; }

        fd_set rfds; FD_ZERO(&rfds); FD_SET(s, &rfds);
        timeval tv{}; tv.tv_sec = 0; tv.tv_usec = 10000;
        int r = select(0, &rfds, nullptr, nullptr, &tv);
        if (r > 0 && FD_ISSET(s, &rfds)) {
            char buf[8192];
            int b = recv(s, buf, sizeof(buf), 0);
            if (b > 0) {
                std::lock_guard<std::mutex> qlk(g_tcp_rx_q_mtx);
                g_tcp_rx_q.emplace_back(buf, buf + b);
                if (g_tcp_rx_q.size() > 1024) g_tcp_rx_q.pop_front();
            }
            else if (b <= 0) {
                std::lock_guard<std::mutex> lk(g_net_mtx);
                if (g_tcp == s) net_close_socket(g_tcp);
            }
        }
        Sleep(1);
    }
    debug_logf("[TCP-RX] Async receive thread stopped\n");
}

// ===================== CEQP 协议定义 =====================
#pragma pack(push, 1)
struct CEQP_FrameHeader {
    char magic[4];
    uint8_t version;
    uint8_t type;
    uint8_t flags;
    uint8_t reserved;
    uint32_t request_id;
    uint32_t payload_len;
};
#pragma pack(pop)

enum CEQP_MsgType : uint8_t {
    CEQP_HEARTBEAT_REQ = 0x01,
    CEQP_HEARTBEAT_RESP = 0x02,
    CEQP_READ_MEM_ADDR = 0x10,
    CEQP_WRITE_MEM_ADDR = 0x11,
    CEQP_READ_MOD_OFF = 0x12,
    CEQP_WRITE_MOD_OFF = 0x13,
    CEQP_READ_PTR_CHAIN = 0x14,
    CEQP_WRITE_PTR_CHAIN = 0x15,
    CEQP_GET_MOD_BASE = 0x20,
    CEQP_ERROR_RESP = 0x7F,
};

enum CEQP_TlvType : uint16_t {
    CEQP_TLV_ADDR = 0x0001,
    CEQP_TLV_LEN = 0x0002,
    CEQP_TLV_MODNAME = 0x0003,
    CEQP_TLV_OFFSET = 0x0004,
    CEQP_TLV_OFFSETS = 0x0005,
    CEQP_TLV_DATA = 0x0006,
    CEQP_TLV_DTYPE = 0x0007,
    CEQP_TLV_ERRCODE = 0x00FE,
    CEQP_TLV_ERRMSG = 0x00FF,
};

static SOCKET g_ceqp_listen = INVALID_SOCKET;
static SOCKET g_ceqp_client = INVALID_SOCKET;
static std::thread g_ceqp_thread;
static std::atomic<bool> g_ceqp_running(false);
static std::mutex g_ceqp_mtx;
static const int CEQP_IO_TIMEOUT_MS = 3000;
static const uint32_t CEQP_MAX_PAYLOAD = 1024 * 1024;

// ===================== TLV 编码/解码 =====================
static inline void ceqp_put16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back((uint8_t)(v & 0xFF));
    b.push_back((uint8_t)((v >> 8) & 0xFF));
}

static inline void ceqp_put32(std::vector<uint8_t>& b, uint32_t v) {
    for (int i = 0; i < 4; i++) b.push_back((uint8_t)((v >> (i * 8)) & 0xFF));
}

static inline void ceqp_put64(std::vector<uint8_t>& b, uint64_t v) {
    for (int i = 0; i < 8; i++) b.push_back((uint8_t)((v >> (i * 8)) & 0xFF));
}

static inline bool ceqp_get32(const uint8_t* p, uint32_t& v) {
    v = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
    return true;
}

static inline bool ceqp_get64(const uint8_t* p, uint64_t& v) {
    v = 0;
    for (int i = 0; i < 8; i++) v |= ((uint64_t)p[i]) << (8 * i);
    return true;
}

static inline void ceqp_append(std::vector<uint8_t>& b, const void* p, size_t n) {
    const uint8_t* q = (const uint8_t*)p;
    b.insert(b.end(), q, q + n);
}

static void ceqp_encodeTLV(std::vector<uint8_t>& out, uint16_t t, const std::vector<uint8_t>& v) {
    ceqp_put16(out, t);
    ceqp_put16(out, (uint16_t)v.size());
    ceqp_append(out, v.data(), v.size());
}

static void ceqp_encodeU32(std::vector<uint8_t>& out, uint16_t t, uint32_t x) {
    std::vector<uint8_t> v; ceqp_put32(v, x); ceqp_encodeTLV(out, t, v);
}

static void ceqp_encodeU64(std::vector<uint8_t>& out, uint16_t t, uint64_t x) {
    std::vector<uint8_t> v; ceqp_put64(v, x); ceqp_encodeTLV(out, t, v);
}

static void ceqp_encodeStr(std::vector<uint8_t>& out, uint16_t t, const std::string& s) {
    std::vector<uint8_t> v((const uint8_t*)s.data(), (const uint8_t*)s.data() + s.size());
    ceqp_encodeTLV(out, t, v);
}

static void ceqp_encodeBytes(std::vector<uint8_t>& out, uint16_t t, const std::vector<uint8_t>& d) {
    ceqp_encodeTLV(out, t, d);
}

static bool ceqp_extractBytes(const std::vector<uint8_t>& tlv, uint16_t t, std::vector<uint8_t>& out) {
    size_t i = 0;
    while (i + 4 <= tlv.size()) {
        uint16_t T = tlv[i] | (tlv[i + 1] << 8);
        uint16_t L = tlv[i + 2] | (tlv[i + 3] << 8);
        i += 4;
        if (i + L > tlv.size()) return false;
        if (T == t) {
            out.assign(tlv.begin() + i, tlv.begin() + i + L);
            return true;
        }
        i += L;
    }
    return false;
}

static bool ceqp_extractU64(const std::vector<uint8_t>& tlv, uint16_t t, uint64_t& out) {
    size_t i = 0;
    while (i + 4 <= tlv.size()) {
        uint16_t T = tlv[i] | (tlv[i + 1] << 8);
        uint16_t L = tlv[i + 2] | (tlv[i + 3] << 8);
        i += 4;
        if (i + L > tlv.size()) return false;
        if (T == t && L == 8) return ceqp_get64(&tlv[i], out);
        i += L;
    }
    return false;
}

static bool ceqp_extractU32(const std::vector<uint8_t>& tlv, uint16_t t, uint32_t& out) {
    size_t i = 0;
    while (i + 4 <= tlv.size()) {
        uint16_t T = tlv[i] | (tlv[i + 1] << 8);
        uint16_t L = tlv[i + 2] | (tlv[i + 3] << 8);
        i += 4;
        if (i + L > tlv.size()) return false;
        if (T == t && L == 4) return ceqp_get32(&tlv[i], out);
        i += L;
    }
    return false;
}

static bool ceqp_extractStr(const std::vector<uint8_t>& tlv, uint16_t t, std::string& out) {
    size_t i = 0;
    while (i + 4 <= tlv.size()) {
        uint16_t T = tlv[i] | (tlv[i + 1] << 8);
        uint16_t L = tlv[i + 2] | (tlv[i + 3] << 8);
        i += 4;
        if (i + L > tlv.size()) return false;
        if (T == t) {
            out.assign((const char*)&tlv[i], L);
            return true;
        }
        i += L;
    }
    return false;
}

// ===================== 网络传输 =====================
static bool ceqp_send_all(SOCKET s, const uint8_t* buf, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        int r = send(s, (const char*)buf + sent, (int)(n - sent), 0);
        if (r <= 0) return false;
        sent += r;
    }
    return true;
}

static bool ceqp_recv_exact_timeout(SOCKET s, uint8_t* buf, size_t n, int timeout_ms) {
    size_t got = 0;
    while (got < n) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(s, &rfds);
        timeval tv{}; tv.tv_sec = timeout_ms / 1000; tv.tv_usec = (timeout_ms % 1000) * 1000;
        int rv = select(0, &rfds, nullptr, nullptr, &tv);
        if (rv <= 0 || !FD_ISSET(s, &rfds)) return false;
        int r = recv(s, (char*)buf + got, (int)(n - got), 0);
        if (r <= 0) return false;
        got += r;
    }
    return true;
}

static void ceqp_send_error(SOCKET s, uint32_t reqid, uint32_t code, const std::string& msg) {
    debug_logf("[CEQP] Sending error: code=%u, msg=%s\n", code, msg.c_str());
    std::vector<uint8_t> pl;
    ceqp_encodeU32(pl, CEQP_TLV_ERRCODE, code);
    ceqp_encodeStr(pl, CEQP_TLV_ERRMSG, msg);
    CEQP_FrameHeader h{};
    memcpy(h.magic, "CEQP", 4);
    h.version = 0x01;
    h.type = CEQP_ERROR_RESP;
    h.request_id = reqid;
    h.payload_len = (uint32_t)pl.size();
    std::vector<uint8_t> buf;
    ceqp_append(buf, &h, sizeof(h));
    ceqp_append(buf, pl.data(), pl.size());
    ceqp_send_all(s, buf.data(), buf.size());
}

static void ceqp_send_ok(SOCKET s, uint8_t type, uint32_t reqid, const std::vector<uint8_t>& pl) {
    debug_logf("[CEQP] Sending OK: type=0x%02X, len=%zu\n", type, pl.size());
    CEQP_FrameHeader h{};
    memcpy(h.magic, "CEQP", 4);
    h.version = 0x01;
    h.type = type;
    h.request_id = reqid;
    h.payload_len = (uint32_t)pl.size();
    std::vector<uint8_t> buf;
    ceqp_append(buf, &h, sizeof(h));
    if (!pl.empty()) ceqp_append(buf, pl.data(), pl.size());
    ceqp_send_all(s, buf.data(), buf.size());
}

// ===================== 模块查找 =====================
static bool ceqp_find_module_base(const std::string& name, uint64_t& base) {
    debug_logf("[CEQP] Finding module: %s\n", name.c_str());
    DWORD pid = *Exported.OpenedProcessID;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) {
        debug_logf("[CEQP] CreateToolhelp32Snapshot failed\n");
        return false;
    }

    std::string lowName = name;
    for (char& c : lowName) c = (char)tolower((unsigned char)c);

    bool found = false;
    MODULEENTRY32 me{};
    me.dwSize = sizeof(me);
    if (Module32First(snap, &me)) {
        do {
            std::string mod = tchar_to_utf8(me.szModule);
            for (char& c : mod) c = (char)tolower((unsigned char)c);
            if (mod == lowName) {
                base = (uint64_t)(uintptr_t)me.modBaseAddr;
                found = true;
                debug_logf("[CEQP] Module found: base=0x%llX\n", (unsigned long long)base);
                break;
            }
        } while (Module32Next(snap, &me));
    }

    CloseHandle(snap);
    if (!found) debug_logf("[CEQP] Module not found\n");
    return found;
}

// ===================== 跨架构内存读写 =====================
static bool ceqp_read_mem(uint64_t addr, uint32_t len, std::vector<uint8_t>& out) {
    debug_logf("[CEQP] Reading: addr=0x%llX, len=%u\n", (unsigned long long)addr, len);
    out.resize(len);

    HANDLE hProcess = *Exported.OpenedProcessHandle;
    ProcessArch arch = get_process_arch(hProcess);

    debug_logf("[CEQP] Target process arch: %s\n",
        arch == ARCH_X64 ? "x64" : (arch == ARCH_X86 ? "x86" : "unknown"));

    SIZE_T bytesRead = 0;
    BOOL success = FALSE;

#ifndef _WIN64
    // 32位插件尝试读取64位进程
    if (arch == ARCH_X64 && g_NtWow64ReadVirtualMemory64) {
        ULONG64 read64 = 0;
        NTSTATUS status = g_NtWow64ReadVirtualMemory64(
            hProcess, (PVOID64)addr, out.data(), len, &read64);
        success = NT_SUCCESS(status);
        bytesRead = (SIZE_T)read64;
        debug_logf("[CEQP] WOW64 read: status=0x%lX, bytes=%llu\n",
            status, (unsigned long long)read64);
    }
    else
#endif
    {
        // 标准读取（同架构或64位插件）
        success = (*Exported.ReadProcessMemory)(
            hProcess, (LPCVOID)(uintptr_t)addr, out.data(), len, &bytesRead);
        debug_logf("[CEQP] Standard read: success=%d, bytes=%llu\n",
            success, (unsigned long long)bytesRead);
    }

    if (!success || bytesRead == 0) {
        debug_logf("[CEQP] Read failed, error=%d\n", GetLastError());
        out.clear();
        return false;
    }

    if (bytesRead < len) {
        out.resize((size_t)bytesRead);
        debug_logf("[CEQP] Partial read: %llu of %u bytes\n",
            (unsigned long long)bytesRead, len);
    }
    return true;
}

static bool ceqp_write_mem(uint64_t addr, const std::vector<uint8_t>& data) {
    debug_logf("[CEQP] Writing: addr=0x%llX, len=%zu\n",
        (unsigned long long)addr, data.size());

    HANDLE hProcess = *Exported.OpenedProcessHandle;
    ProcessArch arch = get_process_arch(hProcess);

    SIZE_T bytesWritten = 0;
    BOOL success = FALSE;

#ifndef _WIN64
    // 32位插件尝试写入64位进程
    if (arch == ARCH_X64 && g_NtWow64WriteVirtualMemory64) {
        ULONG64 written64 = 0;
        NTSTATUS status = g_NtWow64WriteVirtualMemory64(
            hProcess, (PVOID64)addr, (PVOID)data.data(), data.size(), &written64);
        success = NT_SUCCESS(status);
        bytesWritten = (SIZE_T)written64;
        debug_logf("[CEQP] WOW64 write: status=0x%lX, bytes=%llu\n",
            status, (unsigned long long)written64);
    }
    else
#endif
    {
        // 标准写入（同架构或64位插件）
        success = WriteProcessMemory(
            hProcess, (LPVOID)(uintptr_t)addr, data.data(), data.size(), &bytesWritten);
        debug_logf("[CEQP] Standard write: success=%d, bytes=%llu\n",
            success, (unsigned long long)bytesWritten);
    }

    if (!success || bytesWritten != data.size()) {
        debug_logf("[CEQP] Write failed, error=%d\n", GetLastError());
        return false;
    }
    return true;
}

// ===================== CEQP 请求处理 =====================
static bool ceqp_handle(SOCKET s, uint8_t type, uint32_t reqid, const std::vector<uint8_t>& pl) {
    debug_logf("[CEQP] Handling: type=0x%02X, reqid=%u, len=%zu\n", type, reqid, pl.size());

    // 心跳
    if (type == CEQP_HEARTBEAT_REQ) {
        std::vector<uint8_t> ep;
        ceqp_send_ok(s, CEQP_HEARTBEAT_RESP, reqid, ep);
        return true;
    }

    // 获取模块基址
    if (type == CEQP_GET_MOD_BASE) {
        std::string mod;
        if (!ceqp_extractStr(pl, CEQP_TLV_MODNAME, mod)) {
            ceqp_send_error(s, reqid, 1, "modname missing");
            return false;
        }
        uint64_t base = 0;
        if (!ceqp_find_module_base(mod, base)) {
            ceqp_send_error(s, reqid, 2, "module not found");
            return false;
        }
        std::vector<uint8_t> ep;
        ceqp_encodeU64(ep, CEQP_TLV_ADDR, base);
        ceqp_send_ok(s, type, reqid, ep);
        return true;
    }

    // 读取内存（绝对地址）
    if (type == CEQP_READ_MEM_ADDR) {
        uint64_t addr = 0;
        uint32_t len = 0;
        if (!ceqp_extractU64(pl, CEQP_TLV_ADDR, addr) || !ceqp_extractU32(pl, CEQP_TLV_LEN, len)) {
            ceqp_send_error(s, reqid, 3, "addr/len missing");
            return false;
        }
        std::vector<uint8_t> data;
        if (!ceqp_read_mem(addr, len, data)) {
            ceqp_send_error(s, reqid, 4, "read failed");
            return false;
        }
        std::vector<uint8_t> ep;
        ceqp_encodeBytes(ep, CEQP_TLV_DATA, data);
        ceqp_send_ok(s, type, reqid, ep);
        return true;
    }

    // 写入内存（绝对地址）
    if (type == CEQP_WRITE_MEM_ADDR) {
        uint64_t addr = 0;
        std::vector<uint8_t> data;
        if (!ceqp_extractU64(pl, CEQP_TLV_ADDR, addr) || !ceqp_extractBytes(pl, CEQP_TLV_DATA, data)) {
            ceqp_send_error(s, reqid, 5, "addr/data missing");
            return false;
        }
        if (!ceqp_write_mem(addr, data)) {
            ceqp_send_error(s, reqid, 6, "write failed");
            return false;
        }
        std::vector<uint8_t> ep;
        ceqp_send_ok(s, type, reqid, ep);
        return true;
    }

    // 读取指针链
    if (type == CEQP_READ_PTR_CHAIN) {
        uint64_t addr = 0;
        std::vector<uint8_t> offsBytes;
        if (!ceqp_extractU64(pl, CEQP_TLV_ADDR, addr) || !ceqp_extractBytes(pl, CEQP_TLV_OFFSETS, offsBytes)) {
            ceqp_send_error(s, reqid, 13, "addr/offsets missing");
            return false;
        }

        // 解析偏移
        if (offsBytes.size() % 8 != 0) {
            debug_logf("[CEQP] Invalid offsets size: %zu\n", offsBytes.size());
            ceqp_send_error(s, reqid, 13, "invalid offsets size");
            return false;
        }

        std::vector<int64_t> offsets;
        for (size_t i = 0; i < offsBytes.size(); i += 8) {
            uint64_t t = 0;
            ceqp_get64(&offsBytes[i], t);
            offsets.push_back((int64_t)t);
        }

        debug_logf("[CEQP] Pointer chain: base=0x%llX, offset_count=%zu\n",
            (unsigned long long)addr, offsets.size());

        // 确定指针大小
        HANDLE hProcess = *Exported.OpenedProcessHandle;
        ProcessArch arch = get_process_arch(hProcess);
        size_t ptrSize = (arch == ARCH_X64) ? 8 : 4;

        debug_logf("[CEQP] Pointer size: %zu bytes (arch=%s)\n",
            ptrSize, arch == ARCH_X64 ? "x64" : "x86");

        // CE 标准指针链语义：[[[[base] + off0] + off1] + off2]...
        // 每一步：先解引用（读取指针），再加偏移
        uint64_t cur = addr;

        for (size_t k = 0; k < offsets.size(); ++k) {
            // 先解引用：读取当前地址的指针值
            debug_logf("[CEQP] Step %zu: Reading pointer at 0x%llX\n",
                k, (unsigned long long)cur);

            // 检查地址有效性
            if (cur < 0x10000) {
                debug_logf("[CEQP] Invalid address: 0x%llX (too low)\n", (unsigned long long)cur);
                char errbuf[128];
                sprintf_s(errbuf, "invalid address at step %zu: 0x%llX", k, (unsigned long long)cur);
                ceqp_send_error(s, reqid, 14, errbuf);
                return false;
            }

            std::vector<uint8_t> ptrData;
            if (!ceqp_read_mem(cur, (uint32_t)ptrSize, ptrData) || ptrData.size() < ptrSize) {
                debug_logf("[CEQP] Failed to read pointer at 0x%llX\n", (unsigned long long)cur);
                char errbuf[128];
                sprintf_s(errbuf, "ptr read failed at step %zu, addr=0x%llX", k, (unsigned long long)cur);
                ceqp_send_error(s, reqid, 14, errbuf);
                return false;
            }

            // 提取指针值
            uint64_t ptrValue = 0;
            if (ptrSize == 4) {
                uint32_t ptr32 = 0;
                ceqp_get32(ptrData.data(), ptr32);
                ptrValue = (uint64_t)ptr32;
                debug_logf("[CEQP] Step %zu: [0x%llX] => 0x%08X\n",
                    k, (unsigned long long)cur, (unsigned int)ptr32);
            }
            else {
                ceqp_get64(ptrData.data(), ptrValue);
                debug_logf("[CEQP] Step %zu: [0x%llX] => 0x%016llX\n",
                    k, (unsigned long long)cur, (unsigned long long)ptrValue);
            }

            // 验证指针值
            if (ptrValue == 0) {
                debug_logf("[CEQP] Pointer is NULL at step %zu\n", k);
                char errbuf[128];
                sprintf_s(errbuf, "NULL pointer at step %zu, addr=0x%llX", k, (unsigned long long)cur);
                ceqp_send_error(s, reqid, 14, errbuf);
                return false;
            }

            if (ptrValue < 0x10000) {
                debug_logf("[CEQP] Invalid pointer value 0x%llX at step %zu (too low)\n",
                    (unsigned long long)ptrValue, k);
                char errbuf[128];
                sprintf_s(errbuf, "invalid pointer value at step %zu: 0x%llX", k, (unsigned long long)ptrValue);
                ceqp_send_error(s, reqid, 14, errbuf);
                return false;
            }

            // 再加偏移
            cur = ptrValue + (uint64_t)offsets[k];
            debug_logf("[CEQP] Step %zu: 0x%llX + offset(0x%llX) = 0x%llX\n",
                k, (unsigned long long)ptrValue,
                (unsigned long long)offsets[k],
                (unsigned long long)cur);
        }

        // 读取最终数据
        uint32_t len = 0;
        if (!ceqp_extractU32(pl, CEQP_TLV_LEN, len)) len = (uint32_t)ptrSize;

        debug_logf("[CEQP] Final read: addr=0x%llX, len=%u\n", (unsigned long long)cur, len);

        std::vector<uint8_t> data;
        if (!ceqp_read_mem(cur, len, data)) {
            char errbuf[128];
            sprintf_s(errbuf, "final read failed at 0x%llX", (unsigned long long)cur);
            ceqp_send_error(s, reqid, 15, errbuf);
            return false;
        }

        // 构造响应
        std::vector<uint8_t> ep;
        ceqp_encodeBytes(ep, CEQP_TLV_DATA, data);
        ceqp_encodeU64(ep, CEQP_TLV_ADDR, cur);

        if (g_test_env) {
            std::string dtype = (ptrSize == 4) ? "ptr32" : "ptr64";
            ceqp_encodeStr(ep, CEQP_TLV_DTYPE, dtype);
            ceqp_encodeU32(ep, CEQP_TLV_LEN, len);
        }

        ceqp_send_ok(s, type, reqid, ep);
        debug_logf("[CEQP] Pointer chain completed successfully\n");
        return true;
    }

    // 写入指针链
    if (type == CEQP_WRITE_PTR_CHAIN) {
        uint64_t addr = 0;
        std::vector<uint8_t> offsBytes;
        std::vector<uint8_t> data;
        if (!ceqp_extractU64(pl, CEQP_TLV_ADDR, addr) ||
            !ceqp_extractBytes(pl, CEQP_TLV_OFFSETS, offsBytes) ||
            !ceqp_extractBytes(pl, CEQP_TLV_DATA, data)) {
            ceqp_send_error(s, reqid, 13, "addr/offsets/data missing");
            return false;
        }

        // 解析偏移
        if (offsBytes.size() % 8 != 0) {
            debug_logf("[CEQP] Invalid offsets size: %zu\n", offsBytes.size());
            ceqp_send_error(s, reqid, 13, "invalid offsets size");
            return false;
        }

        std::vector<int64_t> offsets;
        for (size_t i = 0; i < offsBytes.size(); i += 8) {
            uint64_t t = 0;
            ceqp_get64(&offsBytes[i], t);
            offsets.push_back((int64_t)t);
        }

        debug_logf("[CEQP] Pointer chain WRITE: base=0x%llX, offset_count=%zu, data_len=%zu\n",
            (unsigned long long)addr, offsets.size(), data.size());

        // 确定指针大小
        HANDLE hProcess = *Exported.OpenedProcessHandle;
        ProcessArch arch = get_process_arch(hProcess);
        size_t ptrSize = (arch == ARCH_X64) ? 8 : 4;

        debug_logf("[CEQP] Pointer size: %zu bytes (arch=%s)\n",
            ptrSize, arch == ARCH_X64 ? "x64" : "x86");

        // 解引用指针链得到最终地址
        uint64_t cur = addr;
        for (size_t k = 0; k < offsets.size(); ++k) {
            debug_logf("[CEQP] Step %zu: Reading pointer at 0x%llX\n",
                k, (unsigned long long)cur);

            if (cur < 0x10000) {
                debug_logf("[CEQP] Invalid address: 0x%llX (too low)\n", (unsigned long long)cur);
                char errbuf[128];
                sprintf_s(errbuf, "invalid address at step %zu: 0x%llX", k, (unsigned long long)cur);
                ceqp_send_error(s, reqid, 14, errbuf);
                return false;
            }

            std::vector<uint8_t> ptrData;
            if (!ceqp_read_mem(cur, (uint32_t)ptrSize, ptrData) || ptrData.size() < ptrSize) {
                debug_logf("[CEQP] Failed to read pointer at 0x%llX\n", (unsigned long long)cur);
                char errbuf[128];
                sprintf_s(errbuf, "ptr read failed at step %zu, addr=0x%llX", k, (unsigned long long)cur);
                ceqp_send_error(s, reqid, 14, errbuf);
                return false;
            }

            uint64_t ptrValue = 0;
            if (ptrSize == 4) {
                uint32_t ptr32 = 0;
                ceqp_get32(ptrData.data(), ptr32);
                ptrValue = (uint64_t)ptr32;
                debug_logf("[CEQP] Step %zu: [0x%llX] => 0x%08X\n",
                    k, (unsigned long long)cur, (unsigned int)ptr32);
            } else {
                ceqp_get64(ptrData.data(), ptrValue);
                debug_logf("[CEQP] Step %zu: [0x%llX] => 0x%016llX\n",
                    k, (unsigned long long)cur, (unsigned long long)ptrValue);
            }

            if (ptrValue == 0) {
                debug_logf("[CEQP] Pointer is NULL at step %zu\n", k);
                char errbuf[128];
                sprintf_s(errbuf, "NULL pointer at step %zu, addr=0x%llX", k, (unsigned long long)cur);
                ceqp_send_error(s, reqid, 14, errbuf);
                return false;
            }
            if (ptrValue < 0x10000) {
                debug_logf("[CEQP] Invalid pointer value 0x%llX at step %zu (too low)\n",
                    (unsigned long long)ptrValue, k);
                char errbuf[128];
                sprintf_s(errbuf, "invalid pointer value at step %zu: 0x%llX", k, (unsigned long long)ptrValue);
                ceqp_send_error(s, reqid, 14, errbuf);
                return false;
            }

            cur = ptrValue + (uint64_t)offsets[k];
            debug_logf("[CEQP] Step %zu: 0x%llX + offset(0x%llX) = 0x%llX\n",
                k, (unsigned long long)ptrValue,
                (unsigned long long)offsets[k],
                (unsigned long long)cur);
        }

        // 最终写入
        debug_logf("[CEQP] Final write: addr=0x%llX, len=%zu\n", (unsigned long long)cur, data.size());
        if (!ceqp_write_mem(cur, data)) {
            char errbuf[128];
            sprintf_s(errbuf, "final write failed at 0x%llX", (unsigned long long)cur);
            ceqp_send_error(s, reqid, 16, errbuf);
            return false;
        }

        std::vector<uint8_t> ep;
        // 可选：返回最终地址，便于调试
        ceqp_encodeU64(ep, CEQP_TLV_ADDR, cur);
        ceqp_send_ok(s, type, reqid, ep);
        debug_logf("[CEQP] Pointer chain WRITE completed successfully\n");
        return true;
    }

    // 未知类型
    debug_logf("[CEQP] Unknown request type: 0x%02X\n", type);
    ceqp_send_error(s, reqid, 100, "unknown type");
    return false;
}

// ===================== CEQP 服务器 =====================
static void ceqp_server_loop() {
    debug_logf("[CEQP] Server loop started\n");
    while (g_ceqp_running) {
        SOCKET c = INVALID_SOCKET;
        {
            std::lock_guard<std::mutex> lk(g_ceqp_mtx);
            SOCKET l = g_ceqp_listen;
            if (l == INVALID_SOCKET) { Sleep(50); continue; }

            fd_set rfds; FD_ZERO(&rfds); FD_SET(l, &rfds);
            timeval tv{}; tv.tv_sec = 0; tv.tv_usec = 50000;
            int rv = select(0, &rfds, nullptr, nullptr, &tv);

            if (rv > 0 && FD_ISSET(l, &rfds)) {
                sockaddr_in cli{}; int clen = sizeof(cli);
                c = accept(l, (sockaddr*)&cli, &clen);
                if (c != INVALID_SOCKET) {
                    char client_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &cli.sin_addr, client_ip, INET_ADDRSTRLEN);
                    debug_logf("[CEQP] Client connected: %s:%d\n",
                        client_ip, ntohs(cli.sin_port));
                }
            }
        }

        if (c == INVALID_SOCKET) continue;

        // 禁用 Nagle 算法
        int flag = 1;
        setsockopt(c, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));

        {
            std::lock_guard<std::mutex> lk(g_ceqp_mtx);
            net_close_socket(g_ceqp_client);
            g_ceqp_client = c;
        }

        // 会话循环
        debug_logf("[CEQP] Session started\n");
        while (g_ceqp_running) {
            CEQP_FrameHeader h{};
            if (!ceqp_recv_exact_timeout(c, (uint8_t*)&h, sizeof(h), CEQP_IO_TIMEOUT_MS)) {
                debug_logf("[CEQP] Header recv failed/timeout\n");
                break;
            }

            if (memcmp(h.magic, "CEQP", 4) != 0) {
                debug_logf("[CEQP] Invalid magic\n");
                break;
            }

            if (h.version != 0x01) {
                debug_logf("[CEQP] Invalid version: 0x%02X\n", h.version);
                ceqp_send_error(c, 0, 101, "bad version");
                break;
            }

            if (h.payload_len > CEQP_MAX_PAYLOAD) {
                debug_logf("[CEQP] Payload too large: %u\n", h.payload_len);
                ceqp_send_error(c, h.request_id, 102, "payload too large");
                break;
            }

            std::vector<uint8_t> pl;
            pl.resize(h.payload_len);
            if (h.payload_len > 0 && !ceqp_recv_exact_timeout(c, pl.data(), pl.size(), CEQP_IO_TIMEOUT_MS)) {
                debug_logf("[CEQP] Payload recv failed\n");
                break;
            }

            ceqp_handle(c, h.type, h.request_id, pl);
        }

        debug_logf("[CEQP] Session ended\n");
        {
            std::lock_guard<std::mutex> lk(g_ceqp_mtx);
            net_close_socket(g_ceqp_client);
        }
    }
    debug_logf("[CEQP] Server loop stopped\n");
}

static bool ceqp_start(int port) {
    debug_logf("[CEQP] Starting server on port %d...\n", port);
    if (!net_init()) return false;

    std::lock_guard<std::mutex> lk(g_ceqp_mtx);
    if (g_ceqp_running) {
        debug_logf("[CEQP] Already running\n");
        return true;
    }

    net_close_socket(g_ceqp_listen);

    SOCKET l = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (l == INVALID_SOCKET) {
        debug_logf("[CEQP] socket() failed\n");
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    int reuse = 1;
    setsockopt(l, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    if (bind(l, (sockaddr*)&addr, sizeof(addr)) != 0) {
        debug_logf("[CEQP] bind() failed\n");
        closesocket(l);
        return false;
    }

    if (listen(l, 4) != 0) {
        debug_logf("[CEQP] listen() failed\n");
        closesocket(l);
        return false;
    }

    g_ceqp_listen = l;
    g_ceqp_running = true;

    try {
        g_ceqp_thread = std::thread(ceqp_server_loop);
    }
    catch (...) {
        debug_logf("[CEQP] Thread creation failed\n");
        g_ceqp_running = false;
        net_close_socket(g_ceqp_listen);
        return false;
    }

    debug_logf("[CEQP] Server started successfully\n");
    return true;
}

static void ceqp_stop() {
    debug_logf("[CEQP] Stopping server...\n");
    bool needJoin = false;
    {
        std::lock_guard<std::mutex> lk(g_ceqp_mtx);
        needJoin = g_ceqp_running;
        g_ceqp_running = false;
        net_close_socket(g_ceqp_listen);
        net_close_socket(g_ceqp_client);
    }
    if (needJoin && g_ceqp_thread.joinable()) {
        g_ceqp_thread.join();
        debug_logf("[CEQP] Server stopped\n");
    }
}

// ===================== Lua API =====================
extern "C" {
    static int lua_CEQPStart(lua_State* L) {
        int port = (int)luaL_optinteger(L, 1, 9178);
        bool ok = ceqp_start(port);
        lua_pushboolean(L, ok ? TRUE : FALSE);
        return 1;
    }

    static int lua_CEQPStop(lua_State* L) {
        ceqp_stop();
        lua_pushboolean(L, TRUE);
        return 1;
    }

    static int lua_TCPConnect(lua_State* L) {
        const char* host = luaL_checkstring(L, 1);
        int port = (int)luaL_checkinteger(L, 2);
        bool ok = tcp_connect(host, port);
        lua_pushboolean(L, ok);
        return 1;
    }

    static int lua_TCPSend(lua_State* L) {
        size_t len = 0;
        const char* data = luaL_checklstring(L, 1, &len);
        int sent = tcp_send(data, (int)len);
        lua_pushinteger(L, sent);
        return 1;
    }

    static int lua_TCPRecv(lua_State* L) {
        int maxlen = (int)luaL_optinteger(L, 1, 4096);
        int timeout_ms = (int)luaL_optinteger(L, 2, 1000);
        std::string r = tcp_recv(maxlen, timeout_ms);
        lua_pushlstring(L, r.data(), r.size());
        return 1;
    }

    static int lua_TCPClose(lua_State* L) {
        tcp_close();
        return 0;
    }

    static int lua_TCPStartRecv(lua_State* L) {
        if (g_tcp_rx_running) {
            lua_pushboolean(L, TRUE);
            return 1;
        }
        g_tcp_rx_running = true;
        try {
            g_tcp_rx_thread = std::thread(tcp_rx_loop);
        }
        catch (...) {
            g_tcp_rx_running = false;
            lua_pushboolean(L, FALSE);
            return 1;
        }
        lua_pushboolean(L, TRUE);
        return 1;
    }

    static int lua_TCPStopRecv(lua_State* L) {
        if (g_tcp_rx_running) {
            g_tcp_rx_running = false;
            if (g_tcp_rx_thread.joinable()) g_tcp_rx_thread.join();
        }
        lua_pushboolean(L, TRUE);
        return 1;
    }

    static int lua_TCPRecvNonblocking(lua_State* L) {
        int maxlen = (int)luaL_optinteger(L, 1, 4096);
        std::string out;
        {
            std::lock_guard<std::mutex> qlk(g_tcp_rx_q_mtx);
            if (!g_tcp_rx_q.empty()) {
                out = std::move(g_tcp_rx_q.front());
                g_tcp_rx_q.pop_front();
            }
        }
        if ((int)out.size() > maxlen) out.resize(maxlen);
        lua_pushlstring(L, out.data(), out.size());
        return 1;
    }

    static int lua_TCPSetNoDelay(lua_State* L) {
        int enable = lua_toboolean(L, 1);
        std::lock_guard<std::mutex> lock(g_net_mtx);
        if (g_tcp == INVALID_SOCKET) {
            lua_pushboolean(L, FALSE);
            return 1;
        }
        int flag = enable ? 1 : 0;
        setsockopt(g_tcp, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));
        lua_pushboolean(L, TRUE);
        return 1;
    }
}

// ===================== CE 插件入口 =====================
BOOL __stdcall CEPlugin_GetVersion(PPluginVersion pv, int sizeofpluginversion) {
    pv->version = CESDK_VERSION;
    pv->pluginname = (char*)"TCP/CEQP Plugin (Cross-arch Memory Access) - Author: Lun. QQ:1596534228 github:Lun-OS";
    return TRUE;
}

BOOL __stdcall CEPlugin_InitializePlugin(PExportedFunctions ef, int pluginid) {
    Exported = *ef;
    if (Exported.sizeofExportedFunctions != sizeof(Exported))
        return FALSE;

    // 初始化日志
    if (!init_log_file()) {
        OutputDebugStringA("[CEQP] Failed to init log file\n");
    }
    else {
        debug_logf("[CEQP] Log file: %s\n", g_log_path.c_str());
    }

    // 初始化 WOW64 支持
    ceqp_init_wow64_symbols();

    // 检测插件架构
#ifdef _WIN64
    debug_logf("[CEQP] Plugin architecture: x64\n");
#else
    debug_logf("[CEQP] Plugin architecture: x86\n");
#endif

    // 读取环境变量
    char ev[32] = { 0 };
    DWORD n = GetEnvironmentVariableA("CEQP_TEST_ENV", ev, sizeof(ev) - 1);
    if (n > 0) {
        std::string v(ev);
        for (char& c : v) c = (char)tolower((unsigned char)c);
        g_test_env = (v == "1" || v == "true" || v == "yes" || v == "on");
    }

    net_init();

    // 注册 Lua API
    lua_State* L = ef->GetLuaState();
    lua_register(L, "pluginTCPConnect", lua_TCPConnect);
    lua_register(L, "pluginTCPSend", lua_TCPSend);
    lua_register(L, "pluginTCPRecv", lua_TCPRecv);
    lua_register(L, "pluginTCPClose", lua_TCPClose);
    lua_register(L, "pluginTCPStartRecv", lua_TCPStartRecv);
    lua_register(L, "pluginTCPStopRecv", lua_TCPStopRecv);
    lua_register(L, "pluginTCPRecvNonblocking", lua_TCPRecvNonblocking);
    lua_register(L, "pluginTCPSetNoDelay", lua_TCPSetNoDelay);
    lua_register(L, "QAQ", lua_CEQPStart);
    lua_register(L, "stopQAQ", lua_CEQPStop);
    lua_register(L, "pluginCEQPSetTestEnv", lua_CEQPSetTestEnv);

    debug_logf("[CEQP] Plugin initialized successfully\n");
    return TRUE;
}

BOOL __stdcall CEPlugin_DisablePlugin(void) {
    debug_logf("[CEQP] Disabling plugin...\n");

    if (g_tcp_rx_running) {
        g_tcp_rx_running = false;
        if (g_tcp_rx_thread.joinable()) g_tcp_rx_thread.join();
    }

    ceqp_stop();
    tcp_close();

    if (g_wsa_inited) {
        WSACleanup();
        g_wsa_inited = false;
    }

    close_log_file();
    return TRUE;
}

static std::string tchar_to_utf8(const TCHAR* p) {
#ifdef UNICODE
    if (!p) return std::string();
    int len = WideCharToMultiByte(CP_UTF8, 0, p, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return std::string();
    std::string s;
    s.resize(len - 1);
    WideCharToMultiByte(CP_UTF8, 0, p, -1, &s[0], len, nullptr, nullptr);
    return s;
#else
    return std::string(p ? p : "");
#endif
}

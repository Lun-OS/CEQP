#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <string>
#include <vector>
#include <cstdint>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// CEQP协议常量定义
namespace CEQP {
    // 消息类型
    constexpr uint8_t PING = 0x01;
    constexpr uint8_t PING_RESP = 0x02;
    constexpr uint8_t READ_MEM = 0x10;
    constexpr uint8_t WRITE_MEM = 0x11;
    constexpr uint8_t READ_MOD_OFFSET = 0x12;
    constexpr uint8_t WRITE_MOD_OFFSET = 0x13;
    constexpr uint8_t READ_PTR_CHAIN = 0x14;
    constexpr uint8_t WRITE_PTR_CHAIN = 0x15;
    constexpr uint8_t GET_MOD_BASE = 0x20;
    constexpr uint8_t ERROR_RESP = 0x7F;

    // TLV类型
    constexpr uint16_t TLV_ADDR = 0x0001;
    constexpr uint16_t TLV_LEN = 0x0002;
    constexpr uint16_t TLV_MODNAME = 0x0003;
    constexpr uint16_t TLV_OFFSET = 0x0004;
    constexpr uint16_t TLV_OFFSETS = 0x0005;
    constexpr uint16_t TLV_DATA = 0x0006;
    constexpr uint16_t TLV_DTYPE = 0x0007;
    constexpr uint16_t TLV_ERRCODE = 0x00FE;
    constexpr uint16_t TLV_ERRMSG = 0x00FF;

    // 协议版本
    constexpr uint8_t VERSION = 0x01;

    // 最大负载大小 (1MB)
    constexpr uint32_t MAX_PAYLOAD_SIZE = 1024 * 1024;
}

// CEQP帧头结构
#pragma pack(push, 1)
struct CEQP_FrameHeader {
    char magic[4];        // 'C', 'E', 'Q', 'P'
    uint8_t version;      // 协议版本
    uint8_t type;         // 消息类型
    uint8_t flags;        // 标志位
    uint8_t reserved;     // 保留字段
    uint32_t request_id;  // 请求ID（小端序）
    uint32_t payload_len; // 负载长度（小端序）
};
#pragma pack(pop)

/**
 * CETCP类 - CEQP协议客户端实现
 * 提供与Cheat Engine TCP_UDP插件通信的完整功能
 */
class CETCP {
public:
    CETCP();
    ~CETCP();

    // ============ 连接管理 ============

    /**
     * 连接到服务器
     * @param host 服务器地址（如"127.0.0.1"）
     * @param port 端口号（默认9178）
     * @return 成功返回true，失败返回false
     */
    bool connect(const std::string& host, int port = 9178);

    /**
     * 断开连接
     */
    void disconnect();

    /**
     * 检查连接状态
     * @return 已连接返回true，否则返回false
     */
    bool isConnected() const { return connected; }

    /**
     * 发送心跳包
     * @return 成功返回true，失败返回false
     */
    bool ping();

    // ============ 内存操作 ============

    /**
     * 读取内存
     * @param address 目标地址
     * @param length 读取长度
     * @param data 输出缓冲区
     * @return 成功返回true，失败返回false
     */
    bool readMemory(uint64_t address, uint32_t length, std::vector<uint8_t>& data);

    /**
     * 写入内存
     * @param address 目标地址
     * @param data 要写入的数据
     * @return 成功返回true，失败返回false
     */
    bool writeMemory(uint64_t address, const std::vector<uint8_t>& data);

    // ============ 模块操作 ============

    /**
     * 获取模块基址
     * @param moduleName 模块名称（如"kernel32.dll"）
     * @param baseAddress 输出基址
     * @return 成功返回true，失败返回false
     */
    bool getModuleBase(const std::string& moduleName, uint64_t& baseAddress);

    /**
     * 读取模块偏移处的内存
     * @param moduleName 模块名称
     * @param offset 偏移量
     * @param length 读取长度
     * @param data 输出缓冲区
     * @return 成功返回true，失败返回false
     */
    bool readModuleOffset(const std::string& moduleName, int64_t offset,
        uint32_t length, std::vector<uint8_t>& data);

    /**
     * 写入模块偏移处的内存
     * @param moduleName 模块名称
     * @param offset 偏移量
     * @param data 要写入的数据
     * @return 成功返回true，失败返回false
     */
    bool writeModuleOffset(const std::string& moduleName, int64_t offset,
        const std::vector<uint8_t>& data);

    // ============ 指针链操作 ============

    /**
     * 读取指针链
     * @param baseAddress 基址
     * @param offsets 偏移量数组
     * @param length 最终读取长度
     * @param data 输出缓冲区
     * @return 成功返回true，失败返回false
     */
    bool readPointerChain(uint64_t baseAddress, const std::vector<int64_t>& offsets,
        uint32_t length, std::vector<uint8_t>& data, const std::string& dtype = "");

    /**
     * 写入指针链
     * @param baseAddress 基址
     * @param offsets 偏移量数组
     * @param data 要写入的数据
     * @return 成功返回true，失败返回false
     */
    bool writePointerChain(uint64_t baseAddress, const std::vector<int64_t>& offsets,
        const std::vector<uint8_t>& data, const std::string& dtype = "");

    // ============ 工具函数 ============

    /**
     * 获取最后的错误信息
     * @return 错误信息字符串
     */
    std::string getLastError() const { return lastError; }

    /**
     * 字节数组转十六进制字符串
     * @param data 字节数组
     * @return 十六进制字符串
     */
    static std::string bytesToHex(const std::vector<uint8_t>& data);

    /**
     * 十六进制字符串转字节数组
     * @param hex 十六进制字符串
     * @return 字节数组
     */
    static std::vector<uint8_t> hexToBytes(const std::string& hex);

    /**
     * 解析地址字符串（支持十六进制）
     * @param addrStr 地址字符串（如"0x400000"或"400000"）
     * @return 地址值
     */
    static uint64_t parseAddress(const std::string& addrStr);

    /**
     * 解析偏移字符串（支持十六进制和负数）
     * @param offsetStr 偏移字符串（如"0x10"或"-0x20"）
     * @return 偏移值
     */
    static int64_t parseOffset(const std::string& offsetStr);

private:
    SOCKET sock;
    bool connected;
    uint32_t nextRequestId;
    std::string lastError;
    bool wsaInitialized;

    // 内部辅助函数
    void setLastError(const std::string& error);
    bool sendFrame(uint8_t type, const std::vector<uint8_t>& payload);
    bool receiveFrame(CEQP_FrameHeader& header, std::vector<uint8_t>& payload);

    // TLV编码/解码
    void encodeTLV(std::vector<uint8_t>& buffer, uint16_t type, const void* data, uint16_t length);
    void encodeU64(std::vector<uint8_t>& buffer, uint16_t type, uint64_t value);
    void encodeU32(std::vector<uint8_t>& buffer, uint16_t type, uint32_t value);
    void encodeI64(std::vector<uint8_t>& buffer, uint16_t type, int64_t value);
    void encodeString(std::vector<uint8_t>& buffer, uint16_t type, const std::string& str);
    void encodeBytes(std::vector<uint8_t>& buffer, uint16_t type, const std::vector<uint8_t>& data);
    void encodeI64Array(std::vector<uint8_t>& buffer, uint16_t type, const std::vector<int64_t>& offsets);

    bool extractTLV(const std::vector<uint8_t>& payload, uint16_t type, std::vector<uint8_t>& value);
    bool extractU64(const std::vector<uint8_t>& payload, uint16_t type, uint64_t& value);
    bool extractU32(const std::vector<uint8_t>& payload, uint16_t type, uint32_t& value);
    bool extractString(const std::vector<uint8_t>& payload, uint16_t type, std::string& value);
    bool extractBytes(const std::vector<uint8_t>& payload, uint16_t type, std::vector<uint8_t>& value);
};

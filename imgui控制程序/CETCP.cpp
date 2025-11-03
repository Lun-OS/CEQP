#include "CETCP.h"
#include <sstream>
#include <iomanip>
#include <cstring>

// 辅助函数：小端序编码
static void writeLE16(uint8_t* buf, uint16_t val) {
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
}

static void writeLE32(uint8_t* buf, uint32_t val) {
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
    buf[2] = (val >> 16) & 0xFF;
    buf[3] = (val >> 24) & 0xFF;
}

static void writeLE64(uint8_t* buf, uint64_t val) {
    for (int i = 0; i < 8; i++) {
        buf[i] = (val >> (i * 8)) & 0xFF;
    }
}

static uint16_t readLE16(const uint8_t* buf) {
    return buf[0] | (buf[1] << 8);
}

static uint32_t readLE32(const uint8_t* buf) {
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

static uint64_t readLE64(const uint8_t* buf) {
    uint64_t val = 0;
    for (int i = 0; i < 8; i++) {
        val |= ((uint64_t)buf[i]) << (i * 8);
    }
    return val;
}

// 构造函数
CETCP::CETCP() : sock(INVALID_SOCKET), connected(false), nextRequestId(1), wsaInitialized(false) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
        wsaInitialized = true;
    }
}

// 析构函数
CETCP::~CETCP() {
    disconnect();
    if (wsaInitialized) {
        WSACleanup();
    }
}

// 连接到服务器
bool CETCP::connect(const std::string& host, int port) {
    if (connected) {
        disconnect();
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        setLastError("Socket creation failed");
        return false;
    }

    // 禁用Nagle算法以减少延迟
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));

    // 设置接收超时
    DWORD timeout = 3000; // 3秒
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr);

    if (::connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        setLastError("Failed to connect to server");
        closesocket(sock);
        sock = INVALID_SOCKET;
        return false;
    }

    connected = true;
    lastError.clear();
    return true;
}

// 写入指针链
bool CETCP::writePointerChain(uint64_t baseAddress, const std::vector<int64_t>& offsets,
    const std::vector<uint8_t>& data, const std::string& dtype) {
    if (!connected) {
        setLastError("未连接到服务器");
        return false;
    }

    // 构建请求负载
    std::vector<uint8_t> payload;
    encodeU64(payload, CEQP::TLV_ADDR, baseAddress);
    encodeI64Array(payload, CEQP::TLV_OFFSETS, offsets);
    encodeBytes(payload, CEQP::TLV_DATA, data);
    if (!dtype.empty()) {
        encodeString(payload, CEQP::TLV_DTYPE, dtype);
    }

    if (!sendFrame(CEQP::WRITE_PTR_CHAIN, payload)) {
        return false;
    }

    // 接收响应
    CEQP_FrameHeader header;
    std::vector<uint8_t> respPayload;
    if (!receiveFrame(header, respPayload)) {
        return false;
    }

    // 检查错误响应
    if (header.type == CEQP::ERROR_RESP) {
        uint32_t errorCode;
        std::string errorMsg;
        extractU32(respPayload, CEQP::TLV_ERRCODE, errorCode);
        extractString(respPayload, CEQP::TLV_ERRMSG, errorMsg);
        setLastError("服务器错误 " + std::to_string(errorCode) + ": " + errorMsg);
        return false;
    }

    return true;
}

// 断开连接
void CETCP::disconnect() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    connected = false;
}

// 发送心跳包
bool CETCP::ping() {
    if (!connected) {
        setLastError("Not connected to server");
        return false;
    }

    std::vector<uint8_t> emptyPayload;
    if (!sendFrame(CEQP::PING, emptyPayload)) {
        return false;
    }

    CEQP_FrameHeader header;
    std::vector<uint8_t> payload;
    if (!receiveFrame(header, payload)) {
        return false;
    }

    if (header.type != CEQP::PING_RESP) {
        setLastError("Unexpected heartbeat response type");
        return false;
    }

    return true;
}

// 读取内存
bool CETCP::readMemory(uint64_t address, uint32_t length, std::vector<uint8_t>& data) {
    if (!connected) {
        setLastError("Not connected to server");
        return false;
    }

    // 构建请求负载
    std::vector<uint8_t> payload;
    encodeU64(payload, CEQP::TLV_ADDR, address);
    encodeU32(payload, CEQP::TLV_LEN, length);

    if (!sendFrame(CEQP::READ_MEM, payload)) {
        return false;
    }

    // 接收响应
    CEQP_FrameHeader header;
    std::vector<uint8_t> respPayload;
    if (!receiveFrame(header, respPayload)) {
        return false;
    }

    // 检查错误响应
    if (header.type == CEQP::ERROR_RESP) {
        uint32_t errorCode;
        std::string errorMsg;
        if (extractU32(respPayload, CEQP::TLV_ERRCODE, errorCode)) {
            extractString(respPayload, CEQP::TLV_ERRMSG, errorMsg);
            setLastError(std::string("Server error ") + std::to_string(errorCode) + ": " + errorMsg);
        }
        else {
            setLastError("Unknown server error");
        }
        return false;
    }

    // 提取数据
    if (!extractBytes(respPayload, CEQP::TLV_DATA, data)) {
        setLastError("Failed to extract data from response");
        return false;
    }

    return true;
}

// 写入内存
bool CETCP::writeMemory(uint64_t address, const std::vector<uint8_t>& data) {
    if (!connected) {
        setLastError("Not connected to server");
        return false;
    }

    // 构建请求负载
    std::vector<uint8_t> payload;
    encodeU64(payload, CEQP::TLV_ADDR, address);
    encodeBytes(payload, CEQP::TLV_DATA, data);

    if (!sendFrame(CEQP::WRITE_MEM, payload)) {
        return false;
    }

    // 接收响应
    CEQP_FrameHeader header;
    std::vector<uint8_t> respPayload;
    if (!receiveFrame(header, respPayload)) {
        return false;
    }

    // 检查错误响应
    if (header.type == CEQP::ERROR_RESP) {
        uint32_t errorCode;
        std::string errorMsg;
        extractU32(respPayload, CEQP::TLV_ERRCODE, errorCode);
        extractString(respPayload, CEQP::TLV_ERRMSG, errorMsg);
        setLastError(std::string("Server error ") + std::to_string(errorCode) + ": " + errorMsg);
        return false;
    }

    return true;
}

// 获取模块基址
bool CETCP::getModuleBase(const std::string& moduleName, uint64_t& baseAddress) {
    if (!connected) {
        setLastError("Not connected to server");
        return false;
    }

    // 构建请求负载
    std::vector<uint8_t> payload;
    encodeString(payload, CEQP::TLV_MODNAME, moduleName);

    if (!sendFrame(CEQP::GET_MOD_BASE, payload)) {
        return false;
    }

    // 接收响应
    CEQP_FrameHeader header;
    std::vector<uint8_t> respPayload;
    if (!receiveFrame(header, respPayload)) {
        return false;
    }

    // 检查错误响应
    if (header.type == CEQP::ERROR_RESP) {
        uint32_t errorCode;
        std::string errorMsg;
        extractU32(respPayload, CEQP::TLV_ERRCODE, errorCode);
        extractString(respPayload, CEQP::TLV_ERRMSG, errorMsg);
        setLastError(std::string("Server error ") + std::to_string(errorCode) + ": " + errorMsg);
        return false;
    }

    // 提取基址
    if (!extractU64(respPayload, CEQP::TLV_ADDR, baseAddress)) {
        setLastError("Failed to extract base address from response");
        return false;
    }

    return true;
}

// 读取模块偏移
bool CETCP::readModuleOffset(const std::string& moduleName, int64_t offset,
    uint32_t length, std::vector<uint8_t>& data) {
    if (!connected) {
        setLastError("Not connected to server");
        return false;
    }

    // 构建请求负载
    std::vector<uint8_t> payload;
    encodeString(payload, CEQP::TLV_MODNAME, moduleName);
    encodeI64(payload, CEQP::TLV_OFFSET, offset);
    encodeU32(payload, CEQP::TLV_LEN, length);

    if (!sendFrame(CEQP::READ_MOD_OFFSET, payload)) {
        return false;
    }

    // 接收响应
    CEQP_FrameHeader header;
    std::vector<uint8_t> respPayload;
    if (!receiveFrame(header, respPayload)) {
        return false;
    }

    // 检查错误响应
    if (header.type == CEQP::ERROR_RESP) {
        uint32_t errorCode;
        std::string errorMsg;
        extractU32(respPayload, CEQP::TLV_ERRCODE, errorCode);
        extractString(respPayload, CEQP::TLV_ERRMSG, errorMsg);
        setLastError(std::string("Server error ") + std::to_string(errorCode) + ": " + errorMsg);
        return false;
    }

    // 提取数据
    if (!extractBytes(respPayload, CEQP::TLV_DATA, data)) {
        setLastError("Failed to extract data from response");
        return false;
    }

    return true;
}

// 写入模块偏移
bool CETCP::writeModuleOffset(const std::string& moduleName, int64_t offset,
    const std::vector<uint8_t>& data) {
    if (!connected) {
        setLastError("未连接到服务器");
        return false;
    }

    // 构建请求负载
    std::vector<uint8_t> payload;
    encodeString(payload, CEQP::TLV_MODNAME, moduleName);
    encodeI64(payload, CEQP::TLV_OFFSET, offset);
    encodeBytes(payload, CEQP::TLV_DATA, data);

    if (!sendFrame(CEQP::WRITE_MOD_OFFSET, payload)) {
        return false;
    }

    // 接收响应
    CEQP_FrameHeader header;
    std::vector<uint8_t> respPayload;
    if (!receiveFrame(header, respPayload)) {
        return false;
    }

    // 检查错误响应
    if (header.type == CEQP::ERROR_RESP) {
        uint32_t errorCode;
        std::string errorMsg;
        extractU32(respPayload, CEQP::TLV_ERRCODE, errorCode);
        extractString(respPayload, CEQP::TLV_ERRMSG, errorMsg);
        setLastError("服务器错误 " + std::to_string(errorCode) + ": " + errorMsg);
        return false;
    }

    return true;
}

// 读取指针链
bool CETCP::readPointerChain(uint64_t baseAddress, const std::vector<int64_t>& offsets,
    uint32_t length, std::vector<uint8_t>& data, const std::string& dtype) {
    if (!connected) {
        setLastError("未连接到服务器");
        return false;
    }

    // 构建请求负载
    std::vector<uint8_t> payload;
    encodeU64(payload, CEQP::TLV_ADDR, baseAddress);
    encodeI64Array(payload, CEQP::TLV_OFFSETS, offsets);
    encodeU32(payload, CEQP::TLV_LEN, length);
    if (!dtype.empty()) {
        encodeString(payload, CEQP::TLV_DTYPE, dtype);
    }

    if (!sendFrame(CEQP::READ_PTR_CHAIN, payload)) {
        return false;
    }

    // 接收响应
    CEQP_FrameHeader header;
    std::vector<uint8_t> respPayload;
    if (!receiveFrame(header, respPayload)) {
        return false;
    }

    // 检查错误响应
    if (header.type == CEQP::ERROR_RESP) {
        uint32_t errorCode;
        std::string errorMsg;
        extractU32(respPayload, CEQP::TLV_ERRCODE, errorCode);
        extractString(respPayload, CEQP::TLV_ERRMSG, errorMsg);
        setLastError("服务器错误 " + std::to_string(errorCode) + ": " + errorMsg);
        return false;
    }

    // 提取数据
    if (!extractBytes(respPayload, CEQP::TLV_DATA, data)) {
        setLastError("无法从响应中提取数据");
        return false;
    }

    return true;
}

// ============ 工具函数实现 ============

std::string CETCP::bytesToHex(const std::vector<uint8_t>& data) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t byte : data) {
        ss << std::setw(2) << (int)byte;
    }
    return ss.str();
}

std::vector<uint8_t> CETCP::hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteStr = hex.substr(i, 2);
        uint8_t byte = (uint8_t)strtol(byteStr.c_str(), nullptr, 16);
        bytes.push_back(byte);
    }
    return bytes;
}

uint64_t CETCP::parseAddress(const std::string& addrStr) {
    std::string s = addrStr;
    // trim whitespace
    auto trim = [](std::string& x){ size_t a = x.find_first_not_of(" \t\r\n"); if (a==std::string::npos){ x.clear(); return; } size_t b = x.find_last_not_of(" \t\r\n"); x = x.substr(a, b-a+1); };
    trim(s);
    if (s.empty()) throw std::invalid_argument("empty address");
    bool hex = false;
    if (s.size()>=2 && s[0]=='0' && (s[1]=='x' || s[1]=='X')) { s = s.substr(2); hex = true; }
    if (!hex) {
        // if contains non-digits, treat as hex
        if (s.find_first_not_of("0123456789") != std::string::npos) hex = true;
    }
    return std::stoull(s, nullptr, hex ? 16 : 10);
}

int64_t CETCP::parseOffset(const std::string& offsetStr) {
    std::string s = offsetStr;
    // trim whitespace
    auto trim = [](std::string& x){ size_t a = x.find_first_not_of(" \t\r\n"); if (a==std::string::npos){ x.clear(); return; } size_t b = x.find_last_not_of(" \t\r\n"); x = x.substr(a, b-a+1); };
    trim(s);
    bool negative = false;
    if (!s.empty() && s[0]=='-'){ negative = true; s = s.substr(1); }
    bool hex = false;
    if (s.size()>=2 && s[0]=='0' && (s[1]=='x' || s[1]=='X')) { s = s.substr(2); hex = true; }
    if (!hex) {
        if (s.find_first_not_of("0123456789") != std::string::npos) hex = true;
    }
    int64_t value = std::stoll(s, nullptr, hex ? 16 : 10);
    return negative ? -value : value;
}

// ============ 内部辅助函数 ============

void CETCP::setLastError(const std::string& error) {
    lastError = error;
}

bool CETCP::sendFrame(uint8_t type, const std::vector<uint8_t>& payload) {
    if (payload.size() > CEQP::MAX_PAYLOAD_SIZE) {
        setLastError("Payload too large");
        return false;
    }

    // 构建帧头
    CEQP_FrameHeader header;
    header.magic[0] = 'C';
    header.magic[1] = 'E';
    header.magic[2] = 'Q';
    header.magic[3] = 'P';
    header.version = CEQP::VERSION;
    header.type = type;
    header.flags = 0;
    header.reserved = 0;
    header.request_id = nextRequestId++;
    header.payload_len = (uint32_t)payload.size();

    // 发送帧头
    if (send(sock, (const char*)&header, sizeof(header), 0) != sizeof(header)) {
        setLastError("Failed to send frame header");
        disconnect();
        return false;
    }

    // 发送负载
    if (!payload.empty()) {
        if (send(sock, (const char*)payload.data(), (int)payload.size(), 0) != (int)payload.size()) {
            setLastError("Failed to send payload");
            disconnect();
            return false;
        }
    }

    return true;
}

bool CETCP::receiveFrame(CEQP_FrameHeader& header, std::vector<uint8_t>& payload) {
    // 接收帧头
    int received = recv(sock, (char*)&header, sizeof(header), MSG_WAITALL);
    if (received != sizeof(header)) {
        setLastError("Failed to receive frame header");
        disconnect();
        return false;
    }

    // 验证魔数
    if (header.magic[0] != 'C' || header.magic[1] != 'E' ||
        header.magic[2] != 'Q' || header.magic[3] != 'P') {
        setLastError("Invalid frame magic");
        disconnect();
        return false;
    }

    // 验证协议版本
    if (header.version != CEQP::VERSION) {
        setLastError("Unsupported protocol version");
        return false;
    }

    // 接收负载
    payload.clear();
    if (header.payload_len > 0) {
        if (header.payload_len > CEQP::MAX_PAYLOAD_SIZE) {
            setLastError("Payload too large");
            disconnect();
            return false;
        }

        payload.resize(header.payload_len);
        received = recv(sock, (char*)payload.data(), header.payload_len, MSG_WAITALL);
        if (received != (int)header.payload_len) {
            setLastError("Failed to receive payload");
            disconnect();
            return false;
        }
    }

    return true;
}

// ============ TLV编码函数 ============

void CETCP::encodeTLV(std::vector<uint8_t>& buffer, uint16_t type, const void* data, uint16_t length) {
    size_t oldSize = buffer.size();
    buffer.resize(oldSize + 4 + length);
    writeLE16(&buffer[oldSize], type);
    writeLE16(&buffer[oldSize + 2], length);
    if (length > 0 && data) {
        memcpy(&buffer[oldSize + 4], data, length);
    }
}

void CETCP::encodeU64(std::vector<uint8_t>& buffer, uint16_t type, uint64_t value) {
    uint8_t data[8];
    writeLE64(data, value);
    encodeTLV(buffer, type, data, 8);
}

void CETCP::encodeU32(std::vector<uint8_t>& buffer, uint16_t type, uint32_t value) {
    uint8_t data[4];
    writeLE32(data, value);
    encodeTLV(buffer, type, data, 4);
}

void CETCP::encodeI64(std::vector<uint8_t>& buffer, uint16_t type, int64_t value) {
    uint8_t data[8];
    writeLE64(data, (uint64_t)value);
    encodeTLV(buffer, type, data, 8);
}

void CETCP::encodeString(std::vector<uint8_t>& buffer, uint16_t type, const std::string& str) {
    encodeTLV(buffer, type, str.data(), (uint16_t)str.length());
}

void CETCP::encodeBytes(std::vector<uint8_t>& buffer, uint16_t type, const std::vector<uint8_t>& data) {
    encodeTLV(buffer, type, data.data(), (uint16_t)data.size());
}

void CETCP::encodeI64Array(std::vector<uint8_t>& buffer, uint16_t type, const std::vector<int64_t>& offsets) {
    std::vector<uint8_t> data(offsets.size() * 8);
    for (size_t i = 0; i < offsets.size(); i++) {
        writeLE64(&data[i * 8], (uint64_t)offsets[i]);
    }
    encodeTLV(buffer, type, data.data(), (uint16_t)data.size());
}

// ============ TLV解码函数 ============

bool CETCP::extractTLV(const std::vector<uint8_t>& payload, uint16_t type, std::vector<uint8_t>& value) {
    size_t pos = 0;
    while (pos + 4 <= payload.size()) {
        uint16_t tlvType = readLE16(&payload[pos]);
        uint16_t tlvLen = readLE16(&payload[pos + 2]);

        if (pos + 4 + tlvLen > payload.size()) {
            break;
        }

        if (tlvType == type) {
            value.assign(payload.begin() + pos + 4, payload.begin() + pos + 4 + tlvLen);
            return true;
        }

        pos += 4 + tlvLen;
    }
    return false;
}

bool CETCP::extractU64(const std::vector<uint8_t>& payload, uint16_t type, uint64_t& value) {
    std::vector<uint8_t> data;
    if (extractTLV(payload, type, data) && data.size() == 8) {
        value = readLE64(data.data());
        return true;
    }
    return false;
}

bool CETCP::extractU32(const std::vector<uint8_t>& payload, uint16_t type, uint32_t& value) {
    std::vector<uint8_t> data;
    if (extractTLV(payload, type, data) && data.size() == 4) {
        value = readLE32(data.data());
        return true;
    }
    return false;
}

bool CETCP::extractString(const std::vector<uint8_t>& payload, uint16_t type, std::string& value) {
    std::vector<uint8_t> data;
    if (extractTLV(payload, type, data)) {
        value.assign(data.begin(), data.end());
        return true;
    }
    return false;
}

bool CETCP::extractBytes(const std::vector<uint8_t>& payload, uint16_t type, std::vector<uint8_t>& value) {
    return extractTLV(payload, type, value);
}

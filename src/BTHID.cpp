//
// Created by awalol on 2026/3/30.
//

#include "BTHID.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <ostream>
#include <queue>
#include <unistd.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>

#include "Utils.h"

#define BUFFER_LENGTH 0xFF

uint32_t crc32(const uint8_t* data, std::size_t size) {
    uint32_t crc = ~0xEADA2D49;  // 0xA2 seed

    while (size--) {
        crc ^= *data++;
        for (unsigned i = 0; i < 8; i++)
            crc = ((crc >> 1) ^ (0xEDB88320 & -(crc & 1)));
    }

    return ~crc;
}

inline void fill_output_report_checksum(uint8_t* outputData,size_t len)
{
    uint32_t crc = crc32(outputData, len - 4);
    outputData[len - 4] = (crc >> 0) & 0xFF;
    outputData[len - 3] = (crc >> 8) & 0xFF;
    outputData[len - 2] = (crc >> 16) & 0xFF;
    outputData[len - 1] = (crc >> 24) & 0xFF;
}

int BTHID::init() {
    fd = open("/dev/hidraw0", O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        fd = -1;
        std::cerr << "Failed to open BTHID device: " << strerror(errno) << std::endl;
        return -1;
    }
    std::cout << "BTHID device opened" << std::endl;

    return 0;
}

// Auto fill crc32 at last 4 bytes
ssize_t BTHID::send(uint8_t* data, size_t size) const {
    if (fd < 0) {
        return 0;
    }
    fill_output_report_checksum(data, size);
    const ssize_t ret = write(fd, data, size);
    if (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        std::cerr << "BT send failed: " << strerror(errno) << std::endl;
    }
    return ret;
}

std::vector<std::uint8_t> BTHID::recv() const {
    std::vector<std::uint8_t> data(128);
    const long ret = read(fd,data.data(), data.size());
    if (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        std::cerr << "BT recv failed: " << strerror(errno) << std::endl;
    }
    data.resize(std::ranges::max(0, (int)ret));
    return data;
}

void BTHID::setStateData(uint8_t* data, size_t size) {
    uint8_t outputData[78] = {};
    outputData[0] = 0x31;
    outputData[1] = reportSeqCounter << 4;
    if (++reportSeqCounter == 256) {
        reportSeqCounter = 0;
    }
    outputData[2] = 0x10;
    memcpy(outputData + 3, data, size);
    send(outputData, sizeof(outputData));
}

ssize_t BTHID::sendHaptics(const uint8_t* data) {
    uint8_t pkt[206] = {};
    pkt[0] = 0x33;
    pkt[1] = reportSeqCounter << 4;
    reportSeqCounter = (reportSeqCounter + 1) % 256;
    pkt[2] = 0x11 | (1 << 7);
    pkt[3] = 7;
    pkt[4] = 0b11111110;
    pkt[5] = BUFFER_LENGTH;
    pkt[6] = BUFFER_LENGTH;
    pkt[7] = BUFFER_LENGTH;
    pkt[8] = BUFFER_LENGTH;
    pkt[9] = BUFFER_LENGTH; // buffer length
    pkt[10] = packetCounter++;
    pkt[11] = 0x12 | (1 << 7);
    pkt[12] = 64;
    memcpy(pkt + 13, data, 64);
    return send(pkt, sizeof(pkt));
}

ssize_t BTHID::sendSpeaker(const uint8_t* data) {
    static uint8_t pkt[270] = {};
    pkt[0] = 0x34;
    pkt[1] = reportSeqCounter << 4;
    reportSeqCounter = (reportSeqCounter + 1) & 0x0F;
    pkt[2] = 0x11 | 0 << 6 | 1 << 7;
    pkt[3] = 7;
    pkt[4] = 0b11111110;
    pkt[5] = BUFFER_LENGTH;
    pkt[6] = BUFFER_LENGTH;
    pkt[7] = BUFFER_LENGTH;
    pkt[8] = BUFFER_LENGTH;
    pkt[9] = BUFFER_LENGTH; // buffer length
    pkt[10] = packetCounter++;
    pkt[11] = 0x16 | 0 << 6 | 1 << 7; // Speaker: 0x13 Headset: 0x16
    pkt[12] = 200;
    memcpy(pkt + 13, data, 200);

    return send(pkt, sizeof(pkt));
}

ssize_t BTHID::sendCombine(const uint8_t* haptics,const uint8_t* speaker) {
    static uint8_t pkt[334] = {};
    pkt[0] = 0x35;
    pkt[1] = reportSeqCounter << 4;
    reportSeqCounter = (reportSeqCounter + 1) & 0x0F;
    pkt[2] = 0x11 | 0 << 6 | 1 << 7;
    pkt[3] = 7;
    pkt[4] = 0b11111110;
    pkt[5] = BUFFER_LENGTH;
    pkt[6] = BUFFER_LENGTH;
    pkt[7] = BUFFER_LENGTH;
    pkt[8] = BUFFER_LENGTH;
    pkt[9] = BUFFER_LENGTH; // buffer length
    pkt[10] = packetCounter++;
    pkt[11] = 0x13 | 0 << 6 | 1 << 7; // 0x96 Speaker: 0x13 Headset: 0x16
    pkt[12] = 200;
    memcpy(pkt + 13, speaker, 200);
    pkt[213] = 0x12 | 0 << 6 | 1 << 7; // 0x91
    pkt[214] = 64;
    memcpy(pkt + 215, haptics, 64);
    // std::cout << "sendCombine" << std::endl;
    // Utils::print_hex(pkt, sizeof(pkt));
    return send(pkt, sizeof(pkt));
}

ssize_t BTHID::send_feature_report(const uint8_t *data, const size_t size) const {
    const auto res = ioctl(fd, HIDIOCSFEATURE(size), data);
    if (res < 0) {
        perror("send_feature_report");
    }
    return res;
}

std::vector<uint8_t> BTHID::get_feature_report(uint8_t reportId, size_t maxLength) const {
    std::vector<uint8_t> buf(maxLength);
    buf[0] = reportId;
    const auto res = ioctl(fd, HIDIOCGFEATURE(maxLength), buf.data());
    if (res < 0) {
        perror("get_feature_report");
    }
    return buf;
}

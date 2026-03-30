//
// Created by awalol on 2026/3/30.
//

#include "USBHID.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <ostream>
#include <unistd.h>

int USBHID::init() {
    fd = open("/dev/hidg0",O_RDWR | O_NONBLOCK);
    if(fd < 0) {
        std::cerr << "Failed to open Gadget HID device: " << strerror(errno) << std::endl;
        fd = -1;
        return -1;
    }
    std::cout << "Gadget HID device opened" << std::endl;
    return 0;
}

ssize_t USBHID::send(uint8_t* data, size_t size) const {
    if (fd < 0) {
        return 0;
    }
    const ssize_t ret = write(fd, data, size);
    if (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        std::cerr << "USB send failed: " << strerror(errno) << std::endl;
    }
    return ret;
}

std::vector<std::uint8_t> USBHID::recv() const {
    std::vector<std::uint8_t> data(128);
    const long ret = read(fd,data.data(), data.size());
    if (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        std::cerr << "USB recv failed: " << strerror(errno) << std::endl;
    }
    data.resize(std::ranges::max(0, (int)ret));
    return data;
}
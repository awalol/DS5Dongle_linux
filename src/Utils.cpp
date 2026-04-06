//
// Created by awalol on 2026/4/5.
//

#include "Utils.h"

void Utils::print_hex(const std::vector<uint8_t> data) {
    for (auto& byte : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
    }
    std::cout << std::endl;
}

void Utils::print_hex(const uint8_t* data,size_t size) {
    for (int i = 0; i < size; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::endl;
}
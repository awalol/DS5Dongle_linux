//
// Created by awalol on 2026/3/29.
//

#include "USBGadget.h"
#include "BTHID.h"
#include "USBHID.h"
#include "ALSARecord.h"

#include <cstring>
#include <chrono>
#include <iostream>
#include <thread>
#include <sys/epoll.h>

#include "Utils.h"

USBGadget gadget;
BTHID bt;
USBHID usb;
ALSARecord recorder(bt);

uint8_t interrupt_data[64] = {
    0x01, 0x7f, 0x7d, 0x7f, 0x7e, 0x00, 0x00, 0xa7,
    0x08, 0x00, 0x00, 0x00, 0x52, 0x43, 0x30, 0x41,
    0x01, 0x00, 0x0e, 0x00, 0xef, 0xff, 0x03, 0x03,
    0x7b, 0x1b, 0x18, 0xf0, 0xcc, 0x9c, 0x60, 0x00,
    0xfc, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x09, 0x09, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xa7, 0xad, 0x60, 0x00, 0x29, 0x18, 0x00,
    0x53, 0x9f, 0x28, 0x35, 0xa5, 0xa8, 0x0c, 0x8b
};

void audio_task(const std::stop_token &stop_token) {
    while (!stop_token.stop_requested()) {
        recorder.audio_loop();
    }
}

void event_bus(const std::stop_token& stop_token) {
    int epoll_fd = epoll_create1(0);
    epoll_event events[3];

    epoll_event usb_event{};
    usb_event.events = EPOLLIN;
    usb_event.data.fd = usb.get_fd();
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, usb.get_fd(), &usb_event);

    epoll_event bt_event{};
    bt_event.events = EPOLLIN;
    bt_event.data.fd = bt.get_fd();
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, bt.get_fd(), &bt_event);

    constexpr auto kUsbSendPeriod = std::chrono::milliseconds(4);
    auto nextSendTime = std::chrono::steady_clock::now() + kUsbSendPeriod;

    while (!stop_token.stop_requested()) {
        auto now = std::chrono::steady_clock::now();
        if (now >= nextSendTime) {
            usb.send(interrupt_data, 64);
            nextSendTime += kUsbSendPeriod;
        }

        int timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(nextSendTime - now).count();
        if (timeout_ms < 0) {
            timeout_ms = 0;
        }

        int num_events = epoll_wait(epoll_fd, events, 3, timeout_ms);

        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == usb.get_fd()) {
                // USB SetReport / Interrupt OUT
                std::vector<std::uint8_t> data = usb.recv();
                if (data.empty()) {
                    continue;
                }
                if (data[0] == 0x02) {
                    bt.setStateData(data.data() + 1,63);
                    continue;
                }
                if (data[0] == 0x80) {
                    data.resize(64);
                    bt.send_feature_report(data.data(),64);
                    if (auto ret = bt.get_feature_report(0x81,64); !ret.empty()) {
                        auto rs = usb.set_get_report(0x81,ret);
                    }
                }
            }else if (events[i].data.fd == bt.get_fd()) {
                // 接收蓝牙的状态数据
                std::vector<std::uint8_t> data = bt.recv();
                if (data.empty()) {
                    continue;
                }
                memcpy(interrupt_data + 1,data.data() + 2,63);
            }
        }
    }
}

int main() {
    if (!gadget.exists()) {
        gadget.destroy();
        gadget.create();
    }

    if (usb.init() != 0) {
        return -1;
    }

    if (bt.init() != 0) {
        return -1;
    }

    if (recorder.init() != 0) {
        return -1;
    }

    // Init DualSense

    uint8_t report32[142] = {};
    report32[0] = 0x32;
    report32[1] = 0x10; // reportSeqCounter
    uint8_t packet_0x10[] =
    {
        0x10 | 0 << 6 | 1 << 7, // Packet: 0x10
        63, // DS:47 DSE:63
        // SetStateData
        0xfd, 0xf7, 0x0, 0x0,
        0x7f, 0x7f, // Headphones, Speaker
        0xff, 0x9, 0x0, 0xf, 0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xa,
        0x7, 0x0, 0x0, 0x2, 0x1,
        0x00,
        0xff, 0xd7, 0x00 // RGB LED: R, G, B (Nijika Color!)✨
    };
    memcpy(report32 + 2, packet_0x10, sizeof(packet_0x10));
    auto ret = bt.send(report32, sizeof(report32));

    std::cout << "Get Controller and Host MAC" << std::endl;
    auto report_0x09 = bt.get_feature_report(0x09,20);
    ret = usb.set_get_report(0x09,report_0x09);
    Utils::print_hex(report_0x09);

    std::cout << "Get Controller Version/Data (Firmware Info)" << std::endl;
    auto report_0x20 = bt.get_feature_report(0x20,64);
    ret = usb.set_get_report(0x20,report_0x20);
    Utils::print_hex(report_0x20);

    std::cout << "Get Hardware Info" << std::endl;
    auto report_0x22 = bt.get_feature_report(0x22,64);
    ret = usb.set_get_report(0x22,report_0x22);
    Utils::print_hex(report_0x22);

    std::cout << "Get Calibration" << std::endl;
    auto report_0x05 = bt.get_feature_report(0x05,41);
    ret = usb.set_get_report(0x05,report_0x05);
    Utils::print_hex(report_0x05);

    auto thread = std::jthread(event_bus);
    auto thread2 = std::jthread(audio_task);

    while (true) {
        std::cout << "press any key to exit" << std::endl;
        std::cin.get();
        thread.request_stop();
        thread.join();
        thread2.request_stop();
        thread2.join();
        // gadget.destroy();
        break;
    }

    return 0;
}

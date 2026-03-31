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

    if (bt.init() != 0) {
        return -1;
    }
    if (usb.init() != 0) {
        return -1;
    }

    if (recorder.init() != 0) {
        return -1;
    }

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

//
// Created by awalol on 2026/3/30.
//

#ifndef DS5_DONGLE_LINUX_BTHID_H
#define DS5_DONGLE_LINUX_BTHID_H
#include <cstdint>
#include <vector>
#include <sys/types.h>


namespace std {
    class stop_token;
}

class BTHID {
private:
    int fd = -1;
    int reportSeqCounter = 0;
    uint8_t packetCounter = 0;
public:
    int init();
    int get_fd() const { return fd; }
    ssize_t send(uint8_t* data, size_t size) const;
    std::vector<std::uint8_t> recv() const;
    void setStateData(uint8_t* data, size_t size);
    void queue_task(std::stop_token);
    ssize_t sendHaptics(const uint8_t* data);
    ssize_t sendSpeaker(const uint8_t* data);
    ssize_t sendCombine(const uint8_t* haptics,const uint8_t* speaker);
};



#endif //DS5_DONGLE_LINUX_BTHID_H

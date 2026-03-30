//
// Created by awalol on 2026/3/30.
//

#ifndef DS5_DONGLE_LINUX_ALSARECORD_H
#define DS5_DONGLE_LINUX_ALSARECORD_H
#include <vector>
#include <alsa/asoundlib.h>


class ALSARecord {
private:
    bool opened = false;
    snd_pcm_t *handle;
public:
    int init();
    // return: read frames
    snd_pcm_uframes_t read(std::vector<uint16_t> buffer, snd_pcm_uframes_t frames) const;
};



#endif //DS5_DONGLE_LINUX_ALSARECORD_H

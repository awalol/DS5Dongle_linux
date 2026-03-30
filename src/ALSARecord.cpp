//
// Created by awalol on 2026/3/30.
//

#include "ALSARecord.h"

#include <iostream>
#include <vector>
#include <alsa/error.h>

#include "USBGadget.h"

int ALSARecord::init() {
    int ret = snd_pcm_open(&handle, "hw:0,0", SND_PCM_STREAM_CAPTURE, 0);
    if (ret < 0) {
        std::cerr << "Failed to open PCM device: " << snd_strerror(ret) << std::endl;
        return ret;
    }
    ret = snd_pcm_set_params(
        handle,
        SND_PCM_FORMAT_S16_LE,
        SND_PCM_ACCESS_RW_INTERLEAVED,
        4,
        48000,
        1,
        500000
    );
    if (ret < 0) {
        std::cerr << "Failed to set PCM parameters: " << snd_strerror(ret) << std::endl;
        return ret;
    }

    ret = snd_pcm_prepare(handle);
    if (ret < 0) {
        std::cerr << "Failed to prepare PCM: " << snd_strerror(ret) << std::endl;
        return ret;
    }
    ret = snd_pcm_start(handle);
    if (ret < 0) {
        std::cerr << "Failed to start PCM: " << snd_strerror(ret) << std::endl;
        return ret;
    }

    opened = true;
    return 0;
}

snd_pcm_uframes_t ALSARecord::read(std::vector<uint16_t> buffer, snd_pcm_uframes_t frames) const {
    if (!opened) {
        return 0;
    }
    // 1 frames = 4ch = 4 * int16
    snd_pcm_uframes_t ret = snd_pcm_readi(handle, buffer.data(), frames);
    return ret;
}

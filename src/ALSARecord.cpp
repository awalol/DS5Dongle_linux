//
// Created by awalol on 2026/3/30.
//

#include "ALSARecord.h"

#include <chrono>
#include <iostream>
#include <alsa/error.h>
#include "resample.h"

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
        10 * 1000 // us
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

    // resampler.SetMode(true,0,false);
    resampler.SetMode(false, 32, true, 32, 16);
    resampler.SetRates(48000,3000);
    resampler.SetFeedMode(true);
    resampler.Prealloc(2,512,32);

    opened = true;
    return 0;
}

ssize_t ALSARecord::read(int16_t* buffer, snd_pcm_uframes_t frames) const {
    if (!opened) {
        return 0;
    }
    // 1 frames = 4ch = 4 * int16
    ssize_t ret = snd_pcm_readi(handle, buffer, frames);
    return ret;
}

void ALSARecord::audio_loop() {
    static int16_t buffer[32 * 4] = {};
    auto frames = read(buffer,32);
    if (frames > 0) {
        haptics_proc(buffer,frames);
    }
}

void ALSARecord::haptics_proc(int16_t* data,ssize_t frames) {
    WDL_ResampleSample *in_buf;
    int nframes = resampler.ResamplePrepare(frames,2,&in_buf);

    for (int i = 0; i < nframes; i++) {
        in_buf[i * 2] = (WDL_ResampleSample) (data[i * 4] / 32768.0f);
        in_buf[i * 2 + 1] = (WDL_ResampleSample) (data[i * 4 + 1] / 32768.0f);
    }

    WDL_ResampleSample out_buf[64];
    int out_frames = resampler.ResampleOut(out_buf,nframes,32,2);
    static int8_t haptics_buf[64];
    static int haptics_buf_pos = 0;

    for (int i = 0;i < out_frames;i++) {
        int val_l = (int) (out_buf[i * 2] * 127.0f * 2.0f);
        int val_r = (int) (out_buf[i * 2 + 1] * 127.0f * 2.0f);
        haptics_buf[haptics_buf_pos++] = (int8_t) std::clamp(val_l, -128, 127);
        haptics_buf[haptics_buf_pos++] = (int8_t) std::clamp(val_r, -128, 127);

        if (haptics_buf_pos != 64) {
            continue;
        }
        bt.sendHaptics(haptics_buf);
        haptics_buf_pos = 0;
    }
}

//
// Created by awalol on 2026/3/30.
//

#ifndef DS5_DONGLE_LINUX_ALSARECORD_H
#define DS5_DONGLE_LINUX_ALSARECORD_H
#include <alsa/asoundlib.h>

#include "BTHID.h"
#include "resample.h"

class ALSARecord {
private:
    bool opened = false;
    snd_pcm_t *handle;
    WDL_Resampler resampler;
    BTHID& bt;
    void haptics_proc(int16_t* data,ssize_t frames);
public:
    int init();
    // return: read frames
    ssize_t read(int16_t* buffer, snd_pcm_uframes_t frames) const;
    void audio_loop();
    ALSARecord(BTHID& bt) : bt(bt) {}
};



#endif //DS5_DONGLE_LINUX_ALSARECORD_H

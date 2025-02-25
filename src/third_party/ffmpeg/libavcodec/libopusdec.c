/*
 * Opus decoder using libopus
 * Copyright (c) 2012 Nicolas George
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <opus.h>
#include <opus_multistream.h>

#include "libavutil/internal.h"
#include "libavutil/intreadwrite.h"

#include "avcodec.h"
#include "internal.h"
#include "vorbis.h"
#include "mathops.h"
#include "libopus.h"

struct libopus_context {
    OpusMSDecoder *dec;
    int pre_skip;
#ifndef OPUS_SET_GAIN
    union { int i; double d; } gain;
#endif
};

#define OPUS_HEAD_SIZE 19

static av_cold int libopus_decode_init(AVCodecContext *avc)
{
    struct libopus_context *opus = avc->priv_data;
    int ret, channel_map = 0, gain_db = 0, nb_streams, nb_coupled;
    uint8_t mapping_arr[8] = { 0, 1 }, *mapping;

    avc->channels = avc->extradata_size >= 10 ? avc->extradata[9] : (avc->channels == 1) ? 1 : 2;
    if (avc->channels <= 0) {
        av_log(avc, AV_LOG_WARNING,
               "Invalid number of channels %d, defaulting to stereo\n", avc->channels);
        avc->channels = 2;
    }

    avc->sample_rate    = 48000;
    avc->sample_fmt     = avc->request_sample_fmt == AV_SAMPLE_FMT_FLT ?
                          AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_S16;
    if (avc->channels <= 0) {
        av_log(avc, AV_LOG_ERROR, "Invalid number of channels\n");
        return AVERROR(EINVAL);
    }

    if (avc->extradata_size >= OPUS_HEAD_SIZE) {
        opus->pre_skip = AV_RL16(avc->extradata + 10);
        gain_db     = sign_extend(AV_RL16(avc->extradata + 16), 16);
        channel_map = AV_RL8 (avc->extradata + 18);
    }
    if (avc->extradata_size >= OPUS_HEAD_SIZE + 2 + avc->channels) {
        nb_streams = avc->extradata[OPUS_HEAD_SIZE + 0];
        nb_coupled = avc->extradata[OPUS_HEAD_SIZE + 1];
        if (nb_streams + nb_coupled != avc->channels)
            av_log(avc, AV_LOG_WARNING, "Inconsistent channel mapping.\n");
        mapping = avc->extradata + OPUS_HEAD_SIZE + 2;
    } else {
        if (avc->channels > 2 || channel_map) {
            av_log(avc, AV_LOG_ERROR,
                   "No channel mapping for %d channels.\n", avc->channels);
            return AVERROR(EINVAL);
        }
        nb_streams = 1;
        nb_coupled = avc->channels > 1;
        mapping    = mapping_arr;
    }

    if (channel_map == 1) {
        avc->channel_layout = avc->channels > 8 ? 0 :
                              ff_vorbis_channel_layouts[avc->channels - 1];
        if (avc->channels > 2 && avc->channels <= 8) {
            const uint8_t *vorbis_offset = ff_vorbis_channel_layout_offsets[avc->channels - 1];
            int ch;

            /* Remap channels from Vorbis order to ffmpeg order */
            for (ch = 0; ch < avc->channels; ch++)
                mapping_arr[ch] = mapping[vorbis_offset[ch]];
            mapping = mapping_arr;
        }
    } else if (channel_map == 2) {
        int ambisonic_channels;
        if (avc->channels > 227) {
            av_log(avc, AV_LOG_ERROR, "Too many channels\n");
            return AVERROR_INVALIDDATA;
        }

        ambisonic_channels = ff_sqrt(avc->channels);
        ambisonic_channels *= ambisonic_channels;
        if (avc->channels != ambisonic_channels &&
            avc->channels != ambisonic_channels + 2) {
            av_log(avc, AV_LOG_ERROR,
                   "Channel mapping 2 is only specified for channel counts"
                   " which can be written as n or n + 2 for nonnegative integer n,"
                   " where n is the number of ambisonic channels.\n");
            return AVERROR_INVALIDDATA;
        }
        avc->channel_layout = 0;
    } else {
        avc->channel_layout = 0;
    }

    opus->dec = opus_multistream_decoder_create(avc->sample_rate, avc->channels,
                                                nb_streams, nb_coupled,
                                                mapping, &ret);
    if (!opus->dec) {
        av_log(avc, AV_LOG_ERROR, "Unable to create decoder: %s\n",
               opus_strerror(ret));
        return ff_opus_error_to_averror(ret);
    }

#ifdef OPUS_SET_GAIN
    ret = opus_multistream_decoder_ctl(opus->dec, OPUS_SET_GAIN(gain_db));
    if (ret != OPUS_OK)
        av_log(avc, AV_LOG_WARNING, "Failed to set gain: %s\n",
               opus_strerror(ret));
#else
    {
        double gain_lin = ff_exp10(gain_db / (20.0 * 256));
        if (avc->sample_fmt == AV_SAMPLE_FMT_FLT)
            opus->gain.d = gain_lin;
        else
            opus->gain.i = FFMIN(gain_lin * 65536, INT_MAX);
    }
#endif

    /* Decoder delay (in samples) at 48kHz */
    avc->delay = avc->internal->skip_samples = opus->pre_skip;

    return 0;
}

static av_cold int libopus_decode_close(AVCodecContext *avc)
{
    struct libopus_context *opus = avc->priv_data;

    opus_multistream_decoder_destroy(opus->dec);
    return 0;
}

#define MAX_FRAME_SIZE (960 * 6)

static int libopus_decode(AVCodecContext *avc, void *data,
                          int *got_frame_ptr, AVPacket *pkt)
{
    struct libopus_context *opus = avc->priv_data;
    AVFrame *frame               = data;
    int ret, nb_samples;

    frame->nb_samples = MAX_FRAME_SIZE;
    if ((ret = ff_get_buffer(avc, frame, 0)) < 0)
        return ret;

    if (avc->sample_fmt == AV_SAMPLE_FMT_S16)
        nb_samples = opus_multistream_decode(opus->dec, pkt->data, pkt->size,
                                             (opus_int16 *)frame->data[0],
                                             frame->nb_samples, 0);
    else
        nb_samples = opus_multistream_decode_float(opus->dec, pkt->data, pkt->size,
                                                   (float *)frame->data[0],
                                                   frame->nb_samples, 0);

    if (nb_samples < 0) {
        av_log(avc, AV_LOG_ERROR, "Decoding error: %s\n",
               opus_strerror(nb_samples));
        return ff_opus_error_to_averror(nb_samples);
    }

#ifndef OPUS_SET_GAIN
    {
        int i = avc->channels * nb_samples;
        if (avc->sample_fmt == AV_SAMPLE_FMT_FLT) {
            float *pcm = (float *)frame->data[0];
            for (; i > 0; i--, pcm++)
                *pcm = av_clipf(*pcm * opus->gain.d, -1, 1);
        } else {
            int16_t *pcm = (int16_t *)frame->data[0];
            for (; i > 0; i--, pcm++)
                *pcm = av_clip_int16(((int64_t)opus->gain.i * *pcm) >> 16);
        }
    }
#endif

    frame->nb_samples = nb_samples;
    *got_frame_ptr    = 1;

    return pkt->size;
}

static void libopus_flush(AVCodecContext *avc)
{
    struct libopus_context *opus = avc->priv_data;

    opus_multistream_decoder_ctl(opus->dec, OPUS_RESET_STATE);
    /* The stream can have been extracted by a tool that is not Opus-aware.
       Therefore, any packet can become the first of the stream. */
    avc->internal->skip_samples = opus->pre_skip;
}

AVCodec ff_libopus_decoder = {
    .name           = "libopus",
    .long_name      = NULL_IF_CONFIG_SMALL("libopus Opus"),
    .type           = AVMEDIA_TYPE_AUDIO,
    .id             = AV_CODEC_ID_OPUS,
    .priv_data_size = sizeof(struct libopus_context),
    .init           = libopus_decode_init,
    .close          = libopus_decode_close,
    .decode         = libopus_decode,
    .flush          = libopus_flush,
    .capabilities   = AV_CODEC_CAP_DR1,
    .sample_fmts    = (const enum AVSampleFormat[]){ AV_SAMPLE_FMT_FLT,
                                                     AV_SAMPLE_FMT_S16,
                                                     AV_SAMPLE_FMT_NONE },
};

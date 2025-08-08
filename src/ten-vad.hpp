#include "ten_vad.h"

// miniaudio format f32 [-1,1] s16 [-32768, 32767]
static int16_t pcmf32tos16(float_t f)
{
	f = f * 32768 ;
	if( f > 32767 ) f = 32767;
	if( f < -32768 ) f = -32768;

	return (int16_t) f;
}

static const int16_t* pcmf32tos16(const float_t * samples, int total_samples)
{
	int16_t* tsamples = (int16_t*)malloc(total_samples*sizeof(int16_t));

	for (int idx=0;idx<total_samples;idx++) tsamples[idx] = pcmf32tos16(samples[idx]);

	return tsamples;
}

struct whisper_vad_segments * whisper_ten_vad_segments_from_samples(
    whisper_vad_params params,
    const float_t* fsamples,
    int total_samples)
{
    const int hop_size = 256; /// @todo
    const float voice_threshold = params.threshold;
    /** @todo
    int     min_speech_duration_ms  = params.min_speech_duration_ms;
    int     min_silence_duration_ms = params.min_silence_duration_ms;
    float   max_speech_duration_s   = params.max_speech_duration_s;
    int     speech_pad_ms           = params.speech_pad_ms;
    * */
    const int16_t* samples = pcmf32tos16(fsamples,total_samples);

    int frame_num = total_samples / hop_size;
    std::vector<float> out_probs(frame_num);
    std::vector<int32_t> out_flags(frame_num);

    void* ten_vad_handle = nullptr;
    ten_vad_create(&ten_vad_handle, hop_size, voice_threshold);

    for (int i = 0; i < frame_num; ++i) {
        const int16_t* frame = samples + i * hop_size;
        ten_vad_process(ten_vad_handle, const_cast<int16_t*>(frame), hop_size,
                        &out_probs[i], &out_flags[i]);
    }

    ten_vad_destroy(&ten_vad_handle);
    free((int16_t*)samples);

    std::vector<whisper_vad_segment> segments;
    int seg_start = -1;

    for (int i = 0; i < frame_num; ++i) {
        bool is_speech = (out_flags[i] != 0);
        int frame_start = i * hop_size;
        int frame_end = frame_start + hop_size;

        if (is_speech) {
            if (seg_start < 0) {
                seg_start = frame_start;
            }
        } else {
            if (seg_start >= 0) {
                segments.push_back({ samples_to_cs(seg_start), samples_to_cs(frame_start) });
                seg_start = -1;
            }
        }
    }

    if (seg_start >= 0) {
        segments.push_back({ samples_to_cs(seg_start), samples_to_cs(total_samples) });
    }
    
    whisper_vad_segments * vad_segments = new whisper_vad_segments;
    if (vad_segments == NULL) {
        WHISPER_LOG_ERROR("%s: failed to allocate memory for whisper_vad_segments\n", __func__);
        return nullptr;
    }

    vad_segments->data = std::move(segments);

    return vad_segments;
}

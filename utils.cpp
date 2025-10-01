#include "plugin.h"
#include <cmath>
#include <cstring>
#include <algorithm>

// Basic utility functions
float db_to_linear(float db) {
    return powf(10.0f, db / 20.0f);
}

float linear_to_db(float linear) {
    return 20.0f * log10f(fmaxf(linear, 1e-10f));
}

// Pitch shifting utilities
float semitones_to_ratio(float semitones) {
    return powf(2.0f, semitones / 12.0f);
}

float ratio_to_semitones(float ratio) {
    return 12.0f * log2f(fmaxf(ratio, 1e-10f));
}

// Grain envelope generation
float hann_window(float phase) {
    if (phase < 0.0f || phase > 1.0f) return 0.0f;
    return 0.5f * (1.0f - cosf(2.0f * M_PI * phase));
}

float linear_fade(float phase, float fade_in_ratio, float fade_out_ratio) {
    if (phase < 0.0f || phase > 1.0f) return 0.0f;
    
    if (phase < fade_in_ratio) {
        return phase / fade_in_ratio;
    } else if (phase > (1.0f - fade_out_ratio)) {
        return (1.0f - phase) / fade_out_ratio;
    } else {
        return 1.0f;
    }
}

// Circular buffer utilities
void circular_buffer_write(float *buffer, int buffer_size, int *write_pos, float sample) {
    buffer[*write_pos] = sample;
    *write_pos = (*write_pos + 1) % buffer_size;
}

float circular_buffer_read(const float *buffer, int buffer_size, float read_pos) {
    // Handle fractional positions with linear interpolation
    // Ensure read_pos is positive and within reasonable range
    while (read_pos < 0.0f) read_pos += buffer_size;
    while (read_pos >= buffer_size) read_pos -= buffer_size;
    
    int pos1 = (int)read_pos;
    int pos2 = (pos1 + 1) % buffer_size;
    float frac = read_pos - floorf(read_pos);
    
    return buffer[pos1] * (1.0f - frac) + buffer[pos2] * frac;
}

float circular_buffer_read_relative(const float *buffer, int buffer_size, int write_pos, float samples_ago) {
    // Clamp samples_ago to buffer size to prevent reading invalid data
    if (samples_ago > buffer_size) {
        samples_ago = buffer_size - 1;
    }
    if (samples_ago < 0.0f) {
        samples_ago = 0.0f;
    }
    
    float read_pos = write_pos - samples_ago;
    while (read_pos < 0) read_pos += buffer_size;
    while (read_pos >= buffer_size) read_pos -= buffer_size;
    
    return circular_buffer_read(buffer, buffer_size, read_pos);
}

// Onset detection utilities
float calculate_spectral_flux(const float *current_frame, const float *previous_frame, int frame_size) {
    float flux = 0.0f;
    for (int i = 0; i < frame_size; ++i) {
        float diff = current_frame[i] - previous_frame[i];
        if (diff > 0.0f) {
            flux += diff;
        }
    }
    return flux;
}

float calculate_energy(const float *frame, int frame_size) {
    float energy = 0.0f;
    for (int i = 0; i < frame_size; ++i) {
        energy += frame[i] * frame[i];
    }
    return energy / frame_size;
}

bool detect_onset_simple(float current_energy, float previous_energy, float threshold, float ratio_threshold) {
    if (previous_energy < 1e-6f) return false;
    
    float energy_ratio = current_energy / previous_energy;
    return (current_energy > threshold) && (energy_ratio > ratio_threshold);
}

// Grain voice management
void init_grain_voice(grain_voice_t *voice) {
    voice->active = false;
    voice->buffer_start_pos = 0.0f;
    voice->playback_pos = 0.0f;
    voice->pitch_ratio = 1.0f;
    voice->grain_length_samples = 0;
    voice->current_sample = 0;
    voice->amplitude = 1.0f;
}

grain_voice_t* allocate_grain_voice(grain_voice_t *voices, int max_voices) {
    // Find first inactive voice
    for (int i = 0; i < max_voices; ++i) {
        if (!voices[i].active) {
            return &voices[i];
        }
    }
    
    // If no inactive voices, steal the oldest one
    grain_voice_t *oldest = &voices[0];
    for (int i = 1; i < max_voices; ++i) {
        if (voices[i].current_sample > oldest->current_sample) {
            oldest = &voices[i];
        }
    }
    
    return oldest;
}

void start_grain_voice(grain_voice_t *voice, float buffer_start_pos, float pitch_ratio, 
                      int grain_length_samples, float amplitude) {
    voice->active = true;
    voice->buffer_start_pos = buffer_start_pos;
    voice->playback_pos = 0.0f;  // Start playback from 0, will be added to buffer_start_pos
    voice->pitch_ratio = pitch_ratio;
    voice->grain_length_samples = grain_length_samples;
    voice->current_sample = 0;
    voice->amplitude = amplitude;
}

float process_grain_voice(grain_voice_t *voice, const float *buffer, int buffer_size, int write_pos) {
    if (!voice->active) return 0.0f;
    
    if (voice->current_sample >= voice->grain_length_samples) {
        voice->active = false;
        return 0.0f;
    }
    
    // Calculate how far back in the buffer to read
    float samples_ago = voice->buffer_start_pos + voice->playback_pos;
    
    // Clamp to buffer size to prevent reading invalid data
    if (samples_ago > buffer_size - 1) {
        samples_ago = buffer_size - 1;
    }
    
    // Read from circular buffer with pitch shifting
    float sample = circular_buffer_read_relative(buffer, buffer_size, write_pos, samples_ago);
    
    // Apply grain envelope
    float phase = (float)voice->current_sample / voice->grain_length_samples;
    float envelope = hann_window(phase);
    
    // Advance playback position
    voice->playback_pos += voice->pitch_ratio;
    voice->current_sample++;
    
    return sample * envelope * voice->amplitude;
}

// Check if an interval is consonant (avoids harsh dissonances)
bool is_consonant_interval(float semitones) {
    // Reduce to within one octave and get absolute value
    float abs_semitones = fabsf(semitones);
    while (abs_semitones >= 12.0f) abs_semitones -= 12.0f;
    
    // Consonant intervals (within an octave)
    return (abs_semitones < 0.5f) ||                    // unison
           (abs_semitones > 2.5f && abs_semitones < 3.5f) ||  // minor third
           (abs_semitones > 3.5f && abs_semitones < 4.5f) ||  // major third
           (abs_semitones > 4.5f && abs_semitones < 5.5f) ||  // perfect fourth
           (abs_semitones > 6.5f && abs_semitones < 7.5f) ||  // perfect fifth
           (abs_semitones > 8.5f && abs_semitones < 9.5f) ||  // major sixth
           (abs_semitones > 10.5f && abs_semitones < 11.5f) || // major seventh
           (abs_semitones > 11.5f);                        // octave
}

// Filter harmonic ratios to remove dissonant intervals relative to base pitch
void filter_consonant_ratios(float *ratios, int *count, float base_pitch_semitones) {
    int filtered_count = 0;
    float temp_ratios[8];
    
    for (int i = 0; i < *count && i < 8; ++i) {
        float harmonic_semitones = ratio_to_semitones(ratios[i]);
        float final_semitones = base_pitch_semitones + harmonic_semitones;
        
        if (is_consonant_interval(final_semitones)) {
            temp_ratios[filtered_count] = ratios[i];
            filtered_count++;
        }
    }
    
    // Copy back filtered results
    for (int i = 0; i < filtered_count; ++i) {
        ratios[i] = temp_ratios[i];
    }
    *count = filtered_count;
    
    // Ensure we always have at least the root
    if (*count == 0) {
        ratios[0] = 1.0f;
        *count = 1;
    }
}

// Musical harmony utilities
void get_harmonic_ratios(float *ratios, int count, harmonic_mode_t mode) {
    switch (mode) {
        case HARMONIC_MAJOR_TRIAD:
            if (count >= 3) {
                ratios[0] = 1.0f;                    // root
                ratios[1] = semitones_to_ratio(4.0f); // major third
                ratios[2] = semitones_to_ratio(7.0f); // perfect fifth
            }
            break;
            
        case HARMONIC_MINOR_TRIAD:
            if (count >= 3) {
                ratios[0] = 1.0f;                    // root
                ratios[1] = semitones_to_ratio(3.0f); // minor third
                ratios[2] = semitones_to_ratio(7.0f); // perfect fifth
            }
            break;
            
        case HARMONIC_PERFECT_FIFTHS:
            for (int i = 0; i < count; ++i) {
                ratios[i] = semitones_to_ratio(7.0f * i); // stacked fifths
            }
            break;
            
        case HARMONIC_OCTAVES:
            for (int i = 0; i < count; ++i) {
                ratios[i] = semitones_to_ratio(12.0f * i); // octaves
            }
            break;
            
        case HARMONIC_PENTATONIC:
            if (count >= 5) {
                ratios[0] = 1.0f;                     // root
                ratios[1] = semitones_to_ratio(2.0f);  // major second
                ratios[2] = semitones_to_ratio(4.0f);  // major third
                ratios[3] = semitones_to_ratio(7.0f);  // perfect fifth
                ratios[4] = semitones_to_ratio(9.0f);  // major sixth
            }
            break;
            
        default:
            // unison
            for (int i = 0; i < count; ++i) {
                ratios[i] = 1.0f;
            }
            break;
    }
}

// Timing utilities
bool should_trigger_grain(trigger_state_t *state, float current_time, float min_interval) {
    if (current_time - state->last_trigger_time >= min_interval) {
        state->last_trigger_time = current_time;
        return true;
    }
    return false;
}

float samples_to_ms(int samples, double sample_rate) {
    return (samples * 1000.0f) / sample_rate;
}

int ms_to_samples(float ms, double sample_rate) {
    return (int)((ms * sample_rate) / 1000.0f);
}

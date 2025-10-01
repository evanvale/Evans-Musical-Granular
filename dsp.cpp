#include "plugin.h"
#include <cmath>
#include <cstring>

// Cross-platform SIMD support for denormal protection
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
    #ifdef _MSC_VER
        #include <intrin.h>
    #else
        #include <immintrin.h>
    #endif
    #define HAS_SSE 1
#else
    #define HAS_SSE 0
#endif

static const double PI = 3.14159265358979323846;

// Enable flush-to-zero for denormal protection
static void enable_flush_to_zero() {
    #if HAS_SSE
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
    #endif
}

void update_parameter_smoothing(plugin_t *p) {
    // Enable denormal protection
    enable_flush_to_zero();
    
    // Calculate smoothing coefficient (5ms smoothing time)
    float smooth_time_samples = (5.0f / 1000.0f) * p->sample_rate;
    p->smooth_coeff = 1.0f / smooth_time_samples;
    
    // Initialize smoothed values
    p->gain_smooth.current = p->gain;
    p->gain_smooth.target = p->gain;
    p->gain_smooth.active = false;
    
    p->freq_smooth.current = p->freq;
    p->freq_smooth.target = p->freq;
    p->freq_smooth.active = false;
    
    p->dry_wet_smooth.current = p->dry_wet;
    p->dry_wet_smooth.target = p->dry_wet;
    p->dry_wet_smooth.active = false;
    
    p->any_smoothing_active = false;
}

void trigger_parameter_smoothing(plugin_t *p) {
    const float epsilon = 0.0001f;
    
    if (fabs(p->gain - p->gain_smooth.target) > epsilon) {
        p->gain_smooth.target = p->gain;
        p->gain_smooth.active = true;
        p->any_smoothing_active = true;
    }
    
    if (fabs(p->freq - p->freq_smooth.target) > epsilon) {
        p->freq_smooth.target = p->freq;
        p->freq_smooth.active = true;
        p->any_smoothing_active = true;
    }
    
    if (fabs(p->dry_wet - p->dry_wet_smooth.target) > epsilon) {
        p->dry_wet_smooth.target = p->dry_wet;
        p->dry_wet_smooth.active = true;
        p->any_smoothing_active = true;
    }
}

void process_parameter_smoothing(plugin_t *p, uint32_t frames) {
    if (!p->any_smoothing_active) return;
    
    const float smooth_threshold = 0.001f;
    bool still_active = false;
    
    auto smooth_param = [&](param_smooth_t &param) {
        if (!param.active) return;
        
        for (uint32_t i = 0; i < frames; i++) {
            param.current += (param.target - param.current) * p->smooth_coeff;
        }
        
        if (fabsf(param.current - param.target) < smooth_threshold) {
            param.current = param.target;
            param.active = false;
        } else {
            still_active = true;
        }
    };
    
    smooth_param(p->gain_smooth);
    smooth_param(p->freq_smooth);
    smooth_param(p->dry_wet_smooth);
    
    p->any_smoothing_active = still_active;
    
    // Check if filter coefficients need update
    if (fabs(p->freq_smooth.current - p->last_freq) > smooth_threshold) {
        p->coefficients_need_update = true;
    }
}

void reset_filter_states(plugin_t *p) {
    memset(&p->filter_L, 0, sizeof(biquad_t));
    memset(&p->filter_R, 0, sizeof(biquad_t));
}

float process_biquad(float input, biquad_t *filter) {
    float output = filter->b0 * input + filter->b1 * filter->x1 + filter->b2 * filter->x2
                  - filter->a1 * filter->y1 - filter->a2 * filter->y2;
    
    // Update delay lines
    filter->x2 = filter->x1;
    filter->x1 = input;
    filter->y2 = filter->y1;
    filter->y1 = output;
    
    return output;
}

void update_filter_coefficients(plugin_t *p) {
    // Simple lowpass filter design
    double freq = p->freq_smooth.current;
    double omega = 2.0 * PI * freq / p->sample_rate;
    
    // Clamp frequency to avoid instability
    if (omega >= PI) omega = PI * 0.99;
    
    double cos_omega = cos(omega);
    double sin_omega = sin(omega);
    double alpha = sin_omega / 1.414; // Q = 1/sqrt(2) for Butterworth
    
    // Lowpass coefficients
    double b0 = (1.0 - cos_omega) / 2.0;
    double b1 = 1.0 - cos_omega;
    double b2 = (1.0 - cos_omega) / 2.0;
    double a0 = 1.0 + alpha;
    double a1 = -2.0 * cos_omega;
    double a2 = 1.0 - alpha;
    
    // Normalize by a0
    p->filter_L.b0 = b0 / a0;
    p->filter_L.b1 = b1 / a0;
    p->filter_L.b2 = b2 / a0;
    p->filter_L.a1 = a1 / a0;
    p->filter_L.a2 = a2 / a0;
    
    // Copy to right channel
    p->filter_R = p->filter_L;
    
    // Update tracking
    p->last_freq = p->freq_smooth.current;
    p->coefficients_need_update = false;
}

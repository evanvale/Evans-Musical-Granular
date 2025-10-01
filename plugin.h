#pragma once
#include <clap/clap.h>

// Plugin identification
#define PLUGIN_ID      "com.yourname.starterplugin"
#define PLUGIN_NAME    "Starter Plugin"  
#define PLUGIN_VENDOR  "Your Name"
#define PLUGIN_VERSION "0.1.0"
#define PLUGIN_DESC    "A basic CLAP plugin template"

// Parameter definitions
enum {
    PARAM_GAIN = 0,
    PARAM_FREQ,
    PARAM_DRY_WET,
    PARAM_COUNT
};

// Parameter ranges
#define GAIN_MIN 0.0
#define GAIN_MAX 2.0
#define GAIN_DEFAULT 1.0

#define FREQ_MIN 20.0
#define FREQ_MAX 20000.0
#define FREQ_DEFAULT 1000.0

#define DRY_WET_MIN 0.0
#define DRY_WET_MAX 1.0
#define DRY_WET_DEFAULT 0.5

// Math constants
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Parameter smoothing structure
typedef struct {
    float current;
    float target;
    bool active;
} param_smooth_t;

// Simple biquad filter structure
typedef struct {
    float x1, x2;  // input delay
    float y1, y2;  // output delay
    float b0, b1, b2;  // numerator coefficients
    float a1, a2;      // denominator coefficients
} biquad_t;

// Plugin state structure
typedef struct {
    clap_host_t *host;
    
    // Parameters
    double gain;
    double freq;
    double dry_wet;
    
    // Parameter smoothing
    param_smooth_t gain_smooth;
    param_smooth_t freq_smooth;
    param_smooth_t dry_wet_smooth;
    float smooth_coeff;
    bool any_smoothing_active;
    
    // DSP state
    double sample_rate;
    
    // Filters (stereo)
    biquad_t filter_L;
    biquad_t filter_R;
    
    // Parameter tracking for coefficient updates
    double last_freq;
    bool coefficients_need_update;
    
} plugin_t;

#ifdef __cplusplus
extern "C" {
#endif

// Plugin lifecycle functions (plugin.cpp)
bool plugin_init(const clap_plugin *plugin);
void plugin_destroy(const clap_plugin *plugin);
bool plugin_activate(const clap_plugin *plugin, double sample_rate, uint32_t min_frames, uint32_t max_frames);
void plugin_deactivate(const clap_plugin *plugin);
bool plugin_start_processing(const clap_plugin *plugin);
void plugin_stop_processing(const clap_plugin *plugin);
void plugin_reset(const clap_plugin *plugin);
clap_process_status plugin_process(const clap_plugin *plugin, const clap_process_t *process);
const void *plugin_get_extension(const clap_plugin *plugin, const char *id);
void plugin_on_main_thread(const clap_plugin *plugin);

// DSP functions (dsp.cpp)
void update_filter_coefficients(plugin_t *p);
void reset_filter_states(plugin_t *p);
void update_parameter_smoothing(plugin_t *p);
void trigger_parameter_smoothing(plugin_t *p);
void process_parameter_smoothing(plugin_t *p, uint32_t frames);
float process_biquad(float input, biquad_t *filter);

// Parameter handling functions (params.cpp)
extern const clap_plugin_params_t plugin_params;
extern const clap_plugin_audio_ports_t plugin_audio_ports;
extern const clap_plugin_state_t plugin_state;

#ifdef __cplusplus
}
#endif

#include "plugin.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

// Plugin features
static const char *features[] = {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    nullptr
};

// Plugin descriptor
static const clap_plugin_descriptor_t plugin_desc = {
    CLAP_VERSION,
    PLUGIN_ID,
    PLUGIN_NAME,
    PLUGIN_VENDOR,
    nullptr,        // url
    nullptr,        // manual_url  
    nullptr,        // support_url
    PLUGIN_VERSION,
    PLUGIN_DESC,
    features
};

// Plugin lifecycle implementations
bool plugin_init(const clap_plugin *plugin) {
    plugin_t *p = (plugin_t*)plugin->plugin_data;
    
    // Initialize parameter values
    p->gain = GAIN_DEFAULT;
    p->freq = FREQ_DEFAULT;
    p->dry_wet = DRY_WET_DEFAULT;
    
    // Initialize DSP state
    p->sample_rate = 44100.0;
    
    // Initialize parameter tracking
    p->last_freq = -1.0;
    p->coefficients_need_update = true;
    
    // Initialize parameter smoothing
    update_parameter_smoothing(p);
    
    // Reset filters
    reset_filter_states(p);
    
    // Calculate initial coefficients
    update_filter_coefficients(p);
    
    return true;
}

void plugin_destroy(const clap_plugin *plugin) {
    if (plugin) {
        free((void*)plugin);
    }
}

bool plugin_activate(const clap_plugin *plugin, double sample_rate, uint32_t min_frames, uint32_t max_frames) {
    plugin_t *p = (plugin_t*)plugin->plugin_data;
    
    p->sample_rate = sample_rate;
    
    // Initialize parameter smoothing with new sample rate
    update_parameter_smoothing(p);
    
    reset_filter_states(p);
    p->coefficients_need_update = true;
    
    // Sync tracking variables
    p->last_freq = p->freq;
    
    return true;
}

void plugin_deactivate(const clap_plugin *plugin) {
    // Nothing to clean up in this simple example
}

bool plugin_start_processing(const clap_plugin *plugin) {
    return true;
}

void plugin_stop_processing(const clap_plugin *plugin) {
}

void plugin_reset(const clap_plugin *plugin) {
    plugin_t *p = (plugin_t*)plugin->plugin_data;
    reset_filter_states(p);
}

clap_process_status plugin_process(const clap_plugin *plugin, const clap_process_t *process) {
    plugin_t *p = (plugin_t*)plugin->plugin_data;
    
    // Handle parameter events
    if (process->in_events) {
        uint32_t event_count = process->in_events->size(process->in_events);
        bool params_changed = false;
        
        for (uint32_t i = 0; i < event_count; ++i) {
            const clap_event_header_t *header = process->in_events->get(process->in_events, i);
            if (header->type == CLAP_EVENT_PARAM_VALUE) {
                const clap_event_param_value_t *param_event = (const clap_event_param_value_t*)header;
                
                switch (param_event->param_id) {
                    case PARAM_GAIN:
                        p->gain = fmax(GAIN_MIN, fmin(GAIN_MAX, param_event->value));
                        params_changed = true;
                        break;
                    case PARAM_FREQ:
                        p->freq = fmax(FREQ_MIN, fmin(FREQ_MAX, param_event->value));
                        p->coefficients_need_update = true;
                        params_changed = true;
                        break;
                    case PARAM_DRY_WET:
                        p->dry_wet = fmax(DRY_WET_MIN, fmin(DRY_WET_MAX, param_event->value));
                        params_changed = true;
                        break;
                }
            }
        }
        
        if (params_changed) {
            trigger_parameter_smoothing(p);
        }
    }
    
    // Check for valid inputs/outputs
    if (!process || process->audio_inputs_count == 0 || process->audio_outputs_count == 0) {
        return CLAP_PROCESS_CONTINUE;
    }
    
    const uint32_t nframes = process->frames_count;
    if (nframes == 0) return CLAP_PROCESS_CONTINUE;
    
    const clap_audio_buffer_t *input = &process->audio_inputs[0];
    const clap_audio_buffer_t *output = &process->audio_outputs[0];
    
    if (!input->data32 || !output->data32) {
        return CLAP_PROCESS_CONTINUE;
    }
    
    // Process parameter smoothing once per block to avoid zipper noise
    process_parameter_smoothing(p, nframes);
    
    // Update filter coefficients only when needed for efficiency
    if (p->coefficients_need_update) {
        update_filter_coefficients(p);
    }
    
    // Process audio channels
    const uint32_t channels = input->channel_count < output->channel_count ? 
                             input->channel_count : output->channel_count;
    
    for (uint32_t ch = 0; ch < channels; ch++) {
        if (!input->data32[ch] || !output->data32[ch]) continue;
        
        biquad_t *filter = (ch == 0) ? &p->filter_L : &p->filter_R;
        
        // Process each sample in the block
        for (uint32_t frame = 0; frame < nframes; frame++) {
            float input_sample = input->data32[ch][frame];
            
            // Apply DSP processing
            float filtered = process_biquad(input_sample, filter);
            
            // Apply smoothed gain to avoid parameter zipper noise
            filtered *= p->gain_smooth.current;
            
            // Dry/wet mix using smoothed values
            float dry_gain = 1.0f - p->dry_wet_smooth.current;
            float wet_gain = p->dry_wet_smooth.current;
            
            float output_sample = input_sample * dry_gain + filtered * wet_gain;
            
            // Safety clipper - prevents digital clipping and protects speakers/ears
            if (fabs(output_sample) > 0.95f) {
                output_sample = tanhf(output_sample * 0.7f) / 0.7f;
            }
            
            output->data32[ch][frame] = output_sample;
        }
    }
    
    return CLAP_PROCESS_CONTINUE;
}

const void *plugin_get_extension(const clap_plugin *plugin, const char *id) {
    if (strcmp(id, CLAP_EXT_PARAMS) == 0) {
        return &plugin_params;
    }
    if (strcmp(id, CLAP_EXT_STATE) == 0) {
        return &plugin_state;
    }
    if (strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) {
        return &plugin_audio_ports;
    }
    return nullptr;
}

void plugin_on_main_thread(const clap_plugin *plugin) {
}

// Plugin creation
static const clap_plugin_t *create_plugin(const clap_plugin_factory_t *factory,
                                          const clap_host_t *host,
                                          const char *plugin_id) {
    if (!plugin_id || strcmp(plugin_id, PLUGIN_ID) != 0) {
        return nullptr;
    }
    
    struct plugin_instance {
        clap_plugin_t plugin;
        plugin_t data;
    };
    
    plugin_instance *instance = (plugin_instance*)calloc(1, sizeof(plugin_instance));
    if (!instance) return nullptr;
    
    // Setup plugin data
    instance->data.host = (clap_host_t*)host;
    instance->data.gain = GAIN_DEFAULT;
    instance->data.freq = FREQ_DEFAULT;
    instance->data.dry_wet = DRY_WET_DEFAULT;
    instance->data.sample_rate = 44100.0;
    instance->data.last_freq = -1.0;
    instance->data.coefficients_need_update = true;
    
    reset_filter_states(&instance->data);
    update_parameter_smoothing(&instance->data);
    
    // Setup plugin interface
    instance->plugin.desc = &plugin_desc;
    instance->plugin.plugin_data = &instance->data;
    instance->plugin.init = plugin_init;
    instance->plugin.destroy = plugin_destroy;
    instance->plugin.activate = plugin_activate;
    instance->plugin.deactivate = plugin_deactivate;
    instance->plugin.start_processing = plugin_start_processing;
    instance->plugin.stop_processing = plugin_stop_processing;
    instance->plugin.reset = plugin_reset;
    instance->plugin.process = plugin_process;
    instance->plugin.get_extension = plugin_get_extension;
    instance->plugin.on_main_thread = plugin_on_main_thread;
    
    return &instance->plugin;
}

// Plugin factory
static uint32_t factory_get_plugin_count(const clap_plugin_factory_t *factory) {
    return 1;
}

static const clap_plugin_descriptor_t *factory_get_plugin_descriptor(const clap_plugin_factory_t *factory,
                                                                     uint32_t index) {
    return index == 0 ? &plugin_desc : nullptr;
}

static const clap_plugin_factory_t plugin_factory = {
    factory_get_plugin_count,
    factory_get_plugin_descriptor,
    create_plugin
};

// Entry point factory function
static const void *get_factory(const char *factory_id) {
    if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
        return &plugin_factory;
    }
    return nullptr;
}

// Main entry point
#ifdef _WIN32
extern "C" __declspec(dllexport)
#else
extern "C"
#endif
const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION,
    [](const char *) -> bool { return true; },
    []() -> void {},
    get_factory
};

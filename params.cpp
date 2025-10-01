#include "plugin.h"
#include <cstring>
#include <cstdio>
#include <cmath>

static uint32_t params_count(const clap_plugin *plugin) {
    return PARAM_COUNT;
}

static bool params_get_info(const clap_plugin *plugin, uint32_t param_index, clap_param_info_t *param_info) {
    if (!param_info) return false;
    
    clap_param_info_t info{};
    
    switch (param_index) {
        case PARAM_GAIN:
            info.id = PARAM_GAIN;
            info.flags = CLAP_PARAM_IS_AUTOMATABLE;
            info.min_value = GAIN_MIN;
            info.max_value = GAIN_MAX;
            info.default_value = GAIN_DEFAULT;
            strncpy(info.name, "Gain", CLAP_NAME_SIZE-1);
            strncpy(info.module, "", CLAP_PATH_SIZE-1);
            break;
        case PARAM_FREQ:
            info.id = PARAM_FREQ;
            info.flags = CLAP_PARAM_IS_AUTOMATABLE;
            info.min_value = FREQ_MIN;
            info.max_value = FREQ_MAX;
            info.default_value = FREQ_DEFAULT;
            strncpy(info.name, "Frequency", CLAP_NAME_SIZE-1);
            strncpy(info.module, "", CLAP_PATH_SIZE-1);
            break;
        case PARAM_DRY_WET:
            info.id = PARAM_DRY_WET;
            info.flags = CLAP_PARAM_IS_AUTOMATABLE;
            info.min_value = DRY_WET_MIN;
            info.max_value = DRY_WET_MAX;
            info.default_value = DRY_WET_DEFAULT;
            strncpy(info.name, "Dry/Wet", CLAP_NAME_SIZE-1);
            strncpy(info.module, "", CLAP_PATH_SIZE-1);
            break;
        default:
            return false;
    }
    
    info.name[CLAP_NAME_SIZE-1] = '\0';
    info.module[CLAP_PATH_SIZE-1] = '\0';
    *param_info = info;
    return true;
}

static bool params_get_value(const clap_plugin *plugin, clap_id param_id, double *value) {
    plugin_t *p = (plugin_t*)plugin->plugin_data;
    
    switch (param_id) {
        case PARAM_GAIN:
            *value = p->gain;
            return true;
        case PARAM_FREQ:
            *value = p->freq;
            return true;
        case PARAM_DRY_WET:
            *value = p->dry_wet;
            return true;
    }
    return false;
}

static bool params_value_to_text(const clap_plugin *plugin, clap_id param_id, double value, char *display, uint32_t size) {
    switch (param_id) {
        case PARAM_GAIN:
            snprintf(display, size, "%.2fx", value);
            return true;
        case PARAM_FREQ:
            if (value >= 1000.0) {
                snprintf(display, size, "%.1f kHz", value / 1000.0);
            } else {
                snprintf(display, size, "%.0f Hz", value);
            }
            return true;
        case PARAM_DRY_WET:
            snprintf(display, size, "%.0f%%", value * 100.0);
            return true;
    }
    return false;
}

static bool params_text_to_value(const clap_plugin *plugin, clap_id param_id, const char *display, double *value) {
    return false;
}

static void params_flush(const clap_plugin *plugin, const clap_input_events_t *in, const clap_output_events_t *out) {
    plugin_t *p = (plugin_t*)plugin->plugin_data;
    
    uint32_t event_count = in->size(in);
    for (uint32_t i = 0; i < event_count; ++i) {
        const clap_event_header_t *header = in->get(in, i);
        if (header->type == CLAP_EVENT_PARAM_VALUE) {
            const clap_event_param_value_t *param_event = (const clap_event_param_value_t*)header;
            
            switch (param_event->param_id) {
                case PARAM_GAIN:
                    p->gain = fmax(GAIN_MIN, fmin(GAIN_MAX, param_event->value));
                    break;
                case PARAM_FREQ:
                    p->freq = fmax(FREQ_MIN, fmin(FREQ_MAX, param_event->value));
                    p->coefficients_need_update = true;
                    break;
                case PARAM_DRY_WET:
                    p->dry_wet = fmax(DRY_WET_MIN, fmin(DRY_WET_MAX, param_event->value));
                    break;
            }
        }
    }
}

const clap_plugin_params_t plugin_params = {
    params_count,
    params_get_info,
    params_get_value,
    params_value_to_text,
    params_text_to_value,
    params_flush
};

// Audio ports extension
static uint32_t audio_ports_count(const clap_plugin *plugin, bool is_input) {
    return 1;
}

static bool audio_ports_get(const clap_plugin *plugin, uint32_t index, bool is_input, clap_audio_port_info_t *info) {
    if (index != 0) return false;
    
    info->id = is_input ? 0 : 1;
    strncpy(info->name, is_input ? "Audio Input" : "Audio Output", CLAP_NAME_SIZE - 1);
    info->name[CLAP_NAME_SIZE - 1] = '\0';
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->channel_count = 2;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = is_input ? CLAP_INVALID_ID : 0;
    
    return true;
}

const clap_plugin_audio_ports_t plugin_audio_ports = {
    audio_ports_count,
    audio_ports_get
};

// State extension
#define STATE_MAGIC 0x53544152  // "STAR" in hex
#define STATE_VERSION 1

static bool state_save(const clap_plugin *plugin, const clap_ostream_t *stream) {
    if (!plugin || !stream) return false;
    
    plugin_t *p = (plugin_t*)plugin->plugin_data;
    
    // Write magic and version
    uint32_t magic = STATE_MAGIC;
    uint32_t version = STATE_VERSION;
    
    if (stream->write(stream, &magic, sizeof(magic)) != sizeof(magic)) return false;
    if (stream->write(stream, &version, sizeof(version)) != sizeof(version)) return false;
    
    // Write parameter values
    double params[PARAM_COUNT] = {
        p->gain,
        p->freq,
        p->dry_wet
    };
    
    size_t params_size = sizeof(params);
    if (stream->write(stream, params, params_size) != params_size) return false;
    
    return true;
}

static bool state_load(const clap_plugin *plugin, const clap_istream_t *stream) {
    if (!plugin || !stream) return false;
    
    plugin_t *p = (plugin_t*)plugin->plugin_data;
    
    // Read and verify magic and version
    uint32_t magic, version;
    
    if (stream->read(stream, &magic, sizeof(magic)) != sizeof(magic)) return false;
    if (magic != STATE_MAGIC) return false;
    
    if (stream->read(stream, &version, sizeof(version)) != sizeof(version)) return false;
    if (version != STATE_VERSION) return false;
    
    // Read parameter values
    double params[PARAM_COUNT];
    size_t params_size = sizeof(params);
    if (stream->read(stream, params, params_size) != params_size) return false;
    
    // Assign values with bounds checking
    p->gain = fmax(GAIN_MIN, fmin(GAIN_MAX, params[0]));
    p->freq = fmax(FREQ_MIN, fmin(FREQ_MAX, params[1]));
    p->dry_wet = fmax(DRY_WET_MIN, fmin(DRY_WET_MAX, params[2]));
    
    // Update smoothed values
    p->gain_smooth.current = p->gain_smooth.target = p->gain;
    p->freq_smooth.current = p->freq_smooth.target = p->freq;
    p->dry_wet_smooth.current = p->dry_wet_smooth.target = p->dry_wet;
    
    // Reset smoothing flags
    p->gain_smooth.active = false;
    p->freq_smooth.active = false;
    p->dry_wet_smooth.active = false;
    p->any_smoothing_active = false;
    
    // Mark coefficients for update
    p->coefficients_need_update = true;
    
    return true;
}

const clap_plugin_state_t plugin_state = {
    state_save,
    state_load
};

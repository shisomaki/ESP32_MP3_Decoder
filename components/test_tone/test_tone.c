#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "esp_system.h"
#include "audio_renderer.h"
#include <math.h>

#define SAMPLE_RATE     48000
#define WAVE_FREQ_HZ    1000
#define PI 3.14159265
#define TAG "test_tone"

#define SAMPLE_PER_CYCLE SAMPLE_RATE/WAVE_FREQ_HZ

void start_test_tone(void)
{
    int *samples_data = malloc(SAMPLE_PER_CYCLE * 4);
    unsigned int i, sample_val;
    double sin_float;
    pcm_format_t buf_desc;
    
    buf_desc.sample_rate = SAMPLE_RATE;
    buf_desc.bit_depth = 16;
    buf_desc.num_channels = 2;
    
    ESP_LOGI(TAG, "SAMPLE_PER_CYCLE: %d", SAMPLE_PER_CYCLE);

    while (1)
    {
        for(i = 0; i < SAMPLE_PER_CYCLE; i++) {
            sin_float = 32767 * sin(PI * i / (SAMPLE_PER_CYCLE / 2));

            sample_val = (short) sin_float;
            sample_val = sample_val << 16;
            sample_val += (short) sin_float;
            samples_data[i] = sample_val;
        }
        render_samples((char*) samples_data, SAMPLE_PER_CYCLE * 4, &buf_desc);
    }
    free(samples_data);
}

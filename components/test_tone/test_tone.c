#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "audio_renderer.h"
#include <math.h>

#define SAMPLE_RATE     40000
#define WAVE_FREQ_HZ    1000
#define PI 3.14159265
#define TAG "test_tone"

void start_test_tone(void)
{
    int *samples_data = malloc((SAMPLE_RATE / WAVE_FREQ_HZ) * 4);
    unsigned int i, sample_val, sample_per_cycle, freq = WAVE_FREQ_HZ, mode;
    double wave_float;
    pcm_format_t buf_desc;

    mode = 0;
    sample_per_cycle = SAMPLE_RATE / freq;
    
    buf_desc.sample_rate = SAMPLE_RATE;
    buf_desc.bit_depth = 16;
    buf_desc.num_channels = 2;
    
    ESP_LOGI(TAG, "SAMPLE_PER_CYCLE: %d", sample_per_cycle);

    while (1)
    {
        if(gpio_get_level(0) == 0) {
            mode++;
            if(mode > 3)
                mode = 0;
            if(mode == 1 || mode == 3) {
                freq = 10000;
            } else {
                freq = 1000;
            }
            
            sample_per_cycle = SAMPLE_RATE / freq;
            ESP_LOGI(TAG, "SAMPLE_PER_CYCLE: %d", sample_per_cycle);
            
            while(!gpio_get_level(0)) ;
        }
        
        for(i = 0; i < sample_per_cycle; i++) {
            if(mode <= 1) {
                wave_float = 32767 * sin(PI * i / (sample_per_cycle / 2));
            } else {
                if(i < (sample_per_cycle / 2)) {
                    wave_float = 32767;
                } else {
                    wave_float = -32767;
                }
            }

            sample_val = (short) wave_float;
            sample_val = sample_val << 16;
            sample_val += (short) wave_float;
            samples_data[i] = sample_val;
        }
        render_samples((char*) samples_data, sample_per_cycle * 4, &buf_desc);
    }
    free(samples_data);
}

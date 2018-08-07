#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static TaskHandle_t *lcd_task;

// commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

#define EN    16
#define RS    17
#define D4    5
#define D5    18
#define D6    19
#define D7    21
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<EN) | (1ULL<<RS) | (1ULL<<D4) | (1ULL<<D5) | (1ULL<<D6) | (1ULL<<D7))

uint8_t _displayfunction;
uint8_t _displaycontrol;
uint8_t _displaymode;

void pulseEnable(void)
{
    gpio_set_level(EN, 0);
    vTaskDelay(1/ portTICK_PERIOD_MS);
    gpio_set_level(EN, 1);
    vTaskDelay(1/ portTICK_PERIOD_MS);
    gpio_set_level(EN, 0);
    vTaskDelay(1/ portTICK_PERIOD_MS);
}

void write4bits(uint8_t value)
{
    //D4
    if (value & 1)
        gpio_set_level(D4, 1);
    else
        gpio_set_level(D4, 0);
    //D5
    if (value & 2)
        gpio_set_level(D5, 1);
    else
        gpio_set_level(D5, 0);
    //D6
    if (value & 4)
        gpio_set_level(D6, 1);
    else
        gpio_set_level(D6, 0);
    //D7
    if (value & 8)
        gpio_set_level(D7, 1);
    else
        gpio_set_level(D7, 0);

    pulseEnable();
}

void lcd_send(uint8_t value, uint8_t mode)
{
    if (mode)
    {
        gpio_set_level(RS, 1);
    } else {
        gpio_set_level(RS, 0);
    }

    write4bits(value>>4);
    write4bits(value);
}

void lcd_command(uint8_t value)
{
    lcd_send(value, 0);
}

void lcd_write(uint8_t value)
{
    lcd_send(value, 1);
}

void LiquidCrystal_Clear(void)
{
    lcd_command(LCD_CLEARDISPLAY);
    vTaskDelay(2/ portTICK_PERIOD_MS);
}

void LiquidCrystal_init(void)
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO16/17 & GPIO18/19/20/21
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    //  RS EN LOW
    gpio_set_level(RS, 0);
    gpio_set_level(EN, 0);

    // Power on wait
    vTaskDelay(50/ portTICK_PERIOD_MS);

    write4bits(0x03);
    vTaskDelay(5/ portTICK_PERIOD_MS);

    write4bits(0x03);
    vTaskDelay(5/ portTICK_PERIOD_MS);

    write4bits(0x03);
    vTaskDelay(1/ portTICK_PERIOD_MS);

    write4bits(0x02);  // 4bit mode
    vTaskDelay(5/ portTICK_PERIOD_MS);

    _displayfunction = LCD_2LINE;
    lcd_command(LCD_FUNCTIONSET | _displayfunction);
    vTaskDelay(5/ portTICK_PERIOD_MS);

    _displaycontrol = LCD_DISPLAYON;
    lcd_command(LCD_DISPLAYCONTROL | _displaycontrol);
    vTaskDelay(5/ portTICK_PERIOD_MS);

    LiquidCrystal_Clear();

    _displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    lcd_command(LCD_ENTRYMODESET | _displaymode);
    vTaskDelay(20/ portTICK_PERIOD_MS);
}

void LCD_Addr (uint8_t value)
{
    if (value >= 80)
        return;

    if (value >= 60)
        value = 0x54 + (value - 60);
    else if (value >= 40)
        value = 0x14 + (value - 40);
    else if (value >= 20)
        value = 0x40 + (value - 20);

    lcd_command(LCD_SETDDRAMADDR | value);  // DDRAM ADDR
}

void LCD_Char (char value)
{
    lcd_write(value);
}

void LCD_Print (char *value)
{
    uint8_t i;
    for(i = 0 ; i < 20 ; i++)
    {
        if (value[i] == '\0')
            return;
        lcd_write(value[i]);
    }
}

void LCD_Print_addr (char *value, uint8_t addr)
{
    uint8_t i;
    LCD_Addr (addr);
    for(i = 0 ; i < 20 ; i++)
    {
        if (value[i] == '\0')
            return;
        lcd_write(value[i]);
    }
}

void LCD_Line_clear(uint8_t value)
{
    uint8_t i;
    LCD_Addr (value);
    for (i = 0 ; i < 20 ; i++)
        lcd_write(' ');
}

void lcd_task_init(TaskFunction_t lcd_handler_task, const uint16_t usStackDepth, void *user_data)
{
    // start lcd task
    xTaskCreatePinnedToCore(lcd_handler_task, "lcd_handler_task", usStackDepth, user_data, 10, lcd_task, 0);
}

void lcd_task_destroy()
{
    vTaskDelete(lcd_task);
}

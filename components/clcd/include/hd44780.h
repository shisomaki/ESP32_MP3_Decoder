# ifndef _CLCD_H
# define _CLCD_H

void LiquidCrystal_Clear(void);
void LiquidCrystal_init(void);
void LCD_Addr (uint8_t value);
void LCD_Char (char value);
void LCD_Print (char *value);
void LCD_Line_clear(uint8_t value);
void lcd_task_init(TaskFunction_t lcd_handler_task, const uint16_t usStackDepth, void *user_data);
void lcd_task_destroy();

# endif

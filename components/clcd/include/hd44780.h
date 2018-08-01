# ifndef _CLCD_H
# define _CLCD_H

void LiquidCrystal_Clear(void);
void LiquidCrystal_init(void);
void LCD_Addr (uint8_t value);
void LCD_Char (char value);
void LCD_Print (char *value);

# endif

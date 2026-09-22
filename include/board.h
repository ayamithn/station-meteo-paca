#ifndef BOARD_H
#define BOARD_H

void Board_Init(void);
void LED_Toggle(void);
void LED_On(void);
void LED_Off(void);
void Serial_Print(const char *msg);
void Delay_ms(int ms);

#endif
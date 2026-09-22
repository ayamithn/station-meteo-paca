#include "board.h"

int main(void)

{
 Board_Init(); //Initialise toutes les fonctionnalités nécessaires à la carte (PIN et communication série)
 Serial_Print("ça marche !\r\n"); //Affiche "ça marche !" sur le moniteur série


 while(1) //Permet au code ci-dessous de boucler continuellement tant que la carte est alimentée

 {
 LED_Toggle(); //Inverse l'état de la LED

 Delay_ms(250); //Le programme attend 250 ms avant de boucler
 }
}
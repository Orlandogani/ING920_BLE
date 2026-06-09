#include "platform_api.h"
#include "xb1_drv_adc.h"

void ADC_DRV_Init(void){
	platform_printf("ADC driver initialized\n");
}

uint16_t ADC_DRV_ReadChannel(uint8_t ch){
	return ch;
}
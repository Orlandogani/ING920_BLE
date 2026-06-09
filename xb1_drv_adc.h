#ifndef __XB1_DRV_ADC_H__
#define __XB1_DRV_ADC_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADC_MAX_VALUE 0x3FF
#define ADC_NO_FLIP 0
#define ADC_FLIP 1

typedef struct
{
    uint8_t pin;
    uint8_t ch;
    uint8_t flip;
    uint16_t adcValue; 
    uint8_t buffer[4];
}ANALOG_T;

void ADC_DRV_Init(void);
uint16_t ADC_DRV_ReadChannel(uint8_t ch);
bool ADC_DRV_Read(ANALOG_T **a, uint16_t num);
void ADC_DRV_Disable(uint32_t pin_num, uint32_t ch);
void ADC_DRV_Enable(uint32_t pin_num, uint32_t ch);


#ifdef __cplusplus
}
#endif

#endif

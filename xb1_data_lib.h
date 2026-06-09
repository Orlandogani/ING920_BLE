#ifndef __XBOX_ONE_DATA_LIB_H__
#define __XBOX_ONE_DATA_LIB_H__

#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#include "xb1_drv_adc.h"

typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;

#if 0
#pragma pack(1)

typedef union
{
    u32_t button;
    struct
    {
        u8_t b_y:1;     /*0*/
        u8_t b_x:1;     /*1*/
        u8_t b_b:1;     /*2*/
        u8_t b_a:1;     /*3*/
        u8_t b_view:1;  /*4 BACK*/
        u8_t b_menu:1;  /*5 START*/
        u8_t resev1:2;  /*6,7*/

        u8_t b_rtsb:1;  /*8 JOYSTICK KEY*/
        u8_t b_ltsb:1;  /*9 JOYSTICK KEY*/
        u8_t b_rb:1;    /*10*/
        u8_t b_lb:1;    /*11*/
        u8_t b_right:1; /*12*/
        u8_t b_left:1;  /*13*/
        u8_t b_down:1;  /*14*/
        u8_t b_up:1;    /*15*/

        u8_t resev2:2;  /*16,17*/
        u8_t b_sync:1;  /*18*/
        u8_t b_xbox:1;  /*19*/
        u8_t resev3:3;  /*20,21,22*/
        u8_t b_share:1; /*23*/

        u8_t resev4:4;  /*24,25,26,27*/
        u8_t b_b2:1;    /*28*/
        u8_t b_b1:1;    /*29*/
        u8_t b_t2:1;    /*30*/
        u8_t b_t1:1;    /*31*/
    } ;
}XBOX_ONE_BUTTON_T; // 0 : Release , 1 : Press

#endif

typedef union 
{
    u32_t button;
    struct
    {
        u8_t resev0:1;	/* 0x01 Reserved */
        u8_t Alive:1;	/* 0x02 Keep Alive */
        u8_t b_menu:1;  /* 0x04 */
        u8_t b_view:1;	/* 0x08 */
        u8_t b_a:1;     /* 0x10 */
        u8_t b_b:1; 	/* 0x20 */
        u8_t b_x:1; 	/* 0x40 */
        u8_t b_y:1;     /* 0x80 */

        u8_t b_up:1;    /* 0x100 */
        u8_t b_down:1;  /* 0x200 */
        u8_t b_left:1;  /* 0x400 */
        u8_t b_right:1; /* 0x800 */
        u8_t b_lb:1;    /* 0x1000 */ /*???*/
        u8_t b_rb:1;	/* 0x2000 */ /*???*/
        u8_t b_ltsb:1;	/* 0x4000 */ /*?????*/
        u8_t b_rtsb:1;  /* 0x8000 */ /*?????*/

        //------------------------------------

        u8_t b_share:1; /* 0x10000 */
        u8_t b_home:1;  /* 0x20000 */
        u8_t b_mute:1;	/* 0x40000 */
        u8_t b_fn:1;    /* 0x80000 */

        u8_t b_lt:1;    /* 0x100000 */ /*???????*/
        u8_t b_rt:1;    /* 0x200000 */ /*???????*/
        u8_t b_bk_l:1;  /* 0x400000 */ /*??*/
        u8_t b_bk_r:1;  /* 0x800000 */ /*??*/
       
        u8_t b_prog:1;  /* 0x1000000 */ /*???*/
        u8_t b_blk_l:1; /* 0x2000000 */ /*?????*/
        u8_t b_blk_r:1; /* 0x4000000 */ /*?????*/
        u8_t resev4:1;  /* 0x8000000 */

        u8_t resev5:1;  /* 0x10000000 */
		u8_t resev6:1;  /* 0x20000000 */
		u8_t resev7:1;  /* 0x40000000 */
		u8_t resev8:1;  /* 0x80000000 */
    };
} XBOX_ONE_BUTTON_T;


enum // u16_joystick[4]
{
	JOYSTICK_LX = 0,
	JOYSTICK_LY,
	JOYSTICK_RX,
	JOYSTICK_RY
};

enum  // u16_trigger[2]
{
	TRIGGER_LT = 0,
	TRIGGER_RT
};


typedef struct
{
	XBOX_ONE_BUTTON_T digital_key;	 // 0 : Release , 1 : Press
	u16_t u16_joystick[4];
	u16_t u16_trigger[2];
} XBOX_ONE_DATA_PACKET_T;

typedef struct
{
    uint16_t adc_max;
	uint16_t adc_mid;
    uint16_t adc_min;
	uint16_t dead_area;
    
    uint16_t rawDataLast;
    uint16_t rawDataLastReport;
    
    int16_t scalledData;
    int16_t scalledDataLast;
    
    uint8_t activity;
            
} ANALOG_SCALED_T;    


typedef struct
{
    ANALOG_T analog;
    ANALOG_SCALED_T scaled;
    
} ANALOG_INPUT_T;

#pragma pack()


bool xbone_app_handler(uint16_t Type, void *Param);

#if 0
inline uint32_t  map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max) 
{
    if((in_max - in_min)==0)//???0??????
    {
		return 0;
    }
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
#endif

// ???????,????????
static inline uint16_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max) 
{
    if (in_max <= in_min || out_max <= out_min) 
    {
        return (uint16_t)out_min;  // ??????
    }
    
    if (x <= in_min) return (uint16_t)out_min;
    if (x >= in_max) return (uint16_t)out_max;
    
    // ??64????????
    uint64_t numerator = (uint64_t)(x - in_min) * (out_max - out_min);
    uint32_t denominator = in_max - in_min;
    
    return (uint16_t)(numerator / denominator + out_min);
}


/*
 ***************************************************************

  adc ??

 ***************************************************************
*/

/*
 IIR????,??????:int filtered = alpha * adc_in + (1 - alpha) * filtered_prev;
*/

/* ???????? */
/******* ???? *******/
//#define ALPHA_Q8    200     // ???a=200/256�0.78125(Q8??)
//#define ALPHA_Q8      50      // ???a=50/256�0.19531(Q8??)50????????????
#define ALPHA_Q8      25      // ???a=25/256�0.097(Q8??)
#define ONE_MINUS_ALPHA_Q8 (256 - ALPHA_Q8)  //??(1-a)
#define Boundary_dead_zone 3  //????,???
#define FILTER_DEAD_ZONE 1024

static inline int16_t optimized_filter(int16_t adc_in, int16_t filtered_prev) 
{
    // ??32?????????
    int32_t term1 = (int32_t)adc_in * ALPHA_Q8;
    int32_t term2 = (int32_t)filtered_prev * ONE_MINUS_ALPHA_Q8;
    
    // ???????8?(????256)
    int32_t result = (term1 + term2) >> 8;
    
    // ????(??????)
    if(result > INT16_MAX) return INT16_MAX;
    if(result < INT16_MIN) return INT16_MIN;
    return (int16_t)result;
}

#endif /* __XBOX_ONE_DATA_LIB_H__ */


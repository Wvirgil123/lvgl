/**
 * @file lv_gpu_swm341_dma2d.h
 *
 */

#ifndef LV_GPU_SWM341_DMA2D_H
#define LV_GPU_SWM341_DMA2D_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../../lv_conf_internal.h"

#if LV_USE_GPU_SWM341_DMA2D
#include "../../misc/lv_color.h"
#include "../../core/lv_disp.h"
#include "../sw/lv_draw_sw.h"

/*********************
 *      DEFINES
 *********************/

#define LV_SWM341_DMA2D_ARGB8888 0
#define LV_SWM341_DMA2D_RGB888 1
#define LV_SWM341_DMA2D_RGB565 2

/**********************
 *      TYPEDEFS
 **********************/
typedef lv_draw_sw_ctx_t lv_draw_swm341_dma2d_ctx_t;

struct _lv_disp_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Turn on the peripheral and set output color mode, this only needs to be done once
 */
void lv_draw_swm341_dma2d_init(void);

void lv_draw_swm341_dma2d_ctx_init(struct _lv_disp_t * disp, lv_layer_t * layer);

void lv_draw_swm341_dma2d_ctx_deinit(struct _lv_disp_t * disp, lv_layer_t * layer);

void lv_draw_swm341_dma2d_blend(lv_layer_t * layer, const lv_draw_sw_blend_dsc_t * dsc);

void lv_gpu_swm341_dma2d_wait_cb(lv_layer_t * layer);

/**********************
 *      MACROS
 **********************/

#endif  /*LV_USE_GPU_SWM341_DMA2D*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_GPU_SWM341_DMA2D_H*/

/**
 * @file lv_draw_sw.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "../lv_draw.h"
#if LV_USE_DRAW_SW

#include "lv_draw_sw.h"
#include "../../core/lv_disp_private.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
#if LV_USE_OS
    static void render_thread_cb(void * ptr);
#endif

static void exectue_drawing(lv_draw_sw_unit_t * u);

static int32_t lv_draw_sw_dispatch(lv_draw_unit_t * draw_unit, lv_layer_t * layer);

static void lv_draw_sw_buffer_copy(lv_layer_t * layer,
                                   void * dest_buf, lv_coord_t dest_stride, const lv_area_t * dest_area,
                                   void * src_buf, lv_coord_t src_stride, const lv_area_t * src_area);

static void lv_draw_sw_buffer_convert(lv_layer_t * layer);

static void lv_draw_sw_buffer_clear(lv_layer_t * layer);

/**********************
 *  GLOBAL PROTOTYPES
 **********************/
LV_ATTRIBUTE_FAST_MEM void lv_draw_sw_img(lv_draw_unit_t * draw_unit, const lv_draw_img_dsc_t * draw_dsc,
                                          const lv_area_t * coords);

void lv_draw_sw_rect(lv_draw_unit_t * draw_unit, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords);

void lv_draw_sw_label(lv_draw_unit_t * draw_unit, const lv_draw_label_dsc_t * dsc, const lv_area_t * coords);

void lv_draw_sw_layer(lv_draw_unit_t * draw_unit, const lv_draw_img_dsc_t * draw_dsc, const lv_area_t * coords);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_layer_t * lv_draw_sw_init_ctx(lv_disp_t * disp)
{
    lv_layer_t * layer = lv_malloc(sizeof(lv_layer_t));
    LV_ASSERT_MALLOC(layer);
    lv_memzero(layer, sizeof(lv_layer_t));
    layer->color_format = disp->color_format;
    layer->buffer_copy = lv_draw_sw_buffer_copy;
    layer->buffer_convert = lv_draw_sw_buffer_convert;
    layer->buffer_clear = lv_draw_sw_buffer_clear;

    if(disp->layer_head) {
        lv_layer_t * tail = disp->layer_head;
        while(tail->next) tail = tail->next;
        tail->next = layer;
    }
    else {
        disp->layer_head = layer;
    }

    return layer;
}

void lv_draw_sw_deinit_ctx(lv_disp_t * disp, lv_layer_t * layer)
{
    LV_UNUSED(disp);
    lv_memzero(layer, sizeof(lv_layer_t));
}

void lv_draw_unit_sw_create(lv_disp_t * disp, uint32_t cnt)
{
    uint32_t i;
    for(i = 0; i < cnt; i++) {
        lv_draw_sw_unit_t * draw_sw_unit = lv_malloc(sizeof(*draw_sw_unit));
        lv_memzero(draw_sw_unit, sizeof(lv_draw_sw_unit_t));
        draw_sw_unit->base_unit.dispatch = lv_draw_sw_dispatch;
        draw_sw_unit->idx = i;

        draw_sw_unit->base_unit.next = disp->draw_unit_head;
        disp->draw_unit_head = (lv_draw_unit_t *) draw_sw_unit;

#if LV_USE_OS
        lv_thread_sync_init(&draw_sw_unit->sync);
        lv_thread_init(&draw_sw_unit->thread, LV_THREAD_PRIO_MID, render_thread_cb, 8 * 1024, draw_sw_unit);
#endif
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static int32_t lv_draw_sw_dispatch(lv_draw_unit_t * draw_unit, lv_layer_t * layer)
{
    lv_draw_sw_unit_t * draw_sw_unit = (lv_draw_sw_unit_t *) draw_unit;

    /*Return immediately if it's busy with draw task*/
    if(draw_sw_unit->task_act) return 0;

    /*Try to get an ready to draw*/
    lv_draw_task_t * t = lv_draw_get_next_available_task(layer, NULL);
    if(t == NULL) return -1;


    /*If the buffer of the layer is not allocated yet, allocate it now*/
    if(layer->buf == NULL) {
        uint8_t * buf = lv_malloc(lv_area_get_size(&layer->buf_area) * lv_color_format_get_size(layer->color_format));
        LV_ASSERT_MALLOC(buf);
        layer->buf = buf;
        layer->buffer_clear(layer);
    }

#if LV_USE_OS
    //    lv_mutex_lock(&draw_sw_unit->mutex);

    t->state = LV_DRAW_TASK_STATE_IN_PRGRESS;
    draw_sw_unit->base_unit.layer = layer;
    draw_sw_unit->base_unit.clip_area = &t->clip_area;
    draw_sw_unit->task_act = t;


    /*Let the render thread work*/
    lv_thread_sync_signal(&draw_sw_unit->sync);
    //    lv_mutex_unlock(&draw_sw_unit->mutex);
#else
    t->state = LV_DRAW_TASK_STATE_IN_PRGRESS;
    draw_sw_unit->base_unit.layer = layer;
    draw_sw_unit->base_unit.clip_area = &t->clip_area;
    draw_sw_unit->task_act = t;

    exectue_drawing(draw_sw_unit);

    draw_sw_unit->task_act->state = LV_DRAW_TASK_STATE_READY;
    draw_sw_unit->task_act = NULL;

    /*The draw unit is free now. Request a new dispatching as it can get a new task*/
    lv_draw_dispatch_request();

#endif
    return 1;
}

static void lv_draw_sw_buffer_copy(lv_layer_t * layer,
                                   void * dest_buf, lv_coord_t dest_stride, const lv_area_t * dest_area,
                                   void * src_buf, lv_coord_t src_stride, const lv_area_t * src_area)
{
    LV_UNUSED(layer);

    uint8_t px_size = lv_color_format_get_size(layer->color_format);
    uint8_t * dest_bufc =  dest_buf;
    uint8_t * src_bufc =  src_buf;

    /*Got the first pixel of each buffer*/
    dest_bufc += dest_stride * px_size * dest_area->y1;
    dest_bufc += dest_area->x1 * px_size;

    src_bufc += src_stride * px_size * src_area->y1;
    src_bufc += src_area->x1 * px_size;

    uint32_t line_length = lv_area_get_width(dest_area) * px_size;
    lv_coord_t y;
    for(y = dest_area->y1; y <= dest_area->y2; y++) {
        lv_memcpy(dest_bufc, src_bufc, line_length);
        dest_bufc += dest_stride;
        src_bufc += src_stride;
    }
}

static void lv_draw_sw_buffer_convert(lv_layer_t * layer)
{
    //    /*Keep the rendered image as it is*/
    //    if(layer->color_format == LV_COLOR_FORMAT_NATIVE) return;
    //
    //#if LV_COLOR_DEPTH == 8
    //    if(layer->color_format == LV_COLOR_FORMAT_L8) return;
    //#endif
    //
    //#if LV_COLOR_DEPTH == 16
    //    if(layer->color_format == LV_COLOR_FORMAT_RGB565) return;
    //
    //    /*Make both the clip and buf area relative to the buf area*/
    //    if(layer->color_format == LV_COLOR_FORMAT_NATIVE_REVERSED) {
    //        uint32_t px_cnt = lv_area_get_size(layer->buf_area);
    //        uint32_t u32_cnt = px_cnt / 2;
    //        uint16_t * buf16 = layer->buf;
    //        uint32_t * buf32 = (uint32_t *) buf16 ;
    //
    //        /*Swap all byte pairs*/
    //        while(u32_cnt >= 8) {
    //            buf32[0] = ((uint32_t)(buf32[0] & 0xff00ff00) >> 8) + ((uint32_t)(buf32[0] & 0x00ff00ff) << 8);
    //            buf32[1] = ((uint32_t)(buf32[1] & 0xff00ff00) >> 8) + ((uint32_t)(buf32[1] & 0x00ff00ff) << 8);
    //            buf32[2] = ((uint32_t)(buf32[2] & 0xff00ff00) >> 8) + ((uint32_t)(buf32[2] & 0x00ff00ff) << 8);
    //            buf32[3] = ((uint32_t)(buf32[3] & 0xff00ff00) >> 8) + ((uint32_t)(buf32[3] & 0x00ff00ff) << 8);
    //            buf32[4] = ((uint32_t)(buf32[4] & 0xff00ff00) >> 8) + ((uint32_t)(buf32[4] & 0x00ff00ff) << 8);
    //            buf32[5] = ((uint32_t)(buf32[5] & 0xff00ff00) >> 8) + ((uint32_t)(buf32[5] & 0x00ff00ff) << 8);
    //            buf32[6] = ((uint32_t)(buf32[6] & 0xff00ff00) >> 8) + ((uint32_t)(buf32[6] & 0x00ff00ff) << 8);
    //            buf32[7] = ((uint32_t)(buf32[7] & 0xff00ff00) >> 8) + ((uint32_t)(buf32[7] & 0x00ff00ff) << 8);
    //            buf32 += 8;
    //            u32_cnt -= 8;
    //        }
    //
    //        while(u32_cnt) {
    //            *buf32 = ((uint32_t)(*buf32 & 0xff00ff00) >> 8) + ((uint32_t)(*buf32 & 0x00ff00ff) << 8);
    //            buf32++;
    //            u32_cnt--;
    //        }
    //
    //        if(px_cnt & 0x1) {
    //            uint32_t e = px_cnt - 1;
    //            buf16[e] = ((buf16[e] & 0xff00) >> 8) + ((buf16[e] & 0x00ff) << 8);
    //        }
    //
    //        return;
    //    }
    //#endif
    //
    //    size_t buf_size_px = lv_area_get_size(&layer->buf_area);
    //
    //    bool has_alpha = lv_color_format_has_alpha(layer->color_format);
    //    uint8_t px_size_in = lv_color_format_get_size(layer->color_format);
    //    uint8_t px_size_out = lv_color_format_get_size(display->color_format);
    //
    //    /*In-plpace conversation can happen only when converting to a smaller pixel size*/
    //    if(px_size_in >= px_size_out) {
    //        if(has_alpha) lv_color_from_native_alpha(layer->buf, layer->buf, layer->color_format, buf_size_px);
    //        else lv_color_convert_from_native(layer->buf, layer->buf, layer->color_format, buf_size_px);
    //    }
    //    else {
    //        /*TODO What to to do when can't perform in-place conversion?*/
    //        LV_LOG_WARN("Can't convert to the desired color format (%d)", layer->color_format);
    //    }
    //    return;
    //
    //    LV_LOG_WARN("Couldn't convert the image to the desired format");
}

static void lv_draw_sw_buffer_clear(lv_layer_t * layer)
{
    uint8_t px_size = lv_color_format_get_size(layer->color_format);
    uint8_t * buf8 = layer->buf;
    lv_area_t a;
    lv_area_copy(&a, &layer->clip_area);
    lv_area_move(&a, -layer->buf_area.x1, -layer->buf_area.y1);
    lv_coord_t w = lv_area_get_width(&a);
    buf8 += a.y1 * w * px_size;
    buf8 += a.x1 * px_size;

    lv_coord_t y;
    for(y = a.y1; y <= a.y2; y++) {
        lv_memzero(buf8, w * px_size);
        buf8 += w * px_size;
    }
}

#if LV_USE_OS
static void render_thread_cb(void * ptr)
{
    lv_draw_sw_unit_t * u = ptr;

    while(1) {
        while(u->task_act == NULL) {
            lv_thread_sync_wait(&u->sync);
        }

        exectue_drawing(u);

        /*Cleaup*/
        u->task_act->state = LV_DRAW_TASK_STATE_READY;
        u->task_act = NULL;

        /*The draw unit is free now. Request a new dispatching as it can get a new task*/
        lv_draw_dispatch_request();
    }
}
#endif

static void exectue_drawing(lv_draw_sw_unit_t * u)
{
    /*Render the draw task*/
    switch(u->task_act->type) {
        case LV_DRAW_TASK_TYPE_RECTANGLE:
            lv_draw_sw_rect((lv_draw_unit_t *)u, u->task_act->draw_dsc, &u->task_act->area);
            break;
        case LV_DRAW_TASK_TYPE_LABEL:
            lv_draw_sw_label((lv_draw_unit_t *)u, u->task_act->draw_dsc, &u->task_act->area);
            break;
        case LV_DRAW_TASK_TYPE_IMAGE:
            lv_draw_sw_img((lv_draw_unit_t *)u, u->task_act->draw_dsc, &u->task_act->area);
            break;
        case LV_DRAW_TASK_TYPE_LAYER:
            lv_draw_sw_layer((lv_draw_unit_t *)u, u->task_act->draw_dsc, &u->task_act->area);
            break;
        default:
            break;
    }
}

#endif /*LV_USE_DRAW_SW*/

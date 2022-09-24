/**
 * @file lv_disp.h
 *
 */

#ifndef LV_DISP_H
#define LV_DISP_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../hal/lv_hal.h"
#include "lv_obj.h"
#include "lv_theme.h"

/*********************
 *      DEFINES
 *********************/
#ifndef LV_INV_BUF_SIZE
#define LV_INV_BUF_SIZE 32 /*Buffer size for invalid areas*/
#endif

#ifndef LV_ATTRIBUTE_FLUSH_READY
#define LV_ATTRIBUTE_FLUSH_READY
#endif

/**********************
 *      TYPEDEFS
 **********************/

struct _lv_obj_t;
struct _lv_theme_t;

typedef enum {
    LV_DISP_ROTATION_NONE = 0,
    LV_DISP_ROTATION_90,
    LV_DISP_ROTATION_180,
    LV_DISP_ROTATION_270
} lv_disp_rotation_t;

typedef enum {
    LV_DISP_EVENT_INVALIDATED_AREA,
    LV_DISP_EVENT_RENDER_START,
    LV_DISP_EVENT_RENDER_READY,
    LV_DISP_EVENT_PARAMETER_CHANGED,
} lv_disp_even_t;

typedef enum {
    LV_DISP_RENDER_MODE_PARTIAL,
    LV_DISP_RENDER_MODE_DIRECT,
    LV_DISP_RENDER_MODE_FULL,
} lv_disp_render_mode_t;

/**
 * Display Driver structure to be registered by HAL.
 * Only its pointer will be saved in `lv_disp_t` so it should be declared as
 * `static lv_disp_drv_t my_drv` or allocated dynamically.
 */
typedef struct _lv_disp_t {

    /*---------------------
     * Resolution
     *--------------------*/

    /** Horizontal resolution.*/
    lv_coord_t hor_res;

    /** Vertical resolution.*/
    lv_coord_t ver_res;

    /** Horizontal resolution of the full / physical display. Set to -1 for fullscreen mode.*/
    lv_coord_t physical_hor_res;

    /** Vertical resolution of the full / physical display. Set to -1 for fullscreen mode.*/
    lv_coord_t physical_ver_res;

    /** Horizontal offset from the full / physical display. Set to 0 for fullscreen mode.*/
    lv_coord_t offset_x;

    /** Vertical offset from the full / physical display. Set to 0 for fullscreen mode.*/
    lv_coord_t offset_y;

    uint32_t dpi;              /** DPI (dot per inch) of the display. Default value is `LV_DPI_DEF`.*/

    /*---------------------
     * Buffering
     *--------------------*/

    /** First display buffer.*/
    void * draw_buf_1;

    /** Second display buffer.*/
    void * draw_buf_2;

    /** Internal, used by the library*/
    void * draw_buf_act;

    /** In pixel count*/
    uint32_t draw_buf_size;

    /** MANDATORY: Write the internal buffer (draw_buf) to the display. 'lv_disp_flush_ready()' has to be
     * called when finished*/
    void (*flush_cb)(struct _lv_disp_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);

    /*1: flushing is in progress. (It can't be a bit field because when it's cleared from IRQ Read-Modify-Write issue might occur)*/
    volatile int flushing;

    /*1: It was the last chunk to flush. (It can't be a bit field because when it's cleared from IRQ Read-Modify-Write issue might occur)*/
    volatile int flushing_last;
    volatile uint32_t last_area         : 1; /*1: the last area is being rendered*/
    volatile uint32_t last_part         : 1; /*1: the last part of the current area is being rendered*/

    lv_disp_render_mode_t render_mode;
    uint32_t antialiasing : 1;       /**< 1: anti-aliasing is enabled on this display.*/
    uint32_t screen_transp : 1;      /**Handle if the screen doesn't have a solid (opa == LV_OPA_COVER) background.*/

    /** 1: The current screen rendering is in progress*/
    uint32_t rendering_in_progress : 1;

    lv_color_format_t   color_format;

    /** Invalidated (marked to redraw) areas*/
    lv_area_t inv_areas[LV_INV_BUF_SIZE];
    uint8_t inv_area_joined[LV_INV_BUF_SIZE];
    uint16_t inv_p;
    int32_t inv_en_cnt;

    /*---------------------
     * Draw context
     *--------------------*/

    lv_draw_ctx_t * draw_ctx;
    void (*draw_ctx_init)(struct _lv_disp_t * disp_drv, lv_draw_ctx_t * draw_ctx);
    void (*draw_ctx_deinit)(struct _lv_disp_t * disp_drv, lv_draw_ctx_t * draw_ctx);
    size_t draw_ctx_size;

    /*---------------------
     * Screens
     *--------------------*/

    /** Screens of the display*/
    struct _lv_obj_t ** screens;    /**< Array of screen objects.*/
    struct _lv_obj_t * act_scr;     /**< Currently active screen on this display*/
    struct _lv_obj_t * prev_scr;    /**< Previous screen. Used during screen animations*/
    struct _lv_obj_t * scr_to_load; /**< The screen prepared to load in lv_scr_load_anim*/
    struct _lv_obj_t * top_layer;   /**< @see lv_disp_get_layer_top*/
    struct _lv_obj_t * sys_layer;   /**< @see lv_disp_get_layer_sys*/
    uint32_t screen_cnt;
    uint8_t draw_prev_over_act  : 1;/** 1: Draw previous screen over active screen*/
    uint8_t del_prev  : 1; /** 1: Automatically delete the previous screen when the screen load animation is ready*/

    /*---------------------
     * Background
     *--------------------*/

    lv_opa_t bg_opa;                /**<Opacity of the background color or wallpaper*/
    lv_color_t bg_color;            /**< Default display color when screens are transparent*/
    const void * bg_img;            /**< An image source to display as wallpaper*/

    /*---------------------
     * Others
     *--------------------*/

#if LV_USE_USER_DATA
    void * user_data; /**< Custom display driver user data*/
#endif

    uint32_t sw_rotate : 1;          /**< 1: use software rotation (slower)*/
    uint32_t rotated : 2;            /**< 1: turn the display by 90 degree. @warning Does not update coordinates for you!*/

    /**< The theme assigned to the screen*/
    struct _lv_theme_t * theme;

    /** A timer which periodically checks the dirty areas and refreshes them*/
    lv_timer_t * refr_timer;

    /*Miscellaneous data*/
    uint32_t last_activity_time;        /**< Last time when there was activity on this display*/

    /** OPTIONAL: Called periodically while lvgl waits for operation to be completed.
     * For example flushing or GPU
     * User can execute very simple tasks here or yield the task*/
    void (*wait_cb)(struct _lv_disp_t * disp_drv);

    /** On CHROMA_KEYED images this color will be transparent.
     * `LV_COLOR_CHROMA_KEY` by default. (lv_conf.h) */
    lv_color_t color_chroma_key;
} lv_disp_t;


typedef enum {
    LV_SCR_LOAD_ANIM_NONE,
    LV_SCR_LOAD_ANIM_OVER_LEFT,
    LV_SCR_LOAD_ANIM_OVER_RIGHT,
    LV_SCR_LOAD_ANIM_OVER_TOP,
    LV_SCR_LOAD_ANIM_OVER_BOTTOM,
    LV_SCR_LOAD_ANIM_MOVE_LEFT,
    LV_SCR_LOAD_ANIM_MOVE_RIGHT,
    LV_SCR_LOAD_ANIM_MOVE_TOP,
    LV_SCR_LOAD_ANIM_MOVE_BOTTOM,
    LV_SCR_LOAD_ANIM_FADE_IN,
    LV_SCR_LOAD_ANIM_FADE_ON = LV_SCR_LOAD_ANIM_FADE_IN, /*For backward compatibility*/
    LV_SCR_LOAD_ANIM_FADE_OUT,
    LV_SCR_LOAD_ANIM_OUT_LEFT,
    LV_SCR_LOAD_ANIM_OUT_RIGHT,
    LV_SCR_LOAD_ANIM_OUT_TOP,
    LV_SCR_LOAD_ANIM_OUT_BOTTOM,
} lv_scr_load_anim_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

lv_disp_t * lv_disp_create(void);

void lv_disp_init(lv_disp_t * disp);

/**
 * Remove a display
 * @param disp pointer to display
 */
void lv_disp_remove(lv_disp_t * disp);

/**
 * Set a default display. The new screens will be created on it by default.
 * @param disp pointer to a display
 */
void lv_disp_set_default(lv_disp_t * disp);

/**
 * Get the default display
 * @return pointer to the default display
 */
lv_disp_t * lv_disp_get_default(void);

/**
 * Get the next display.
 * @param disp pointer to the current display. NULL to initialize.
 * @return the next display or NULL if no more. Give the first display when the parameter is NULL
 */
lv_disp_t * lv_disp_get_next(lv_disp_t * disp);

/*---------------------
 * RESOLUTION
 *--------------------*/

void lv_disp_set_resolution(lv_disp_t * disp, lv_coord_t hor_res, lv_coord_t ver_res);

void lv_disp_set_physical_resolution(lv_disp_t * disp, lv_coord_t hor_res, lv_coord_t ver_res);

void lv_disp_set_offset(lv_disp_t * disp, lv_coord_t x, lv_coord_t y);

void lv_disp_set_dpi(lv_disp_t * disp, lv_coord_t dpi);

/**
 * Get the DPI of the display
 * @param disp pointer to a display (NULL to use the default display)
 * @return dpi of the display
 */
lv_coord_t lv_disp_get_dpi(const lv_disp_t * disp);

/**
 * Get the horizontal resolution of a display
 * @param disp pointer to a display (NULL to use the default display)
 * @return the horizontal resolution of the display
 */
lv_coord_t lv_disp_get_horizonal_resolution(const lv_disp_t * disp);

/**
 * Get the vertical resolution of a display
 * @param disp pointer to a display (NULL to use the default display)
 * @return the vertical resolution of the display
 */
lv_coord_t lv_disp_get_vertical_resolution(const lv_disp_t * disp);

/**
 * Get the full / physical horizontal resolution of a display
 * @param disp pointer to a display (NULL to use the default display)
 * @return the full / physical horizontal resolution of the display
 */
lv_coord_t lv_disp_get_physical_horizontal_resolution(const lv_disp_t * disp);

/**
 * Get the full / physical vertical resolution of a display
 * @param disp pointer to a display (NULL to use the default display)
 * @return the full / physical vertical resolution of the display
 */
lv_coord_t lv_disp_get_physical_vertical_resolution(const lv_disp_t * disp);

/**
 * Get the horizontal offset from the full / physical display
 * @param disp pointer to a display (NULL to use the default display)
 * @return the horizontal offset from the full / physical display
 */
lv_coord_t lv_disp_get_offset_x(const lv_disp_t * disp);

/**
 * Get the vertical offset from the full / physical display
 * @param disp pointer to a display (NULL to use the default display)
 * @return the horizontal offset from the full / physical display
 */
lv_coord_t lv_disp_get_offset_y(const lv_disp_t * disp);

/*---------------------
 * BUFFERING
 *--------------------*/

void lv_disp_set_draw_buffers(lv_disp_t * disp, void * buf1, void * buf2, uint32_t buf_size_px,
                              lv_disp_render_mode_t render_mode);

void lv_disp_set_flush_cb(lv_disp_t * disp, void (*flush_cb)(struct _lv_disp_t * disp, const lv_area_t * area,
                                                             lv_color_t * color_p));

void lv_disp_set_color_format(lv_disp_t * disp, lv_color_format_t color_format);

void lv_disp_set_antialaising(lv_disp_t * disp, bool en);

/**
 * Get if anti-aliasing is enabled for a display or not
 * @param disp pointer to a display (NULL to use the default display)
 * @return true: anti-aliasing is enabled; false: disabled
 */
bool lv_disp_get_antialiasing(lv_disp_t * disp);


//! @cond Doxygen_Suppress

/**
 * Call in the display driver's `flush_cb` function when the flushing is finished
 * @param disp_drv pointer to display driver in `flush_cb` where this function is called
 */
LV_ATTRIBUTE_FLUSH_READY void lv_disp_flush_ready(lv_disp_t * disp);

/**
 * Tell if it's the last area of the refreshing process.
 * Can be called from `flush_cb` to execute some special display refreshing if needed when all areas area flushed.
 * @param disp_drv pointer to display driver
 * @return true: it's the last area to flush; false: there are other areas too which will be refreshed soon
 */
LV_ATTRIBUTE_FLUSH_READY bool lv_disp_flush_is_last(lv_disp_t * disp);

//! @endcond

/*---------------------
 * DRAW CONTEXT
 *--------------------*/

void lv_disp_set_draw_ctx(lv_disp_t * disp,
                          void (*draw_ctx_init)(lv_disp_t * disp, lv_draw_ctx_t * draw_ctx),
                          void (*draw_ctx_deinit)(lv_disp_t * disp, lv_draw_ctx_t * draw_ctx),
                          size_t draw_ctx_size);

/*---------------------
  * SCREENS
  *--------------------*/

/**
 * Return with a pointer to the active screen
 * @param disp pointer to display which active screen should be get. (NULL to use the default
 * screen)
 * @return pointer to the active screen object (loaded by 'lv_scr_load()')
 */
lv_obj_t * lv_disp_get_scr_act(lv_disp_t * disp);

/**
 * Return with a pointer to the previous screen. Only used during screen transitions.
 * @param disp pointer to display which previous screen should be get. (NULL to use the default
 * screen)
 * @return pointer to the previous screen object or NULL if not used now
 */
lv_obj_t * lv_disp_get_scr_prev(lv_disp_t * disp);

/**
 * Make a screen active
 * @param scr pointer to a screen
 */
void lv_disp_load_scr(lv_obj_t * scr);

/**
 * Return with the top layer. (Same on every screen and it is above the normal screen layer)
 * @param disp pointer to display which top layer should be get. (NULL to use the default screen)
 * @return pointer to the top layer object (transparent screen sized lv_obj)
 */
lv_obj_t * lv_disp_get_layer_top(lv_disp_t * disp);

/**
 * Return with the sys. layer. (Same on every screen and it is above the normal screen and the top
 * layer)
 * @param disp pointer to display which sys. layer should be retrieved. (NULL to use the default screen)
 * @return pointer to the sys layer object (transparent screen sized lv_obj)
 */
lv_obj_t * lv_disp_get_layer_sys(lv_disp_t * disp);

/**
 * Switch screen with animation
 * @param scr pointer to the new screen to load
 * @param anim_type type of the animation from `lv_scr_load_anim_t`, e.g. `LV_SCR_LOAD_ANIM_MOVE_LEFT`
 * @param time time of the animation
 * @param delay delay before the transition
 * @param auto_del true: automatically delete the old screen
 */
void lv_scr_load_anim(lv_obj_t * scr, lv_scr_load_anim_t anim_type, uint32_t time, uint32_t delay, bool auto_del);

/**
 * Get the active screen of the default display
 * @return pointer to the active screen
 */
static inline lv_obj_t * lv_scr_act(void)
{
    return lv_disp_get_scr_act(lv_disp_get_default());
}

/**
 * Get the top layer  of the default display
 * @return pointer to the top layer
 */
static inline lv_obj_t * lv_layer_top(void)
{
    return lv_disp_get_layer_top(lv_disp_get_default());
}

/**
 * Get the active screen of the default display
 * @return  pointer to the sys layer
 */
static inline lv_obj_t * lv_layer_sys(void)
{
    return lv_disp_get_layer_sys(lv_disp_get_default());
}

static inline void lv_scr_load(lv_obj_t * scr)
{
    lv_disp_load_scr(scr);
}

/*---------------------
 * Background
 *--------------------*/

/**
 * Set the background color of a display
 * @param disp pointer to a display
 * @param color color of the background
 */
void lv_disp_set_bg_color(lv_disp_t * disp, lv_color_t color);

/**
 * Set the background image of a display
 * @param disp pointer to a display
 * @param img_src path to file or pointer to an `lv_img_dsc_t` variable
 */
void lv_disp_set_bg_image(lv_disp_t * disp, const void  * img_src);

/**
 * Set opacity of the background
 * @param disp pointer to a display
 * @param opa opacity (0..255)
 */
void lv_disp_set_bg_opa(lv_disp_t * disp, lv_opa_t opa);

/**
 * Set the background color of a display
 * @param disp pointer to a display
 * @return color of the background
 */
lv_color_t lv_disp_get_bg_color(lv_disp_t * disp);

/**
 * Set the background image of a display
 * @param disp pointer to a display
 * @return path to file or pointer to an `lv_img_dsc_t` variable
 */
const void * lv_disp_get_bg_image(lv_disp_t * disp);

/**
 * Set opacity of the background
 * @param disp pointer to a display
 * @return opacity (0..255)
 */
lv_opa_t lv_disp_get_bg_opa(lv_disp_t * disp);


/*---------------------
 * Others
 *--------------------*/

/**
 * Set the rotation of this display.
 * @param disp pointer to a display (NULL to use the default display)
 * @param rotation rotation angle
 */
void lv_disp_set_rotation(lv_disp_t * disp, lv_disp_rotation_t rotation);

/**
 * Get the current rotation of this display.
 * @param disp pointer to a display (NULL to use the default display)
 * @return rotation angle
 */
lv_disp_rotation_t lv_disp_get_rotation(lv_disp_t * disp);

/**
 * Set the theme of a display
 * @param disp pointer to a display
 */
void lv_disp_set_theme(lv_disp_t * disp, lv_theme_t * th);

/**
 * Get the theme of a display
 * @param disp pointer to a display
 * @return the display's theme (can be NULL)
 */
lv_theme_t * lv_disp_get_theme(lv_disp_t * disp);

/**
 * Get elapsed time since last user activity on a display (e.g. click)
 * @param disp pointer to a display (NULL to get the overall smallest inactivity)
 * @return elapsed ticks (milliseconds) since the last activity
 */
uint32_t lv_disp_get_inactive_time(const lv_disp_t * disp);

/**
 * Manually trigger an activity on a display
 * @param disp pointer to a display (NULL to use the default display)
 */
void lv_disp_trig_activity(lv_disp_t * disp);

/**
 * Temporarily enable and disable the invalidation of the display.
 * @param disp pointer to a display (NULL to use the default display)
 * @param en true: enable invalidation; false: invalidation
 */
void lv_disp_enable_invalidation(lv_disp_t * disp, bool en);

/**
 * Get display invalidation is enabled.
 * @param disp pointer to a display (NULL to use the default display)
 * @return return true if invalidation is enabled
 */
bool lv_disp_is_invalidation_enabled(lv_disp_t * disp);

/**
 * Get a pointer to the screen refresher timer to
 * modify its parameters with `lv_timer_...` functions.
 * @param disp pointer to a display
 * @return pointer to the display refresher timer. (NULL on error)
 */
lv_timer_t * _lv_disp_get_refr_timer(lv_disp_t * disp);

lv_color_t lv_disp_get_chroma_key_color(lv_disp_t * disp);

/**********************
 *      MACROS
 **********************/

/*------------------------------------------------
 * To improve backward compatibility
 * Recommended only if you have one display
 *------------------------------------------------*/

#ifndef LV_HOR_RES
/**
 * The horizontal resolution of the currently active display.
 */
#define LV_HOR_RES lv_disp_get_horizonal_resolution(lv_disp_get_default())
#endif

#ifndef LV_VER_RES
/**
 * The vertical resolution of the currently active display.
 */
#define LV_VER_RES lv_disp_get_vertical_resolution(lv_disp_get_default())
#endif


/**
 * Same as Android's DIP. (Different name is chosen to avoid mistype between LV_DPI and LV_DIP)
 * 1 dip is 1 px on a 160 DPI screen
 * 1 dip is 2 px on a 320 DPI screen
 * https://stackoverflow.com/questions/2025282/what-is-the-difference-between-px-dip-dp-and-sp
 */
#define _LV_DPX_CALC(dpi, n)   ((n) == 0 ? 0 :LV_MAX((( (dpi) * (n) + 80) / 160), 1)) /*+80 for rounding*/
#define LV_DPX(n)   _LV_DPX_CALC(lv_disp_get_dpi(NULL), n)

/**
 * Scale the given number of pixels (a distance or size) relative to a 160 DPI display
 * considering the DPI of the default display.
 * It ensures that e.g. `lv_dpx(100)` will have the same physical size regardless to the
 * DPI of the display.
 * @param n     the number of pixels to scale
 * @return      `n x current_dpi/160`
 */
static inline lv_coord_t lv_dpx(lv_coord_t n)
{
    return LV_DPX(n);
}

/**
 * Scale the given number of pixels (a distance or size) relative to a 160 DPI display
 * considering the DPI of the given display.
 * It ensures that e.g. `lv_dpx(100)` will have the same physical size regardless to the
 * DPI of the display.
 * @param obj   a display whose dpi should be considered
 * @param n     the number of pixels to scale
 * @return      `n x current_dpi/160`
 */
static inline lv_coord_t lv_disp_dpx(const lv_disp_t * disp, lv_coord_t n)
{
    return _LV_DPX_CALC(lv_disp_get_dpi(disp), n);
}


#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DISP_H*/

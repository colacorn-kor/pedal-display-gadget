/* ============================================================================
 *  display_bringup.c  —  ST7796 3.5" SPI 디스플레이 브링업 (ESP-IDF v5.x)
 *  esp_lcd_st7796 + esp_lvgl_port (LVGL v9). SPI 패널 + LVGL 디스플레이 등록.
 *
 *  컴포넌트 추가:
 *    idf.py add-dependency "lvgl/lvgl^9"
 *    idf.py add-dependency "espressif/esp_lvgl_port^2.8"
 *    idf.py add-dependency "espressif/esp_lcd_st7796"
 *
 *  ⚠ esp_lvgl_port가 LVGL 태스크를 자체 구동 → lv_timer_handler 직접 호출 금지.
 *  ⚠ 모든 LVGL 호출은 lvgl_port_lock()/unlock()로 감쌀 것(멀티태스크).
 * ========================================================================== */
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7796.h"
#include "esp_lvgl_port.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "display_bringup.h"
#include <stdint.h>

/* ── 확정 핀맵: EstarDyn ST7796S, SPI 4-line ─────────────────────────────── */
#define LCD_SPI_HOST  SPI2_HOST
#define PIN_SCLK  12
#define PIN_MOSI  13
#define PIN_CS     2
#define PIN_DC    21
#define PIN_RST   14
#define PIN_BL     1        /* 백라이트 EN (전류 크면 트랜지스터 경유) */
#define LCD_HRES  480
#define LCD_VRES  320
#define LCD_PCLK_MHZ  10
#define LCD_PCLK      (LCD_PCLK_MHZ * 1000 * 1000)

#define LCD_BRINGUP_TEST 0
#define LCD_SWAP_XY     1
#define LCD_MIRROR_X    1
#define LCD_MIRROR_Y    0
#define LCD_GAP_X       0
#define LCD_GAP_Y       0
#define LCD_INVERT      0
#define LCD_RGB_BGR     1
#define LCD_SWAP_BYTES  1

#define LCD_TEST_STRIP_ROWS 40

#define RGB565_RED   0xF800
#define RGB565_GREEN 0x07E0
#define RGB565_BLUE  0x001F
#define RGB565_WHITE 0xFFFF

static const char *TAG = "display";

#if LCD_BRINGUP_TEST
static uint16_t lcd_test_strip[LCD_HRES * LCD_TEST_STRIP_ROWS];

static uint16_t lcd_color(uint16_t rgb565)
{
#if LCD_SWAP_BYTES
    return (uint16_t)((rgb565 >> 8) | (rgb565 << 8));
#else
    return rgb565;
#endif
}

static esp_err_t lcd_test_pattern(esp_lcd_panel_handle_t panel)
{
    ESP_LOGI(TAG,
             "LCD test pattern: %dx%d pclk=%dMHz swap=%d mirror=(%d,%d) gap=(%d,%d) invert=%d bgr=%d",
             LCD_HRES, LCD_VRES, LCD_PCLK_MHZ, LCD_SWAP_XY,
             LCD_MIRROR_X, LCD_MIRROR_Y, LCD_GAP_X, LCD_GAP_Y,
             LCD_INVERT, LCD_RGB_BGR);

    for (int y0 = 0; y0 < LCD_VRES; y0 += LCD_TEST_STRIP_ROWS) {
        int y1 = y0 + LCD_TEST_STRIP_ROWS;
        if (y1 > LCD_VRES) {
            y1 = LCD_VRES;
        }
        int rows = y1 - y0;

        for (int y = 0; y < rows; y++) {
            int screen_y = y0 + y;
            for (int x = 0; x < LCD_HRES; x++) {
                uint16_t color = RGB565_GREEN;
                if (screen_y < 9) {
                    color = RGB565_BLUE;
                }
                if (x < 40 && screen_y < 40) {
                    color = RGB565_RED;
                }
                if (x >= LCD_HRES - 40 && screen_y >= LCD_VRES - 40) {
                    color = RGB565_WHITE;
                }
                lcd_test_strip[y * LCD_HRES + x] = lcd_color(color);
            }
        }

        esp_err_t err = esp_lcd_panel_draw_bitmap(panel, 0, y0, LCD_HRES, y1,
                                                  lcd_test_strip);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "test pattern draw failed at y=%d: %s",
                     y0, esp_err_to_name(err));
            return err;
        }
    }
    return ESP_OK;
}
#endif

lv_display_t* bsp_display_init(void)
{
    /* 백라이트 핀 (초기화 끝나고 켜야 깜빡임 없음) */
    gpio_config_t bk={ .mode=GPIO_MODE_OUTPUT, .pin_bit_mask=1ULL<<PIN_BL };
    ESP_ERROR_CHECK(gpio_config(&bk));
    ESP_ERROR_CHECK(gpio_set_level(PIN_BL,0));

    /* SPI 버스 */
    spi_bus_config_t bus={
        .sclk_io_num=PIN_SCLK, .mosi_io_num=PIN_MOSI, .miso_io_num=-1,
        .quadwp_io_num=-1, .quadhd_io_num=-1,
        .max_transfer_sz=LCD_HRES*80*sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST,&bus,SPI_DMA_CH_AUTO));

    /* 패널 IO (SPI) */
    esp_lcd_panel_io_handle_t io=NULL;
    esp_lcd_panel_io_spi_config_t iocfg={
        .dc_gpio_num=PIN_DC, .cs_gpio_num=PIN_CS, .pclk_hz=LCD_PCLK,
        .lcd_cmd_bits=8, .lcd_param_bits=8, .spi_mode=0, .trans_queue_depth=10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST,&iocfg,&io));

    /* ST7796 패널 */
    esp_lcd_panel_handle_t panel=NULL;
    /*
     * Vendor init hook placeholder. Do not invent register values here; fill
     * this only after obtaining the EstarDyn/ST7796S module vendor sequence.
     *
     * static const st7796_lcd_init_cmd_t vendor_init_cmds[] = {
     *     // { cmd, data, data_len, delay_ms },
     * };
     * static const st7796_vendor_config_t vendor_config = {
     *     .init_cmds = vendor_init_cmds,
     *     .init_cmds_size = sizeof(vendor_init_cmds) / sizeof(vendor_init_cmds[0]),
     * };
     */
    esp_lcd_panel_dev_config_t pcfg={
        .reset_gpio_num=PIN_RST,
        .rgb_ele_order=LCD_RGB_BGR ? LCD_RGB_ELEMENT_ORDER_BGR
                                   : LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel=16,
        /* .vendor_config=&vendor_config, */
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7796(io,&pcfg,&panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel,LCD_INVERT != 0));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel,LCD_SWAP_XY != 0));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel,LCD_MIRROR_X != 0,LCD_MIRROR_Y != 0));
    /*
     * Gap offsets are controller address-window offsets. With swap_xy enabled,
     * the visible horizontal/vertical axes are rotated, so tune these together
     * with LCD_SWAP_XY/LCD_MIRROR_X/LCD_MIRROR_Y while observing the test pattern.
     */
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel,LCD_GAP_X,LCD_GAP_Y));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel,true));

#if LCD_BRINGUP_TEST
    esp_err_t test_err = lcd_test_pattern(panel);
    if (test_err != ESP_OK) {
        esp_lcd_panel_del(panel);
        esp_lcd_panel_io_del(io);
        spi_bus_free(LCD_SPI_HOST);
        return NULL;
    }
    ESP_ERROR_CHECK(gpio_set_level(PIN_BL,1));
    ESP_LOGI(TAG,"LCD_BRINGUP_TEST active; LVGL display registration skipped");
    return NULL;
#else
    /* LVGL 포트 + 디스플레이 등록 (자체 LVGL 태스크 생성) */
    lvgl_port_cfg_t lp=ESP_LVGL_PORT_INIT_CONFIG();
    lp.task_affinity=0;                         /* Core 1은 오디오 전용 */
    ESP_ERROR_CHECK(lvgl_port_init(&lp));
    const lvgl_port_display_cfg_t dc={
        .io_handle=io, .panel_handle=panel,
        .buffer_size=LCD_HRES*40,
        .double_buffer=true,
        .hres=LCD_HRES, .vres=LCD_VRES,
        .color_format=LV_COLOR_FORMAT_RGB565,
        .flags={ .buff_dma=true, .buff_spiram=false, .swap_bytes=true },
    };
    lv_display_t *display=lvgl_port_add_disp(&dc);
    if(!display){
        ESP_LOGE(TAG,"lvgl_port_add_disp failed");
        lvgl_port_deinit();
        esp_lcd_panel_del(panel);
        esp_lcd_panel_io_del(io);
        spi_bus_free(LCD_SPI_HOST);
        return NULL;
    }
    ESP_ERROR_CHECK(gpio_set_level(PIN_BL,1));  /* 등록 성공 후 백라이트 ON */
    return display;
#endif
}

/* ── 스모크 테스트: 전체 앱 붙이기 전에 이걸로 "첫 불" 확인 ────────────────
 *  app_main 임시 교체 → 백라이트 켜지고 "HELLO ST7796" 보이면 성공.
 *
 *  void app_main(void){
 *      bsp_display_init();
 *      lvgl_port_lock(0);
 *      lv_obj_t* l=lv_label_create(lv_screen_active());
 *      lv_label_set_text(l,"HELLO ST7796");
 *      lv_obj_set_style_text_font(l,&lv_font_montserrat_28,0);
 *      lv_obj_center(l);
 *      lvgl_port_unlock();
 *  }
 * ========================================================================== */

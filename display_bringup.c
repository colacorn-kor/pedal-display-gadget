/* ============================================================================
 *  display_bringup.c  —  ST7796 3.5" SPI 디스플레이 브링업 (ESP-IDF v5.x)
 *  esp_lcd_st7796 + esp_lvgl_port (LVGL v9). SPI 패널 + LVGL 디스플레이 등록.
 *
 *  컴포넌트 추가:
 *    idf.py add-dependency "lvgl/lvgl^9"
 *    idf.py add-dependency "espressif/esp_lvgl_port^2.6"
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

/* ── 핀 (앞서 쓴 I2S 15-18 / 버튼 4-7 / 뮤트 21 / USB 19-20 / 옥타PSRAM·플래시
        26-37 / 스트래핑 0·3·45·46 와 충돌 없는 GPIO) ───────────────────────── */
#define LCD_SPI_HOST  SPI2_HOST
#define PIN_SCLK  12
#define PIN_MOSI  11
#define PIN_CS    10
#define PIN_DC     9
#define PIN_RST   14
#define PIN_BL     8        /* 백라이트 EN (전류 크면 트랜지스터 경유) */
#define LCD_HRES  480
#define LCD_VRES  320
#define LCD_PCLK  (40*1000*1000)   /* 우선 40MHz, 안정되면 80MHz 시도 */

static const char *TAG = "display";

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
    esp_lcd_panel_dev_config_t pcfg={
        .reset_gpio_num=PIN_RST,
        .rgb_ele_order=LCD_RGB_ELEMENT_ORDER_BGR,   /* 색 R↔B 뒤바뀌면 RGB로 */
        .bits_per_pixel=16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7796(io,&pcfg,&panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel,true));   /* 네거티브로 보이면 false */
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel,true));         /* 480x320 가로 */
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel,true,false));    /* 상하/좌우 뒤집히면 조정 */
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel,true));

    /* LVGL 포트 + 디스플레이 등록 (자체 LVGL 태스크 생성) */
    lvgl_port_cfg_t lp=ESP_LVGL_PORT_INIT_CONFIG();
    lp.task_affinity=0;                         /* Core 1은 오디오 전용 */
    ESP_ERROR_CHECK(lvgl_port_init(&lp));
    const lvgl_port_display_cfg_t dc={
        .io_handle=io, .panel_handle=panel,
        .buffer_size=LCD_HRES*40,               /* 부분버퍼(내부 DMA RAM) */
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

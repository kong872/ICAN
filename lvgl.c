#include "lvgl-v8.2/lvgl.h"
#include "lv_drivers/lv_drm.h"
#include "evdev.h"
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>

// 首页图片资源
#include "car1.c"
#include "car2.c"
#include "weizhi.c"
#include "chepai.c"
#include "luxian.c"
// 界面3图片资源
#include "chemo.c"
#include "rukou.c"
#include "dianti.c"
#include "erweima.c"
// 字体声明
extern const lv_font_t lv_hz75;
extern const lv_font_t lv_hzc40;
extern const lv_font_t lv_hz24;
extern const lv_font_t lv_font_montserrat_24;
extern const lv_font_t lv_hz30;
extern const lv_font_t lv_hz48;
extern const lv_font_t lv_font_montserrat_18;

extern evdev_device_t global_dsc;
void evdev_read(lv_indev_drv_t * drv, lv_indev_data_t * data);

// 全局
static lv_disp_t *g_disp = NULL;
#define SCREEN_W 1024
#define SCREEN_H 600
#define SLOT_W 120
#define SLOT_H 180
static pthread_t lv_tick_thread_id;
static bool thread_run_flag = true;
static lv_obj_t *g_search_box = NULL;
static void show_tip_popup(const char *tip_text);

//目标高亮车位，0代表无高亮，7=7号车位
static uint8_t g_target_slot_num = 0;



// ========= 只新增：车牌白名单 + 判断函数 =========
static const char *valid_plate[] = {
    "川GX9999",
    "川GXX666",
    "川GF0000",
    "川A8888X",
    "苏A25567",
    "苏A33376",
    "苏A60788",
    "苏A38882"
};
#define VALID_PLATE_CNT (sizeof(valid_plate)/sizeof(valid_plate[0]))

static int is_right_plate(const char *str)
{
    if(str == NULL || strlen(str) == 0)
        return 0;
    for(int i=0; i<VALID_PLATE_CNT; i++)
    {
        if(strcmp(str, valid_plate[i]) == 0)
            return 1;
    }
    return 0;
}
// ==============================================

// 时钟线程
void *lv_tick_thread(void *arg)
{
    (void)arg;
    while(thread_run_flag)
    {
        lv_tick_inc(1);
        usleep(1000);
    }
    pthread_exit(NULL);
}

// 退出信号处理
void sigint_handler(int sig)
{
    (void)sig;
    thread_run_flag = false;
    if (g_disp)
    {
        lv_disp_remove(g_disp);
    }
    drm_exit();
    lv_deinit();
    printf("Exit safely\n");
    exit(0);
}

// ===================== 界面3 停车场地图UI =====================
typedef struct {
    lv_coord_t x;
    lv_coord_t y;
    uint8_t num;
    lv_obj_t *obj;
} ParkingSlot_t;

static ParkingSlot_t slot_list[10] = {
    {790, 350, 1, NULL},
    {670, 350, 2, NULL},
    {550, 350, 3, NULL},
    {320, 350, 4, NULL},
    {790, 70,  5, NULL},
    {670, 70,  6, NULL},
    {550, 70,  7, NULL},
    {320, 70,  8, NULL},
    {200, 70,  9, NULL},
    {200, 350, 10,NULL}
};

static void create_single_parking_slot(lv_obj_t *parent, ParkingSlot_t *slot)
{
    lv_obj_t *slot_frame = lv_obj_create(parent);
    lv_obj_set_size(slot_frame, SLOT_W, SLOT_H);
    lv_obj_set_pos(slot_frame, slot->x, slot->y);
    lv_obj_set_style_bg_opa(slot_frame, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(slot_frame, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(slot_frame, 3, 0);
    lv_obj_set_style_radius(slot_frame, 0, 0);
    lv_obj_set_style_shadow_width(slot_frame, 0, 0);
    lv_obj_set_style_pad_all(slot_frame, 0, 0);

    lv_obj_t *slot_label = lv_label_create(slot_frame);
    char num_buf[4];
    sprintf(num_buf, "%d", slot->num);
    lv_label_set_text(slot_label, num_buf);
    lv_obj_set_style_text_font(slot_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(slot_label, lv_color_hex(0x000000), 0);
    lv_obj_center(slot_label);

    slot->obj = slot_frame; //保存句柄
}


void home_page_create(void);
static void create_car_input_ui(void);

// 原版返回按钮回调 原样不动
static void back_btn_cb(lv_event_t *e)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    create_car_input_ui();
}
static void create_parking_map_ui(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    
    // 左上角返回按钮
    lv_obj_t *back_btn = lv_btn_create(scr);
    lv_obj_set_size(back_btn, 60, 45);
    lv_obj_set_pos(back_btn, 20, 20);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(back_btn, 4, 0);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_text = lv_label_create(back_btn);
    lv_label_set_text(back_text, "←");
    lv_obj_set_style_text_font(back_text, &lv_hz24, 0);
    lv_obj_set_style_text_color(back_text, lv_color_white(), 0);
    lv_obj_center(back_text);

    for(int i = 0; i < 10; i++)
    {
        create_single_parking_slot(scr, &slot_list[i]);
    }

    //高亮目标车位，半透明红色
    if(g_target_slot_num != 0)
    {
        for(int i=0;i<10;i++)
        {
            if(slot_list[i].num == g_target_slot_num)
            {
                lv_obj_set_style_bg_color(slot_list[i].obj, lv_color_hex(0xff0000), 0);
                lv_obj_set_style_bg_opa(slot_list[i].obj, LV_OPA_50, 0);
                break;
            }
        }
    }

    lv_obj_t * img_chemo1 = lv_img_create(scr);
    lv_img_set_src(img_chemo1, &chemo);
    lv_obj_set_size(img_chemo1, 80, 158);
    lv_obj_align(img_chemo1, LV_ALIGN_TOP_LEFT,220 , 81);
    lv_obj_t * img_chemo2 = lv_img_create(scr);
    lv_img_set_src(img_chemo2, &chemo);
    lv_obj_set_size(img_chemo2, 80, 158);
    lv_obj_align(img_chemo2, LV_ALIGN_TOP_LEFT,220 , 361);
    lv_obj_t * img_rukou = lv_img_create(scr);
    lv_img_set_src(img_rukou, &rukou);
    lv_obj_set_size(img_rukou, 150, 49);
    lv_obj_align(img_rukou, LV_ALIGN_TOP_LEFT,420 , 550);
    lv_obj_t * img_dianti = lv_img_create(scr);
    lv_img_set_src(img_dianti, &dianti);
    lv_obj_set_size(img_dianti, 100, 70);
    lv_obj_align(img_dianti, LV_ALIGN_TOP_LEFT,10 , 265);
   // ========== 二维码 放在右上角，最后创建，顶层不被遮挡 ==========
    lv_obj_t * img_erweima = lv_img_create(scr);
    lv_img_set_src(img_erweima, &erweima);   
    lv_obj_set_size(img_erweima, 150, 150);  
    lv_obj_align(img_erweima, LV_ALIGN_TOP_RIGHT, 0,0 );
    //实心绿色矩形：左上角(110,300)，长830(水平宽度)，高度20
    lv_obj_t* green_rect = lv_obj_create(scr);
    lv_obj_set_size(green_rect, 500, 20);
    lv_obj_set_pos(green_rect, 110, 290);
    //去掉圆角、边框、阴影
    lv_obj_set_style_radius(green_rect, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(green_rect, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(green_rect, 0, LV_PART_MAIN);
    //填充绿色，完全不透明
    lv_obj_set_style_bg_color(green_rect, lv_color_hex(0x00b42a), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(green_rect, LV_OPA_COVER, LV_PART_MAIN);
    //实心绿色矩形：左上角(500,250)，宽20，高60
    lv_obj_t* green_rect2 = lv_obj_create(scr);
    lv_obj_set_size(green_rect2, 20, 60);
    lv_obj_set_pos(green_rect2, 610, 250);
    //去掉圆角、边框、阴影
    lv_obj_set_style_radius(green_rect2, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(green_rect2, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(green_rect2, 0, LV_PART_MAIN);
    //填充和上面一模一样的绿色
    lv_obj_set_style_bg_color(green_rect2, lv_color_hex(0x00b42a), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(green_rect2, LV_OPA_COVER, LV_PART_MAIN);
}


// ===================== 界面2 按键回调函数 =====================
static void btn_cancel_cb(lv_event_t *e)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    home_page_create();
}

// ========== 修改后的查询按钮回调（只改这里逻辑） ==========
static void btn_search_cb(lv_event_t *e)
{
    if(g_search_box == NULL) return;
    const char *input_str = lv_textarea_get_text(g_search_box);
    
    // 空输入
    if(input_str == NULL || strlen(input_str) == 0)
    {
        show_tip_popup("请输入车牌号");
        return;
    }
    // 判断是否是合法车牌
    if(!is_right_plate(input_str))
    {
        show_tip_popup("查无此车");
        return;
    }
    // 正确才跳转
    g_target_slot_num = 7;
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    create_parking_map_ui();
}

static void key_add_char_cb(lv_event_t *e)
{
    if(g_search_box == NULL) return;
    const char *str = lv_event_get_user_data(e);
    lv_textarea_add_text(g_search_box, str);
}
static void mask_click_close_cb(lv_event_t *e)
{
    lv_obj_t *mask = lv_event_get_target(e);
    lv_obj_del(mask);
}

static void show_tip_popup(const char *tip_text)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_t *mask = lv_obj_create(scr);
    lv_obj_set_size(mask, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(mask, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_60, 0);
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(mask, 0, 0);
    lv_obj_add_event_cb(mask, mask_click_close_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *popup = lv_obj_create(mask);
    lv_obj_set_size(popup, 320, 120);
    lv_obj_center(popup);
    lv_obj_set_style_bg_color(popup, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_radius(popup, 12, 0);
    lv_obj_set_style_shadow_width(popup, 10, 0);

    lv_obj_t *tip_label = lv_label_create(popup);
    lv_label_set_text(tip_label, tip_text);
    lv_obj_set_style_text_font(tip_label, &lv_hz30, 0);
    lv_obj_set_style_text_color(tip_label, lv_color_hex(0x333333), 0);
    lv_obj_center(tip_label);
}
static void backspace_click_cb(lv_event_t *e)
{
    if(g_search_box == NULL) return;
    const char *current_text = lv_textarea_get_text(g_search_box);
    if(current_text == NULL || strlen(current_text) == 0)
        return;
    int len = strlen(current_text);
    lv_textarea_set_cursor_pos(g_search_box, len);
    lv_textarea_del_char(g_search_box);
}

static void clear_all_click_cb(lv_event_t *e)
{
    if(g_search_box == NULL) return;
    lv_textarea_set_text(g_search_box, "");
}

static void create_car_input_ui(void)
{
    printf("==== 加载静态车牌输入界面2 ====\n");
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x222222), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "请输入车牌号查询");
    lv_obj_set_style_text_font(title, &lv_hz30, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *search_box = lv_textarea_create(scr);
    lv_obj_set_size(search_box, 600, 48);
    lv_textarea_set_one_line(search_box, true);
    lv_obj_align_to(search_box, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 40);

    g_search_box = search_box;
    lv_obj_set_style_text_font(search_box, &lv_hz24, 0);

    lv_obj_t *btn_cancel = lv_btn_create(scr);
    lv_obj_set_size(btn_cancel, 80, 48);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0xe6212a), 0);
    lv_obj_add_event_cb(btn_cancel, btn_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "取消");
    lv_obj_set_style_text_font(label_cancel, &lv_hz24, 0);
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_center(label_cancel);
    lv_obj_align_to(btn_cancel, search_box, LV_ALIGN_OUT_BOTTOM_LEFT, 10, 30);

    lv_obj_t *btn_search = lv_btn_create(scr);
    lv_obj_set_size(btn_search, 80, 48);
    lv_obj_set_style_bg_color(btn_search, lv_color_hex(0x00b42a), 0);
    lv_obj_add_event_cb(btn_search, btn_search_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label_search = lv_label_create(btn_search);
    lv_label_set_text(label_search, "查询");
    lv_obj_set_style_text_font(label_search, &lv_hz24, 0);
    lv_obj_set_style_text_color(label_search, lv_color_white(), 0);
    lv_obj_center(label_search);
    lv_obj_align_to(btn_search, search_box, LV_ALIGN_OUT_BOTTOM_RIGHT,-10 , 30);

    lv_obj_t *keyboard_frame = lv_obj_create(scr);
    lv_obj_set_scroll_dir(keyboard_frame, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(keyboard_frame, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(keyboard_frame, 980, 252);
    lv_obj_align(keyboard_frame, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(keyboard_frame, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(keyboard_frame, 2, 0);
    lv_obj_set_style_border_color(keyboard_frame, lv_color_white(), 0);
    lv_obj_set_scrollbar_mode(keyboard_frame, LV_SCROLLBAR_MODE_OFF);

    const int row_num = 4;
    const int col_num = 13;
    int key_w = (956 - 20) / col_num;
    int key_h = (240 - 20) / row_num;
    int space_x = 2;
    int space_y = 4;

    for(int row = 0; row < row_num; row++)
    {
        for(int col = 0; col < col_num; col++)
        {
            if((row == 0 || row == 3) && col >= 10) continue;
            lv_obj_t *key_btn = lv_btn_create(keyboard_frame);
            lv_obj_set_size(key_btn, key_w , key_h );
            lv_obj_clear_flag(key_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(key_btn, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_radius(key_btn,0,0);
            lv_obj_set_style_shadow_width(key_btn,0,0);
            lv_obj_set_style_pad_all(key_btn,0,0);
            int x_pos = -12 + col * key_w + col*space_x;
            int y_pos = -12 + row * key_h + row*space_y;
            lv_obj_align(key_btn, LV_ALIGN_TOP_LEFT, x_pos, y_pos);

            if(row == 0 && col < 10)
            {
                const char *ch_list[] = {"黑","吉","辽","川","湘","浙","豫","苏","云","鲁"};
                lv_obj_t *label = lv_label_create(key_btn);
                lv_label_set_text(label, ch_list[col]);
                lv_obj_set_style_text_font(label, &lv_hz24, 0);
                lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);
                lv_obj_center(label);              
                lv_obj_add_flag(key_btn, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_add_event_cb(key_btn, key_add_char_cb, LV_EVENT_CLICKED, (void*)ch_list[col]);
                
            }
            if(row == 1)
            {
                const char *alpha_list[] = {"A","B","C","D","E","F","G","H","I","J","K","L","M"};
                lv_obj_t *label = lv_label_create(key_btn);
                lv_label_set_text(label, alpha_list[col]);
                lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
                lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);
                lv_obj_center(label);

                lv_obj_add_flag(key_btn, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_add_event_cb(key_btn, key_add_char_cb, LV_EVENT_CLICKED, (void*)alpha_list[col]);
            }
            if(row == 2)
            {
                const char *alpha_list[] = {"N","O","P","Q","R","S","T","U","V","W","X","Y","Z"};
                lv_obj_t *label = lv_label_create(key_btn);
                lv_label_set_text(label, alpha_list[col]);
                lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
                lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);
                lv_obj_center(label);

                lv_obj_add_flag(key_btn, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_add_event_cb(key_btn, key_add_char_cb, LV_EVENT_CLICKED, (void*)alpha_list[col]);
            }
            if(row == 3 && col < 10)
            {
                const char *num_list[] = {"0","1","2","3","4","5","6","7","8","9"};
                lv_obj_t *label = lv_label_create(key_btn);
                lv_label_set_text(label, num_list[col]);
                lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
                lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);
                lv_obj_center(label);

                lv_obj_add_flag(key_btn, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_add_event_cb(key_btn, key_add_char_cb, LV_EVENT_CLICKED, (void*)num_list[col]);
            }
        }
    }

    lv_obj_t *merge_btn1 = lv_btn_create(keyboard_frame);
    int merge_w = 3 * key_w + 2 * space_x;
    lv_obj_set_size(merge_btn1, merge_w, key_h);
    lv_obj_set_style_bg_color(merge_btn1, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(merge_btn1, 0, 0);
    lv_obj_set_style_shadow_width(merge_btn1, 0, 0);
    lv_obj_set_style_pad_all(merge_btn1, 0, 0);
    int merge1_x = -12 + 10 * key_w + 10 * space_x;
    int merge1_y = -12 + 0 * key_h + 0 * space_y;
    lv_obj_align(merge_btn1, LV_ALIGN_TOP_LEFT, merge1_x, merge1_y);
    lv_obj_t *label_backspace = lv_label_create(merge_btn1);
    lv_label_set_text(label_backspace, "←");
    lv_obj_set_style_text_font(label_backspace, &lv_hz48, 0);
    lv_obj_set_style_text_color(label_backspace, lv_color_hex(0x000000), 0);
    lv_obj_center(label_backspace);
    lv_obj_add_event_cb(merge_btn1, backspace_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *merge_btn4 = lv_btn_create(keyboard_frame);
    lv_obj_set_size(merge_btn4, merge_w, key_h);
    lv_obj_set_style_bg_color(merge_btn4, lv_color_hex(0xFFFFFF),0);
    lv_obj_set_style_radius(merge_btn4,0,0);
    lv_obj_set_style_shadow_width(merge_btn4,0,0);
    lv_obj_set_style_pad_all(merge_btn4,0,0);
    int merge4_x = -12 + 10 * key_w + 10 * space_x;
    int merge4_y = -12 + 3 * key_h + 3 * space_y;
    lv_obj_align(merge_btn4, LV_ALIGN_TOP_LEFT, merge4_x, merge4_y);
    lv_obj_t *label_clear = lv_label_create(merge_btn4);
    lv_label_set_text(label_clear, "清  空");
    lv_obj_set_style_text_font(label_clear, &lv_hz24, 0);
    lv_obj_set_style_text_color(label_clear, lv_color_hex(0x000000), 0);
    lv_obj_center(label_clear);
    lv_obj_add_event_cb(merge_btn4, clear_all_click_cb, LV_EVENT_CLICKED, NULL);
}

static void ring_click_event_cb(lv_event_t * e)
{
    printf("\n=====================================\n");
    printf("【DEBUG】点击触发，准备跳转界面\n");
    lv_obj_t * scr = lv_scr_act();
    lv_obj_clean(scr);
    printf("【DEBUG】首页界面已清空\n");
    create_car_input_ui();
    printf("【DEBUG】成功切换到车牌输入键盘界面\n");
    printf("=====================================\n\n");
}

void home_page_create(void)
{
    printf("==== 开始创建首页UI ====\n");
    lv_obj_t * scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F1829), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t * title_label = lv_label_create(scr);
    lv_label_set_text(title_label, "反向寻车");
    lv_obj_set_style_text_font(title_label, &lv_hz75, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0,10);

    lv_obj_t * ring_main = lv_obj_create(scr);
    lv_obj_clear_flag(ring_main, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ring_main, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ring_main, ring_click_event_cb, LV_EVENT_CLICKED, NULL);
    printf("【DEBUG】圆环点击事件绑定完成\n");

    lv_obj_set_size(ring_main,300, 300);
    lv_obj_align(ring_main, LV_ALIGN_CENTER, 0, -30);
    lv_obj_set_style_bg_opa(ring_main, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_opa(ring_main, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_obj_t * ring1 = lv_obj_create(ring_main);
    lv_obj_clear_flag(ring1, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(ring1, 300, 300);
    lv_obj_align(ring1, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(ring1, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_radius(ring1, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(ring1, 4, LV_PART_MAIN);
    lv_obj_set_style_border_color(ring1, lv_color_hex(0x80D8FF), LV_PART_MAIN);
    lv_obj_set_style_border_opa(ring1, 70, LV_PART_MAIN);

    lv_obj_t * ring2 = lv_obj_create(ring_main);
    lv_obj_clear_flag(ring2, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(ring2, 250, 250);
    lv_obj_align(ring2, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(ring2, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_radius(ring2, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(ring2, 4, LV_PART_MAIN);
    lv_obj_set_style_border_color(ring2, lv_color_hex(0x80D8FF), LV_PART_MAIN);
    lv_obj_set_style_border_opa(ring2, 70, LV_PART_MAIN);

    lv_obj_t * ring3 = lv_obj_create(ring_main);
    lv_obj_clear_flag(ring3, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(ring3, 200, 200);
    lv_obj_align(ring3, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(ring3, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_radius(ring3, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(ring3, 6, LV_PART_MAIN);
    lv_obj_set_style_border_color(ring3, lv_color_hex(0xA8E0FF), LV_PART_MAIN);
    lv_obj_set_style_border_opa(ring3, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t * center_text = lv_label_create(ring_main);
    lv_label_set_text(center_text, "点击开始");
    lv_obj_clear_flag(center_text, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_text_font(center_text, &lv_hzc40, LV_PART_MAIN);
    lv_obj_set_style_text_color(center_text, lv_color_hex(0xA8E0FF), LV_PART_MAIN);
    lv_obj_align(center_text, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t * img_car1 = lv_img_create(scr);
    lv_img_set_src(img_car1, &car1);
    lv_obj_set_size(img_car1, 200, 94);
    lv_obj_align(img_car1, LV_ALIGN_TOP_MID, -300, 233);

    lv_obj_t * img_car2 = lv_img_create(scr);
    lv_img_set_src(img_car2, &car2);
    lv_obj_set_size(img_car2, 200, 94);
    lv_obj_align(img_car2, LV_ALIGN_TOP_MID, 300, 233);

    lv_obj_t * btn_group = lv_obj_create(scr);
    lv_obj_clear_flag(btn_group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn_group, 520, 100);
    lv_obj_align(btn_group, LV_ALIGN_CENTER, 0, 200);
    lv_obj_set_style_bg_opa(btn_group, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_opa(btn_group, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_obj_t * img_chepai = lv_img_create(btn_group);
    lv_img_set_src(img_chepai, &chepai);
    lv_obj_set_size(img_chepai, 70, 51);
    lv_obj_align(img_chepai, LV_ALIGN_LEFT_MID, 0, -20);
    lv_obj_t * txt_chepai = lv_label_create(btn_group);
    lv_label_set_text(txt_chepai, "查车牌");
    lv_obj_set_style_text_font(txt_chepai, &lv_hz24, LV_PART_MAIN);
    lv_obj_set_style_text_color(txt_chepai, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(txt_chepai, img_chepai, LV_ALIGN_OUT_BOTTOM_MID, 0, 7.5);

    lv_obj_t * img_weizhi = lv_img_create(btn_group);
    lv_img_set_src(img_weizhi, &weizhi);
    lv_obj_set_size(img_weizhi, 80, 50);
    lv_obj_align(img_weizhi, LV_ALIGN_CENTER, 0, -20);
    lv_obj_t * txt_weizhi = lv_label_create(btn_group);
    lv_label_set_text(txt_weizhi, "找位置");
    lv_obj_set_style_text_font(txt_weizhi, &lv_hz24, LV_PART_MAIN);
    lv_obj_set_style_text_color(txt_weizhi, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(txt_weizhi, img_weizhi, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);

    lv_obj_t * img_luxian = lv_img_create(btn_group);
    lv_img_set_src(img_luxian, &luxian);
    lv_obj_set_size(img_luxian, 90, 42);
    lv_obj_align(img_luxian, LV_ALIGN_RIGHT_MID, 0, -18);
    lv_obj_t * txt_luxian = lv_label_create(btn_group);
    lv_label_set_text(txt_luxian, "导路线");
    lv_obj_set_style_text_font(txt_luxian, &lv_hz24, LV_PART_MAIN);
    lv_obj_set_style_text_color(txt_luxian, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(txt_luxian, img_luxian, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    lv_obj_t *txt_info = lv_label_create(scr);
    lv_label_set_text(txt_info, "B1层 空闲车位");
    lv_obj_set_style_text_font(txt_info, &lv_hz24, LV_PART_MAIN);
    lv_obj_set_style_text_color(txt_info, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(txt_info, LV_ALIGN_BOTTOM_RIGHT, -100, -10);

    lv_obj_t *num_label = lv_label_create(scr);
    lv_label_set_text(num_label, "6");
    lv_obj_set_style_text_font(num_label, &lv_hzc40, LV_PART_MAIN);
    lv_obj_set_style_text_color(num_label, lv_color_hex(0xFF7800), LV_PART_MAIN);
    lv_obj_align_to(num_label, txt_info, LV_ALIGN_OUT_RIGHT_MID, 8, -5);

    printf("==== 首页UI全部创建完成 ====\n");
}

int main(void)
{
    signal(SIGINT, sigint_handler);
    lv_init();

    FILE *dpms_fd = fopen("/sys/class/drm/card0-DSI-1/dpms", "w");
    if(dpms_fd != NULL)
    {
        fputs("On", dpms_fd);
        fclose(dpms_fd);
    }

    lv_disp_drv_t disp_drv;
    drm_disp_drv_init(&disp_drv);
    g_disp = lv_disp_drv_register(&disp_drv);

    evdev_init();
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = evdev_read;
    lv_indev_drv_register(&indev_drv);
    printf("触摸设备初始化完成\n");

    home_page_create();

    pthread_create(&lv_tick_thread_id, NULL, lv_tick_thread, NULL);
    pthread_detach(lv_tick_thread_id);

    while (1)
    {
        lv_timer_handler();
        usleep(1000);
    }

    drm_exit();
    lv_deinit();
    return 0;
}

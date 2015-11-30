#include <pebble.h>
#include <pebble_chart.h>
#include <data-processor.h>
#include <cgm_info.h>

#define ANTIALIASING true

typedef struct {
    int hours;
    int minutes;
} Time;

static Window * s_main_window;
static Layer * s_canvas_layer;

static AppTimer *timer;

static GPoint s_center;
static Time s_last_time, s_anim_time;
static int s_radius = 0, t_delta = 3, has_launched = 0, vibe_state = 1, alert_state = 0 ,s_anim_hours_60 = 0;
static int * bgs;
static int * bg_times;
static int num_bgs = 0;
static int tag_raw = 0;
static bool s_animating = false;

static GBitmap *icon_bitmap = NULL;

static BitmapLayer * icon_layer;
static TextLayer * bg_layer, *delta_layer, *time_delta_layer, *time_layer, *date_layer;

static char date_app_text[] = "";
static char last_bg[124];
static char data_id[255] = "99";
static char time_delta_str[124] = "";
static char time_text[124] = "";

static ChartLayer* chart_layer;

enum CgmKey {
    CGM_EGV_DELTA_KEY = 0x0,
    CGM_EGV_KEY = 0x1,
    CGM_TREND_KEY = 0x2,
    CGM_ALERT_KEY = 0x3,
    CGM_VIBE_KEY = 0x4,
    CGM_ID = 0x5,
    CGM_TIME_DELTA_KEY = 0x6,
	CGM_BGS = 0x7,
    CGM_BG_TIMES = 0x8
};


enum Alerts {
    OKAY = 0x0,
    LOSS_MID_NO_NOISE = 0x1,
    LOSS_HIGH_NO_NOISE = 0x2,
    NO_CHANGE = 0x3,
    OLD_DATA = 0x4,
};

static int s_color_channels[3] = { 85, 85, 85 };
static int b_color_channels[3] = { 0, 0, 0 };

static const uint32_t const error[] = { 100,100,100,100,100 };

static const uint32_t CGM_ICONS[] = {
    RESOURCE_ID_IMAGE_NONE_WHITE,	  //4 - 0
    RESOURCE_ID_IMAGE_UPUP_WHITE,     //0 - 1
    RESOURCE_ID_IMAGE_UP_WHITE,       //1 - 2
    RESOURCE_ID_IMAGE_UP45_WHITE,     //2 - 3
    RESOURCE_ID_IMAGE_FLAT_WHITE,     //3 - 4
    RESOURCE_ID_IMAGE_DOWN45_WHITE,   //5 - 5
    RESOURCE_ID_IMAGE_DOWN_WHITE,     //6 - 6
    RESOURCE_ID_IMAGE_DOWNDOWN_WHITE,  //7 - 7
};

char *translate_error(AppMessageResult result) {
    switch (result) {
        case APP_MSG_OK: return "OK";
        case APP_MSG_SEND_TIMEOUT: return "ast";
        case APP_MSG_SEND_REJECTED: return "asr"; 
        case APP_MSG_NOT_CONNECTED: return "anc"; 
        case APP_MSG_APP_NOT_RUNNING: return "anr"; 
        case APP_MSG_INVALID_ARGS: return "aia"; 
        case APP_MSG_BUSY: return "aby"; 
        case APP_MSG_BUFFER_OVERFLOW: return "abo"; 
        case APP_MSG_ALREADY_RELEASED: return "aar";
        case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "car"; 
        case APP_MSG_CALLBACK_NOT_REGISTERED: return "cnr";
        case APP_MSG_OUT_OF_MEMORY: return "oom";
        case APP_MSG_CLOSED: return "acd";
        case APP_MSG_INTERNAL_ERROR: return "aie";
        default: return "uer";
    }
}

/************************************ UI **************************************/
void send_cmd(void) {
    APP_LOG(APP_LOG_LEVEL_INFO, "send_cmd");
    
    if (s_canvas_layer) {
        gbitmap_destroy(icon_bitmap);
        snprintf(time_delta_str, 12, "check(%d)", (int)t_delta-3);
        if(time_delta_layer)
            text_layer_set_text(time_delta_layer, time_delta_str);

        icon_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_REFRESH_WHITE);
        if(icon_layer)
            bitmap_layer_set_bitmap(icon_layer, icon_bitmap);
    }
    
        DictionaryIterator *iter;
        app_message_outbox_begin(&iter);
    
        if (iter == NULL) {
            return;
        }
    
        static char *idptr = data_id;

        Tuplet idval = TupletCString(5, idptr);
    
        dict_write_tuplet(iter, &idval);
        dict_write_end(iter);
        APP_LOG(APP_LOG_LEVEL_INFO, "Message sent!");
        APP_LOG(APP_LOG_LEVEL_INFO, "t_delta: %d", t_delta);

        app_message_outbox_send(); 
}
/*************Startup Timer*******/
//Message SHOULD come from smartphone app, but this will kick it off in less than 60 seconds if it can.
static void timer_callback(void *data) {   
    send_cmd(); 
}

static void clock_refresh(struct tm * tick_time) {
     char *time_format;
 
    if (!tick_time) {
        time_t now = time(NULL);
        tick_time = localtime(&now);
    }
    
    if (clock_is_24h_style()) {
        time_format = "%H:%M  %B %e";
    } else {
        time_format = "%I:%M  %B %e";
    }
    
    strftime(time_text, sizeof(time_text), time_format, tick_time);
    
    if (time_text[0] == '0') {
        memmove(time_text, &time_text[1], sizeof(time_text) - 1);
    }
    
    if (s_canvas_layer) {
        layer_mark_dirty(s_canvas_layer);
        if(time_layer)
            text_layer_set_text(time_layer, time_text);
    }

    s_last_time.hours = tick_time->tm_hour;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "time hours 1: %d", s_last_time.hours);
    s_last_time.hours -= (s_last_time.hours > 12) ? 12 : 0;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "time hours 2: %d", s_last_time.hours);
    s_last_time.minutes = tick_time->tm_min;
    
}

void accel_tap_handler(AccelAxisType axis, int32_t direction) {
    
    if (axis == ACCEL_AXIS_X)
    {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "axis: %s", "X");
        //send_cmd();
    } else if (axis == ACCEL_AXIS_Y)
        
    {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "axis: %s", "Y");
        //send_cmd();
    } else if (axis == ACCEL_AXIS_Z)
    {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "axis: %s", "Z");
        //send_cmd();
    }
    
}
static void comm_alert() {
    
    VibePattern pat = {
        .durations = error,
        .num_segments = ARRAY_LENGTH(error),
    };     
        if (t_delta % 5 == 0) {
            vibes_enqueue_custom_pattern(pat);
        }
    
    b_color_channels[0] = 255;
    b_color_channels[1] = 0;
    b_color_channels[2] = 0;
   
    layer_mark_dirty(s_canvas_layer);

}

static void tick_handler(struct tm * tick_time, TimeUnits changed) {
    if (!has_launched) {                 
            snprintf(time_delta_str, 12, "load(%d)", t_delta-4);
            
            if (time_delta_layer) {
                text_layer_set_text(time_delta_layer, time_delta_str);
            }
    } else 
    APP_LOG(APP_LOG_LEVEL_INFO, "t_delta_tt: %d", t_delta);
    if(t_delta > 4) {
        // accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
        // accel_tap_service_subscribe(accel_tap_handler);
        send_cmd();
        
    } else
    {   
        if (has_launched) {
            if (t_delta <= 0) {
                t_delta = 0;
                snprintf(time_delta_str, 12, "now"); // puts string into buffer
            } else {
                snprintf(time_delta_str, 12, "%d min", t_delta); // puts string into buffer
            }
            if (time_delta_layer) {
                text_layer_set_text(time_delta_layer, time_delta_str);
            }
        } else {

        }
    }
    t_delta++;  
    clock_refresh(tick_time);
    
}
static void update_proc(Layer * layer, GContext * ctx) {

#ifdef PBL_PLATFORM_BASALT

    graphics_context_set_fill_color(ctx, GColorFromRGB(b_color_channels[0], b_color_channels[1], b_color_channels[2]));
    graphics_fill_rect(ctx, GRect(0, 0, 144, 168), 0, GCornerNone);
    
    graphics_context_set_stroke_color(ctx, GColorBlack); 
    graphics_context_set_antialiased(ctx, ANTIALIASING);
    
    graphics_context_set_fill_color(ctx, GColorFromRGB(s_color_channels[0], s_color_channels[1], s_color_channels[2]));
    graphics_fill_rect(ctx, GRect(0, 24, 144, 74), 4, GCornersAll);
    graphics_context_set_stroke_color(ctx, GColorWhite); 
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_round_rect(ctx, GRect(0, 24, 144, 74), 4);
    
    if(chart_layer)
        chart_layer_set_canvas_color(chart_layer, GColorFromRGB(b_color_channels[0], b_color_channels[1], b_color_channels[2]));
#elif PBL_PLATFORM_CHALK

    graphics_context_set_fill_color(ctx, GColorFromRGB(b_color_channels[0], b_color_channels[1], b_color_channels[2]));
    graphics_fill_rect(ctx, GRect(0, 0, 180, 180), 0, GCornerNone);
    
    graphics_context_set_stroke_color(ctx, GColorBlack); 
    graphics_context_set_antialiased(ctx, ANTIALIASING);
    
    graphics_context_set_fill_color(ctx, GColorFromRGB(s_color_channels[0], s_color_channels[1], s_color_channels[2]));
    graphics_fill_rect(ctx, GRect(0, 40, 180, 46), 4, GCornersAll);
    graphics_context_set_stroke_color(ctx, GColorWhite); 
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_round_rect(ctx, GRect(0, 40, 180, 46), 4);
	graphics_draw_round_rect(ctx, GRect(0, 137, 180, 70), 4);
    
    if(chart_layer)
        chart_layer_set_canvas_color(chart_layer, GColorFromRGB(b_color_channels[0], b_color_channels[1], b_color_channels[2]));
    
#else
    //Change Background
    if (b_color_channels[0] == 170) {
        //OKAY
        graphics_context_set_fill_color(ctx, GColorWhite);
        graphics_fill_rect(ctx, GRect(0, 0, 144, 168), 0, GCornerNone);
        if(time_layer)
            text_layer_set_text_color(time_layer, GColorBlack);
    } else {
        //BAD COMMS
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_rect(ctx, GRect(0, 0, 144, 168), 0, GCornerNone);
        if(time_layer)
            text_layer_set_text_color(time_layer, GColorWhite);
    }
    // Change clockface
    if (s_color_channels[0] < 255){
        //OKAY
        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_context_set_fill_color(ctx, GColorClear);
        
        if (bg_layer)
            text_layer_set_text_color(bg_layer, GColorBlack);      
        if (delta_layer)
            text_layer_set_text_color(delta_layer, GColorBlack);      
        if (time_delta_layer)
            text_layer_set_text_color(time_delta_layer, GColorBlack);      
        if(icon_layer)
            bitmap_layer_set_compositing_mode(icon_layer, GCompOpClear);
        
    } else {
        //NOT OKAY
        graphics_context_set_stroke_color(ctx, GColorWhite);
        graphics_context_set_fill_color(ctx, GColorBlack);
        
        if (bg_layer)
            text_layer_set_text_color(bg_layer, GColorWhite);
        if (delta_layer)
            text_layer_set_text_color(delta_layer, GColorWhite);
        if (time_delta_layer)
            text_layer_set_text_color(time_delta_layer, GColorWhite);
        if(icon_layer)
            bitmap_layer_set_compositing_mode(icon_layer, GCompOpOr);
    }
    graphics_fill_rect(ctx, GRect(0, 24, 144, 74), 0, GCornerNone);

#endif
    
}

static void reset_background() {
    
    b_color_channels[0] = 0;
    b_color_channels[1] = 0;
    b_color_channels[2] = 0;
    if(time_layer)
        text_layer_set_text_color(time_layer, GColorWhite);
    if(chart_layer)
        chart_layer_set_canvas_color(chart_layer, GColorBlack);
    layer_mark_dirty(s_canvas_layer);
}

static void process_alert() {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Vibe State: %i", vibe_state);
    switch (alert_state) {
        
    case LOSS_MID_NO_NOISE:;    
        s_color_channels[0] = 255;
        s_color_channels[1] = 255;
        s_color_channels[2] = 0;
        
        if (vibe_state > 0)
            vibes_long_pulse();
            
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Alert key: %i", LOSS_MID_NO_NOISE);
#if defined(PBL_COLOR)
        text_layer_set_text_color(bg_layer, GColorBlack);
#ifdef PBL_PLATFORM_CHALK 
        if(delta_layer)       
            text_layer_set_text_color(delta_layer, GColorWhite);
        if(time_delta_layer)
            text_layer_set_text_color(time_delta_layer, GColorWhite);
#else 
        if(delta_layer)
            text_layer_set_text_color(delta_layer, GColorBlack);
        if(time_delta_layer)
            text_layer_set_text_color(time_delta_layer, GColorBlack);
#endif
        bitmap_layer_set_compositing_mode(icon_layer, GCompOpClear);
#elif defined(PBL_BW)
        if(bg_layer)
            text_layer_set_text_color(bg_layer, GColorWhite);
        if(delta_layer)
            text_layer_set_text_color(delta_layer, GColorWhite);
        if(time_delta_layer)
            text_layer_set_text_color(time_delta_layer, GColorWhite);
        if(icon_layer)
            bitmap_layer_set_compositing_mode(icon_layer, GCompOpOr);
#endif
                     
        break;
        
    case LOSS_HIGH_NO_NOISE:;
        s_color_channels[0] = 255;
        s_color_channels[1] = 0;
        s_color_channels[2] = 0;
        
       if (vibe_state > 0)
            vibes_long_pulse();
            
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Alert key: %i", LOSS_HIGH_NO_NOISE);
        if(bg_layer)
            text_layer_set_text_color(bg_layer, GColorWhite);
        if(delta_layer)
            text_layer_set_text_color(delta_layer, GColorWhite);
        if(time_delta_layer)
            text_layer_set_text_color(time_delta_layer, GColorWhite);
        if(icon_layer)
            bitmap_layer_set_compositing_mode(icon_layer, GCompOpOr);
        break;
        
    case OKAY:;
        s_color_channels[0] = 0;
        s_color_channels[1] = 255;
        s_color_channels[2] = 0;
        
        if (vibe_state > 1)
            vibes_double_pulse();
            
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Alert key: %i", OKAY);
        if(bg_layer)
            text_layer_set_text_color(bg_layer, GColorBlack);
#ifdef PBL_PLATFORM_CHALK
        if(delta_layer)
            text_layer_set_text_color(delta_layer, GColorWhite);
        if(time_delta_layer)
            text_layer_set_text_color(time_delta_layer, GColorWhite);
#else   
        if(delta_layer)   
            text_layer_set_text_color(delta_layer, GColorBlack);
        if(time_delta_layer)
            text_layer_set_text_color(time_delta_layer, GColorBlack);
#endif
        if(icon_layer)
            bitmap_layer_set_compositing_mode(icon_layer, GCompOpClear);
        break;
        
    case OLD_DATA:;
        VibePattern pat = {
            .durations = error,
            .num_segments = ARRAY_LENGTH(error),
        };
        comm_alert();
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Alert key: %i", OLD_DATA);
        
        s_color_channels[0] = 0;
        s_color_channels[1] = 0;
        s_color_channels[2] = 255;
        
        if (time_delta_layer)
            text_layer_set_text_color(time_delta_layer, GColorWhite);
        if (icon_layer)
            bitmap_layer_set_compositing_mode(icon_layer, GCompOpOr);
        if (bg_layer)
            text_layer_set_text_color(bg_layer, GColorWhite);
        if (delta_layer)    
            text_layer_set_text_color(delta_layer, GColorWhite);
        break;
     
     case NO_CHANGE:;
        break;   
    }
        
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
    app_timer_cancel(timer);
    APP_LOG(APP_LOG_LEVEL_INFO, "Message received!");
    if(time_delta_layer)
        text_layer_set_text(time_delta_layer, "in...");
        
    // Get the first pair
    Tuple *new_tuple = dict_read_first(iterator);
    reset_background();
    CgmData* cgm_data = cgm_data_create(1,2,"3m","199","+3mg/dL","Evan");

    // Process all pairs present
    while(new_tuple != NULL) {
        // Process this pair's key      
        switch (new_tuple->key) {
                
            case CGM_ID:;
                strncpy(data_id, new_tuple->value->cstring, 255);
                break;
                
            case CGM_EGV_DELTA_KEY:;
                if(delta_layer)
                    text_layer_set_text(delta_layer, new_tuple->value->cstring);
                break;
                
            case CGM_EGV_KEY:;
                cgm_data_set_egv(cgm_data, new_tuple->value->cstring);

                if(bg_layer)
                    text_layer_set_text(bg_layer, cgm_data_get_egv(cgm_data));

                strncpy(last_bg, new_tuple->value->cstring, 124);
                break;
                
            case CGM_TREND_KEY:;
                if (icon_bitmap) {
                    gbitmap_destroy(icon_bitmap);
                }
                icon_bitmap = gbitmap_create_with_resource(CGM_ICONS[new_tuple->value->uint8]);
                if(icon_layer)
                    bitmap_layer_set_bitmap(icon_layer, icon_bitmap);
                break;
                
            case CGM_ALERT_KEY:;
                alert_state = new_tuple->value->uint8;
                break;
                
            case CGM_VIBE_KEY:
                vibe_state = new_tuple->value->int16;             
                break;
                
            case CGM_TIME_DELTA_KEY:;
                int t_delta_temp = new_tuple->value->int16;
                
                if (t_delta_temp < 0) {
                    t_delta = t_delta;
                }else {
                    t_delta = t_delta_temp;
                }
                
                if (t_delta <= 0) {
                t_delta = 0;
                    snprintf(time_delta_str, 12, "now"); // puts string into buffer
                } else {
                    snprintf(time_delta_str, 12, "%d min", t_delta); // puts string into buffer
                }
                if (time_delta_layer) {
                    text_layer_set_text(time_delta_layer, time_delta_str);
                }
                break;
            case CGM_BGS:;
                ProcessingState* state = data_processor_create(new_tuple->value->cstring, ',');
                uint8_t num_strings = data_processor_count(state);
                APP_LOG(APP_LOG_LEVEL_DEBUG, "BG num: %i", num_strings);
                bgs = (int*)malloc((num_strings-1)*sizeof(int));     
                for (uint8_t n = 0; n < num_strings; n += 1) {
                    if (n == 0) {
                        tag_raw = data_processor_get_int(state);
                        APP_LOG(APP_LOG_LEVEL_DEBUG, "Tag Raw : %i", tag_raw);
                    }
                    else {
                        bgs[n-1] = data_processor_get_int(state);
                        APP_LOG(APP_LOG_LEVEL_DEBUG, "BG Split: %i", bgs[n-1]);
                    }     
                }
                num_bgs = num_strings - 1;			
                break;
            case CGM_BG_TIMES:;
                ProcessingState* state_t = data_processor_create(new_tuple->value->cstring, ',');
                uint8_t num_strings_t = data_processor_count(state_t);
                APP_LOG(APP_LOG_LEVEL_DEBUG, "BG_t num: %i", num_strings_t);
                bg_times = (int*)malloc(num_strings_t*sizeof(int));
                for (uint8_t n = 0; n < num_strings_t; n += 1) {
                    bg_times[n] = data_processor_get_int(state_t);
                    APP_LOG(APP_LOG_LEVEL_DEBUG, "BG_t Split: %i", bg_times[n]);
                }			
                break;
        }
        
        // Get next pair, if any
        new_tuple = dict_read_next(iterator);
    }
    

    // Redraw
    if (s_canvas_layer) {
        //load_chart_1();
        time_t t = time(NULL);
        struct tm * time_now = localtime(&t);
        clock_refresh(time_now);
        layer_mark_dirty(s_canvas_layer);
        if (chart_layer) {
            chart_layer_set_canvas_color(chart_layer, GColorBlack);
            chart_layer_set_margin(chart_layer, 7);
            chart_layer_set_data(chart_layer, bg_times, eINT, bgs, eINT, num_bgs);
        }
    }
    //Process Alerts
    process_alert();
    accel_tap_service_unsubscribe();
    has_launched = 1;

}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
    s_color_channels[0] = 0;
    s_color_channels[1] = 0;
    s_color_channels[2] = 255;
        
    if (time_delta_layer)
        text_layer_set_text_color(time_delta_layer, GColorWhite);
    if (icon_layer)
        bitmap_layer_set_compositing_mode(icon_layer, GCompOpOr);
    if (bg_layer)
        text_layer_set_text_color(bg_layer, GColorWhite);
    if (delta_layer)    
        text_layer_set_text_color(delta_layer, GColorWhite);
    
    snprintf(time_delta_str, 12, "in-err(%d)", t_delta);
    if(bg_layer)
        text_layer_set_text(bg_layer, translate_error(reason));
        
    if(time_delta_layer)
        text_layer_set_text(time_delta_layer, time_delta_str);
        
    comm_alert();

}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
    s_color_channels[0] = 0;
    s_color_channels[1] = 0;
    s_color_channels[2] = 255;
        
    if (time_delta_layer)
        text_layer_set_text_color(time_delta_layer, GColorWhite);
    if (icon_layer)
        bitmap_layer_set_compositing_mode(icon_layer, GCompOpOr);
    if (bg_layer)
        text_layer_set_text_color(bg_layer, GColorWhite);
    if (delta_layer)    
        text_layer_set_text_color(delta_layer, GColorWhite);
        
    snprintf(time_delta_str, 12, "out-err(%d)", t_delta);
    
    if(bg_layer)
        text_layer_set_text(bg_layer, translate_error(reason));
    
    if(time_delta_layer)
        text_layer_set_text(time_delta_layer, time_delta_str);
    comm_alert();

}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
    APP_LOG(APP_LOG_LEVEL_INFO, "out sent callback");
}

static void window_load(Window * window) {
    
    Layer * window_layer = window_get_root_layer(window);
    GRect window_bounds = layer_get_bounds(window_layer);
    
    int offset = 24/2;
    GRect inner_bounds = GRect(0, 24, 144, 144);
    s_center = grect_center_point(&inner_bounds);

    s_canvas_layer = layer_create(window_bounds);
    layer_set_update_proc(s_canvas_layer, update_proc);
    layer_add_child(window_layer, s_canvas_layer);
    
#ifdef PBL_PLATFORM_CHALK   
    icon_layer = bitmap_layer_create(GRect(106 + 18, 36+offset, 30, 30));
    bg_layer = text_layer_create(GRect(4 + 18, 19+offset, 100, 75));
	delta_layer = text_layer_create(GRect(0, 136, 180, 25));
    time_delta_layer = text_layer_create(GRect(0, 156, 180, 25));
    time_layer = text_layer_create(GRect(40, 0, 100, 40));  
    chart_layer = chart_layer_create((GRect) { 
      	.origin = { 18, 89},
		.size = { 146, 45 } });
    text_layer_set_text_alignment(time_delta_layer, GTextAlignmentCenter); 
    text_layer_set_text_alignment(delta_layer, GTextAlignmentCenter);
#else 
    icon_layer = bitmap_layer_create(GRect(106, 34+offset, 30, 30));
    bg_layer = text_layer_create(GRect(8, 17+offset, 100, 75));
    delta_layer = text_layer_create(GRect(4, 74, 136, 25));
    time_delta_layer = text_layer_create(GRect(4, 21, 136, 25));
    time_layer = text_layer_create(GRect(0, 0, 144, 20));
    chart_layer = chart_layer_create((GRect) { 
      	.origin = { 4, 102},
		.size = { 136, 62 } });
    text_layer_set_text_alignment(time_delta_layer, GTextAlignmentRight); 
    text_layer_set_text_alignment(delta_layer, GTextAlignmentRight);  
 #endif  
    bitmap_layer_set_background_color(icon_layer, GColorClear);
    bitmap_layer_set_compositing_mode(icon_layer, GCompOpClear);
    layer_add_child(s_canvas_layer, bitmap_layer_get_layer(icon_layer));
 
    text_layer_set_text_color(bg_layer, GColorBlack);
    text_layer_set_background_color(bg_layer, GColorClear);
    text_layer_set_font(bg_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_HN_BOLD_48)));
    text_layer_set_text_alignment(bg_layer, GTextAlignmentLeft);
    layer_add_child(s_canvas_layer, text_layer_get_layer(bg_layer)); 
 
    text_layer_set_text_color(delta_layer, GColorBlack);
    text_layer_set_background_color(delta_layer, GColorClear);
    text_layer_set_font(delta_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    layer_add_child(s_canvas_layer, text_layer_get_layer(delta_layer)); 
 
    text_layer_set_text_color(time_delta_layer, GColorWhite);
    text_layer_set_background_color(time_delta_layer, GColorClear);
    text_layer_set_font(time_delta_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    layer_add_child(s_canvas_layer, text_layer_get_layer(time_delta_layer)); 
 
    text_layer_set_text_color(time_layer, GColorWhite);
    text_layer_set_background_color(time_layer, GColorClear);
    text_layer_set_font(time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD)); 
 
    text_layer_set_text_alignment(time_layer, GTextAlignmentCenter);
    layer_add_child(s_canvas_layer, text_layer_get_layer(time_layer));
     
  	chart_layer_set_plot_color(chart_layer, GColorWhite);
  	chart_layer_set_canvas_color(chart_layer, GColorBlack);
  	chart_layer_show_points_on_line(chart_layer, true);
	chart_layer_animate(chart_layer, false);
  	layer_add_child(window_layer, chart_layer_get_layer(chart_layer));

}

static void window_unload(Window * window) {
    layer_destroy(s_canvas_layer);
}

/*********************************** App **************************************/
static void init() {

    time_t t = time(NULL);
    struct tm * time_now = localtime(&t);
    tick_handler(time_now, MINUTE_UNIT);
    
    s_main_window = window_create();
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
    });
    window_stack_push(s_main_window, true);
    
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    
    accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
    accel_tap_service_subscribe(accel_tap_handler);
    
       
    // Registering callbacks
    app_message_register_inbox_received(inbox_received_callback);
    app_message_register_inbox_dropped(inbox_dropped_callback);
    app_message_register_outbox_failed(outbox_failed_callback);
    app_message_register_outbox_sent(outbox_sent_callback);
    app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
    
    timer = app_timer_register(10000, timer_callback, NULL);

}

static void deinit() {
    window_destroy(s_main_window);
    persist_write_string(0, data_id);
}

int main() {
    init();
    app_event_loop();
    deinit();
}

#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "http.h"
#include "pebcastRoulette.h"

//#define MY_UUID { 	0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0x10, 0x34, 0xBF, 0xBE, 0x12, 0x98}
#define MY_UUID { 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0x04, 0x9F, 0x49, 0xC0, 0x99, 0xAD }
	
PBL_APP_INFO(MY_UUID, "PebCast", "Mano Samy", 1,0, /* App version */
				   RESOURCE_ID_PEBCAST_ICON,APP_INFO_STANDARD_APP);

void handle_init(AppContextRef ctx);
void window_appear(Window* me);

//Maximum dimensions
static int WIDTH = 144;

static int pollFrequencyMins = 30;
static int pollOffsetMins = 0;
static bool rouletteInError = false;
static char rouletteStatus[4];
Window window;

int firstrun = 1; // Variable to check for first run. 1 - YES. 0 - NO.

TextLayer layer_errIcon;
TextLayer layer_text2;
ScrollLayer sLayer;

TextLayer text_day_layer;
TextLayer text_time_layer;
Layer line_layer;

void line_layer_update_callback(Layer *me, GContext* ctx) {
    (void)me;
    
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_line(ctx, GPoint(6, 32), GPoint(144-12, 32));
}


void repaintStatus(int minsSinceRefresh) {
	int dotsCount = 1 + (minsSinceRefresh / (pollFrequencyMins/3));
	memset(rouletteStatus, 0, 4);
	if(rouletteInError)
		memset(rouletteStatus, '!',dotsCount > 3 ? 3 : dotsCount);
	else
		memset(rouletteStatus, '.',dotsCount > 3 ? 3 : dotsCount);
	text_layer_set_text(&layer_errIcon,  rouletteStatus);
}

void repaintMessage(char* msg) {
    text_layer_set_text(&layer_text2, msg);

		//Get size used by TextLayer
    GSize max_size = text_layer_get_max_used_size(app_get_current_graphics_context(), &layer_text2);
   max_size.h += 300;
    //Use TextLayer size
    text_layer_set_size(&layer_text2, max_size);
   
    //Use TextLayer size for ScrollLayer - this has to be done manually for now!
    scroll_layer_set_content_size(&sLayer, GSize(WIDTH, max_size.h));
	scroll_layer_set_content_offset(&sLayer, GPointZero, true);
}



void roulette_Error(int failCount, char* msg) {
	rouletteInError = true;
	repaintStatus(0);
}

void roulette_initComplete() {
		repaintMessage(pebcast_roulette_nextMessage());
		if(pebcast_roulette_should_vibe())
			vibes_short_pulse();
}

void handle_init(AppContextRef ctx) {
  (void)ctx;
	resource_init_current_app(&APP_RESOURCES);
  	
  window_set_fullscreen(&window, true);
  window_init(&window, "PebCast");

  window_stack_push(&window, true /* Animated */);
  window_set_window_handlers(&window, (WindowHandlers){
    .appear  = window_appear
  });
  window_set_background_color(&window, GColorBlack);

		
	// Day
    text_layer_init(&text_day_layer, window.layer.frame);
    text_layer_set_text_color(&text_day_layer, GColorWhite);
    text_layer_set_background_color(&text_day_layer, GColorClear);
    layer_set_frame(&text_day_layer.layer, GRect(6, 0, 144-20, 30));
    text_layer_set_font(&text_day_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    text_layer_set_text_alignment(&text_day_layer, GTextAlignmentLeft);
    layer_add_child(&window.layer, &text_day_layer.layer);
    
    // Hour and Minute
    text_layer_init(&text_time_layer, window.layer.frame);
    text_layer_set_text_color(&text_time_layer, GColorWhite);
    text_layer_set_background_color(&text_time_layer, GColorClear);
    layer_set_frame(&text_time_layer.layer, GRect(6, 25, 144-6, 45));
    text_layer_set_font(&text_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
    text_layer_set_text_alignment(&text_time_layer, GTextAlignmentLeft);
    layer_add_child(&window.layer, &text_time_layer.layer);
	
	// line
    layer_init(&line_layer, window.layer.frame);
    line_layer.update_proc = &line_layer_update_callback;
    layer_add_child(&window.layer, &line_layer);
	
	//Init ScrollLayer and attach button click provider
    scroll_layer_init(&sLayer, GRect(0, 70, 144, 168 - 70));
    scroll_layer_set_click_config_onto_window(&sLayer, &window);
	
  text_layer_init(&layer_text2, GRect(0, -5, 140, 250));
  text_layer_set_text_color(&layer_text2, GColorWhite);
  text_layer_set_background_color(&layer_text2, GColorClear);
  text_layer_set_font(&layer_text2, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_overflow_mode(&layer_text2, GTextOverflowModeWordWrap);
//  repaintMessage("Preparing to cast...");	
 
		
		
	// ErrorIcon
    text_layer_init(&layer_errIcon, window.layer.frame);
    text_layer_set_text_color(&layer_errIcon, GColorWhite);
	text_layer_set_text(&layer_errIcon,  "!");
    text_layer_set_background_color(&layer_errIcon, GColorClear);
    layer_set_frame(&layer_errIcon.layer, GRect(144-18, 10, 144-6, 30));
    text_layer_set_font(&layer_errIcon, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(&layer_errIcon, GTextAlignmentLeft);
    layer_add_child(&window.layer, &layer_errIcon.layer);
     
    //Add TextLayer to ScrollLayer to Window
    scroll_layer_add_child(&sLayer, &layer_text2.layer);
    layer_add_child(&window.layer, (Layer*)&sLayer);

	pebcast_roulette_register_callbacks((PebcastRouletteCallbacks) {
    	.onInitComplete = roulette_initComplete,
		.onError = roulette_Error,
    });
	pebcast_roulette_init(PC_ROULETTE_ALL_MESSAGES_SORTED);
	
}



void handle_tick(AppContextRef ctx, PebbleTickEvent *t) {
    
    (void)ctx;
    
    // Get Time From Pebble To Check Which Layers To Update
    PblTm pebble_time;
    get_time(&pebble_time);
    
    // Update Every Minute
    if (pebble_time.tm_sec == 0 || firstrun == 1) {
        
        static char time_text[] = "00:00"; // Need to be static because they're used by the system later.
        
        char *time_format;
        
        // Hour & Minute Formatting Type
        if (clock_is_24h_style()) {
            time_format = "%R";
        } else {
            time_format = "%I:%M";
        }
        
        // Hour & Minute Formatting and Update
        string_format_time(time_text, sizeof(time_text), time_format, t->tick_time);
        
        // Kludge to handle lack of non-padded hour format string
        // for twelve hour clock.
        if (!clock_is_24h_style() && (time_text[0] == '0')) {
            memmove(time_text, &time_text[1], sizeof(time_text) - 1);
        }
        
        text_layer_set_text(&text_time_layer, time_text);
		
		repaintMessage(pebcast_roulette_nextMessage());
		if(pebcast_roulette_should_vibe())
			vibes_short_pulse();
		
		if(pebcast_roulette_failureCount() ==0)
			rouletteInError = false;
		repaintStatus((pollFrequencyMins + pebble_time.tm_min - pollOffsetMins) % pollFrequencyMins);
    }
   // Update Every Hour
    if (pebble_time.tm_min == 0 || firstrun == 1) {
        
        static char day_text[] = "XXX, W00 00"; // Need to be static because they're used by the system later.
 
        // Day Formatting and Update
        string_format_time(day_text, sizeof(day_text), "%a, %b %e", t->tick_time);
        text_layer_set_text(&text_day_layer, day_text);
        

    }
	if(pollOffsetMins == 0)
		pollOffsetMins = pebble_time.tm_min %pollFrequencyMins;
	//Its impolite to ping exactly at the top/bottom of the hour, too much load on the server
    if ((pebble_time.tm_min % pollFrequencyMins) - pollOffsetMins == 0 || firstrun == 1) {
        pebcast_roulette_poll();  
    }
    
    // Set firstrun to 0
    if (firstrun == 1) {
        firstrun = 0;
    }
}

void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
	.messaging_info = {
      .buffer_sizes = {
        .inbound = 124,
        .outbound = 256,
      },
	},
   .tick_info = {
      .tick_handler = &handle_tick,
      .tick_units = MINUTE_UNIT
      }  
  };
  app_event_loop(params, &handlers);
}

void window_appear(Window* me) {
// pollPebCast(1);
}


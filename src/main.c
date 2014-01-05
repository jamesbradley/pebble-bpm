#include "pebble.h"
#include "settings.h"
#include "main.h"

//Layers
static Window *window;
static TextLayer *offset_text_layer;
static TextLayer *single_mode_text_layer;
static TextLayer *dual_mode_standby_text_layer;
static InverterLayer *single_mode_inverter_layer;
static InverterLayer *dual_mode_standby_inverter_layer;
static GRect bounds;
static char single_mode_bpm_text[10];
static char string_offset[7];

static BpmTimer primary_bpm_timer;
static BpmTimer secondary_bpm_timer;
static BpmTimer *active_bpm_timer;
static bool is_dual_mode_active = false;

static BpmSettings* settings;

static GBitmap* button_image_up;
static GBitmap* button_image_down;
static GBitmap* button_image_middle;
static ActionBarLayer *action_bar;


//50ms burst vibration pattern
static const uint32_t const segments[] = { 50 };
static VibePattern pat = { .durations = segments, .num_segments = ARRAY_LENGTH(segments) };



unsigned long time_in_ms() {
        time_t seconds;
        uint16_t milliseconds;
        time_ms(&seconds, &milliseconds);
        return ((unsigned long)(seconds*1000)) + (unsigned long)milliseconds;
}

static void flash_timer_callback(void *data) {
	
	BpmTimer timer = *((BpmTimer*)data);
	layer_set_hidden((Layer *)timer.inverter_layer, false);
	
	if (settings->light)
		light_enable(false);
}

static void stop_all_active() {
	
	app_timer_cancel(primary_bpm_timer.tapTimer);
	app_timer_cancel(secondary_bpm_timer.tapTimer);
	
	primary_bpm_timer.number_of_taps = 0;
	secondary_bpm_timer.number_of_taps = 0;
	is_dual_mode_active = false;
	
	text_layer_set_text(single_mode_text_layer, "TAP\nME");
	layer_set_hidden((Layer *)single_mode_text_layer, false);
	layer_set_hidden((Layer *)single_mode_inverter_layer, false);
	
	layer_set_hidden((Layer *)dual_mode_standby_text_layer, true);
	layer_set_hidden((Layer *)dual_mode_standby_inverter_layer, true);
	
	layer_set_hidden((Layer *)secondary_bpm_timer.text_layer, true);
	layer_set_hidden((Layer *)secondary_bpm_timer.inverter_layer, true);
	
	layer_set_hidden((Layer *)offset_text_layer, true);
	
	primary_bpm_timer.text_layer = single_mode_text_layer;		
	primary_bpm_timer.inverter_layer = single_mode_inverter_layer;
}
static void show_beat_flash(BpmTimer *timer) {
	
	layer_set_hidden((Layer *)timer->inverter_layer, true);
	timer->flashOffTimer = app_timer_register(100, flash_timer_callback, timer);
	
	//Dont flash the light or vibrate if this is a non-active timer in dual mode
	if (is_dual_mode_active && timer != active_bpm_timer)
		return;
	
	if (settings->light)
		light_enable(true);
	
	if (settings->vibrate)
		vibes_enqueue_custom_pattern(pat);
}
static void tap_timer_callback(void *data) {

	BpmTimer *timer = ((BpmTimer *)data);
	app_timer_cancel(timer->tapTimer);
	
	if (timer->average_tap_gap > 0)
		timer->tapTimer = app_timer_register(timer->average_tap_gap, tap_timer_callback, timer);

	//APP_LOG(APP_LOG_LEVEL_DEBUG, "(Callback) average tap time:%lu", timer->average_tap_gap);
	
	show_beat_flash(timer);
}

static void update_tap_gap_offset() {
	
	float bigger  = primary_bpm_timer.average_tap_gap > secondary_bpm_timer.average_tap_gap ? primary_bpm_timer.average_tap_gap : secondary_bpm_timer.average_tap_gap;
	float smaller = primary_bpm_timer.average_tap_gap > secondary_bpm_timer.average_tap_gap ? secondary_bpm_timer.average_tap_gap : primary_bpm_timer.average_tap_gap;

	float tap_gap_offset = (( bigger / smaller )*100.0)-100.0;
	
	int decimal_place = (tap_gap_offset - ((int)tap_gap_offset))*10;
	snprintf(string_offset, 10, "%d.%d%%", (int)tap_gap_offset, decimal_place);
	text_layer_set_text(offset_text_layer, string_offset);
		
}

static void process_tap(BpmTimer *timer) {

	//Capture the time first so we can subract any time processing this function from the time-to-next-beat
	unsigned long tap_start_time =  time_in_ms();
	
	//If we've more than 8 samples, push one off the end of the queue so we have the latest 8
	if (timer->number_of_taps == MAX_NUMBER_OF_SAMPLES) {
		for(int i = 1; i < timer->number_of_taps; i++) {
			timer->taps[i-1] = timer->taps[i];
		}
	}
	else
	{
		timer->number_of_taps++;
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "tap no:%d", timer->number_of_taps);
	}
	
	timer->taps[timer->number_of_taps-1] = tap_start_time;
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "current time:%lu tap no:%d", timer->taps[timer->number_of_taps-1], timer->number_of_taps);
	
	//Work out if we need to start a new timer, by calculating if a different tap pattern has started
	if (timer->average_tap_gap != 0 && timer->number_of_taps) {
		
		unsigned long latest_tap_gap = (timer->taps[timer->number_of_taps-1] - timer->taps[timer->number_of_taps-2]);
		float variance = ((float)timer->average_tap_gap)/latest_tap_gap;
		
		if (variance > 1.5 || variance < 0.5)
		{
			//APP_LOG(APP_LOG_LEVEL_DEBUG, "#RESET taps:%d", timer->number_of_taps);			
			//APP_LOG(APP_LOG_LEVEL_DEBUG, "latest:%lu earlier:%lu", latest_tap_gap, timer->average_tap_gap);			
			//APP_LOG(APP_LOG_LEVEL_DEBUG, "varience:%d", (int)variance*100);
			
			timer->taps [0] = timer->taps[timer->number_of_taps-1];
			timer->number_of_taps = 1;
			timer->average_tap_gap = 0;
			text_layer_set_text(timer->text_layer, "again");
		}
	}
	
	//If we have enough taps (2) to calculate a gap between them
	if (timer->number_of_taps >= 2) {

		float beats_per_minute = 0;
		
		//Calculate an average time between taps
		unsigned long total_tap_gap = 0;
		for (int i = 1; i < timer->number_of_taps; i++)
		{
			unsigned long latest_tap_gap = timer->taps[i] - timer->taps[i-1];
			total_tap_gap += latest_tap_gap;
			//APP_LOG(APP_LOG_LEVEL_DEBUG, "sub %lu from %lu", timer->taps[i-1], timer->taps[i]);
			//APP_LOG(APP_LOG_LEVEL_DEBUG, "tap gap %d: %lu", i, latest_tap_gap);
		}
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "%lu / %d", total_tap_gap, (number_of_taps-1));
		timer->average_tap_gap = total_tap_gap/(timer->number_of_taps-1);
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "average tap time:%lu", timer->average_tap_gap);
		
		//Kick off the new timer ASAP
		app_timer_cancel(timer->tapTimer);
		timer->tapTimer = app_timer_register((uint32_t)timer->average_tap_gap - (time_in_ms() - tap_start_time), tap_timer_callback, timer);
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "now %lu then %lu", time_in_ms(), tap_start_time);
		
		//Fire this tap off as a beat, as it replaces whatever else might have fired shortly
		show_beat_flash(timer);
		
		beats_per_minute = 60.0*(1000.0/timer->average_tap_gap);
		
		//snprintf doesnt support floats, so do some dodgy maths to put it in a seperate int
		int decimal_place = (beats_per_minute - ((int)beats_per_minute))*10;
		
		snprintf(timer->string_bpm, 10, "%d.%d", (int)beats_per_minute, decimal_place);
		
		if (!is_dual_mode_active)
		{
			strcpy (single_mode_bpm_text,timer->string_bpm);
			strcat (single_mode_bpm_text,"\nBPM");
			text_layer_set_text(timer->text_layer, single_mode_bpm_text);
		}
		else
		{
			text_layer_set_text(timer->text_layer, timer->string_bpm);
			update_tap_gap_offset();
		}	
		
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "snprintf: %s", string_bpm);
	}
	
}

//Decide whethet this tap is inteded for the primary or secondary timer
static BpmTimer* get_timer(int calling_button) {
	
	bool returnPrimary = true;
	
	//First ever press - this is, dy definition, the primary layer
	if (primary_bpm_timer.number_of_taps == 0 && secondary_bpm_timer.number_of_taps == 0)
	{
		primary_bpm_timer.initialising_button = calling_button;
		
		//dual_mode_standby_text_layer = primary_bpm_timer.text_layer;
		//dual_mode_standby_inverter_layer = primary_bpm_timer.inverter_layer;

		primary_bpm_timer.text_layer = single_mode_text_layer;		
		primary_bpm_timer.inverter_layer = single_mode_inverter_layer;
	}
	
	if (primary_bpm_timer.initialising_button == calling_button)
		returnPrimary = true;
	else
		returnPrimary = false;
	
	//First press for the second timer
	if (!returnPrimary && secondary_bpm_timer.number_of_taps == 0)
	{
		is_dual_mode_active = true;
		secondary_bpm_timer.initialising_button = calling_button;
		
		primary_bpm_timer.text_layer = dual_mode_standby_text_layer;
		primary_bpm_timer.inverter_layer = dual_mode_standby_inverter_layer;
		
		//Set the bpm text of the dual mode layer to what the single mode layer had
		text_layer_set_text(primary_bpm_timer.text_layer, primary_bpm_timer.string_bpm);
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "dual mode active %s", primary_bpm_timer.string_bpm);
		
		layer_set_hidden((Layer *)single_mode_text_layer, true);
		layer_set_hidden((Layer *)primary_bpm_timer.text_layer, false);
		layer_set_hidden((Layer *)secondary_bpm_timer.text_layer, false);
		
		layer_set_hidden((Layer *)single_mode_inverter_layer, true);
		layer_set_hidden((Layer *)primary_bpm_timer.inverter_layer, false);
		layer_set_hidden((Layer *)secondary_bpm_timer.inverter_layer, false);
		
		layer_set_hidden((Layer *)offset_text_layer, false);
	}
	
	if (returnPrimary)
	{
		active_bpm_timer = &primary_bpm_timer;
		return &primary_bpm_timer;
	}
	//else
		active_bpm_timer = &secondary_bpm_timer;
		return &secondary_bpm_timer;
}

static void process_tap_btn_top(void *context) {
	
	process_tap(get_timer(BUTTON_ID_UP));
}

static void process_tap_btn_bottom(void *context) {
	
	process_tap(get_timer(BUTTON_ID_DOWN));
}

static void process_tap_btn_select(void *context) {
	
	//Stop all timers
	stop_all_active();
}

static void process_tap_btn_select_long(void *context) {
	
	display_settings();
}

static void process_tap_accel(AccelAxisType axis, int32_t direction) {
	
	if (settings->accelerometer) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "Accel:%lu", direction);
		//Hook up the accellerometer to come from the back button, so hitting UP or DOWN causes get_timer to fire up Dual Mode
		process_tap(get_timer(BUTTON_ID_BACK));
	}
}

void click_config_provider(void *context) {
	//Decide what will happen when you click the three buttons
	window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) process_tap_btn_bottom);
	window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) process_tap_btn_top);
	window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) process_tap_btn_select);
	window_long_click_subscribe(BUTTON_ID_SELECT, 1000, (ClickHandler) process_tap_btn_select_long, NULL);
}



static void init_action_bar() {
	
	// Initialize the action bar
	action_bar = action_bar_layer_create();
	action_bar_layer_add_to_window(action_bar, window);
	action_bar_layer_set_click_config_provider(action_bar, click_config_provider);
	
	//Set the actionbar icons
	button_image_up = gbitmap_create_with_resource(RESOURCE_ID_ICON_NOTE);
	button_image_middle = gbitmap_create_with_resource(RESOURCE_ID_ICON_STOP);
	button_image_down = gbitmap_create_with_resource(RESOURCE_ID_ICON_NOTE);
	action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, button_image_up);
	action_bar_layer_set_icon(action_bar, BUTTON_ID_SELECT, button_image_middle);
    action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, button_image_down);
 
}

static void init() {
	
	//Graba live copy of the savesd settings from the settings provider
	load_settings();
	settings = get_settings();
	
	//Only hook up the accelerometer if its on in settings, or it'll kill battery
	if (settings->accelerometer) {
		accel_service_set_sampling_rate(100);
		accel_tap_service_subscribe(&process_tap_accel);
	}
	
 	//Set up the window (animate it in)
	window = window_create();
	window_stack_push(window, true );
	Layer *window_layer = window_get_root_layer(window);
	bounds = layer_get_frame(window_layer);

	//Set up main BPM readout layer
	single_mode_text_layer = text_layer_create(GRect(0, 20, bounds.size.w-ACTION_BAR_WIDTH, bounds.size.h-20));
	text_layer_set_text(single_mode_text_layer, "TAP\nME");
	text_layer_set_font(single_mode_text_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));
	text_layer_set_text_alignment(single_mode_text_layer, GTextAlignmentCenter);
	layer_add_child(window_layer, text_layer_get_layer(single_mode_text_layer));
	
	//Set up the dual mode layers in advance (but hide them), so this doesn't cause a stall mid-count
	primary_bpm_timer.text_layer = text_layer_create(GRect(0, 0, bounds.size.w-ACTION_BAR_WIDTH-3, bounds.size.h/2));
	text_layer_set_text(primary_bpm_timer.text_layer, "0.0");
	text_layer_set_font(primary_bpm_timer.text_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));
	text_layer_set_text_alignment(primary_bpm_timer.text_layer, GTextAlignmentCenter);
	layer_set_hidden((Layer *)primary_bpm_timer.text_layer, true);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(primary_bpm_timer.text_layer));
	
	secondary_bpm_timer.text_layer = text_layer_create(GRect(0, (bounds.size.h/2)+20, bounds.size.w-ACTION_BAR_WIDTH-3, (bounds.size.h/2)-20 ));
	text_layer_set_text(secondary_bpm_timer.text_layer, "0.0");
	text_layer_set_font(secondary_bpm_timer.text_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));
	text_layer_set_text_alignment(secondary_bpm_timer.text_layer, GTextAlignmentCenter);
	layer_set_hidden((Layer *)secondary_bpm_timer.text_layer, true);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(secondary_bpm_timer.text_layer));
	
	offset_text_layer = text_layer_create(GRect(0, (bounds.size.h/2)-20, bounds.size.w-ACTION_BAR_WIDTH-3, 30 ));
	text_layer_set_text(offset_text_layer, "0.0");
	text_layer_set_font(offset_text_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
	text_layer_set_text_alignment(offset_text_layer, GTextAlignmentCenter);
	layer_set_hidden((Layer *)offset_text_layer, true);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(offset_text_layer));
	
	//Hook up the action bar
	init_action_bar();
	
	//Add the inverter
	single_mode_inverter_layer = inverter_layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
	layer_add_child(window_layer, inverter_layer_get_layer(single_mode_inverter_layer));
	
	primary_bpm_timer.inverter_layer = inverter_layer_create(GRect(0, 0, bounds.size.w-ACTION_BAR_WIDTH-3, (bounds.size.h/2)-15 ));
	layer_set_hidden((Layer *)primary_bpm_timer.inverter_layer, true);
	layer_add_child(window_layer, inverter_layer_get_layer(primary_bpm_timer.inverter_layer));
	
	secondary_bpm_timer.inverter_layer = inverter_layer_create(GRect(0, (bounds.size.h/2)+15, bounds.size.w-ACTION_BAR_WIDTH-3, (bounds.size.h/2)-15 ));
	layer_set_hidden((Layer *)secondary_bpm_timer.inverter_layer, true);
	layer_add_child(window_layer, inverter_layer_get_layer(secondary_bpm_timer.inverter_layer));
	
	dual_mode_standby_text_layer = primary_bpm_timer.text_layer;
	dual_mode_standby_inverter_layer = primary_bpm_timer.inverter_layer;
}

static void deinit() {
	
	inverter_layer_destroy(single_mode_inverter_layer);
	inverter_layer_destroy(primary_bpm_timer.inverter_layer);
	inverter_layer_destroy(secondary_bpm_timer.inverter_layer);
	
	text_layer_destroy(primary_bpm_timer.text_layer);
	text_layer_destroy(secondary_bpm_timer.text_layer);
	text_layer_destroy(single_mode_text_layer);
	
	window_destroy(window);
}

int main(void) {
	
	init();
	app_event_loop();
	deinit();
}

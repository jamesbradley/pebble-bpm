#pragma once
	
#define MAX_NUMBER_OF_SAMPLES 8
	
//Timers
typedef struct {
    int unsigned long taps[8];
	unsigned long     average_tap_gap;
    int               number_of_taps;
    AppTimer          *tapTimer;
	AppTimer          *flashOffTimer;
	TextLayer         *text_layer;
 	InverterLayer     *inverter_layer;
	int               initialising_button;
	char 			  string_bpm[10];
} BpmTimer;
	
static void process_tap_accel(AccelAxisType axis, int32_t direction);
static void process_tap_btn_top(void *context);
static void process_tap_btn_bottom(void *context);
static void tap_timer_callback(void *data);

void click_config_provider(void *context);
static void init_action_bar();

static void init();
static void deinit();
int main(void);
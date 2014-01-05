//
//  settings.h
//  


#ifndef SETTINGS_H
#define SETTINGS_H
	
#define MENU_ITEM_VIBRATE 0
#define MENU_ITEM_LIGHT 1
#define MENU_ITEM_ACCELEROMETER 2
#define MENU_ITEM_DUALMODE 3

static const uint32_t 	SETTING_SETTINGS_ALREADY_SAVED = 0;
static const uint32_t 	SETTING_VIBRATE = 1;
static const uint32_t 	SETTING_ACCELEROMETER = 2;
static const uint32_t 	SETTING_DUALMODE = 3;
static const uint32_t 	SETTING_LIGHT = 4;

typedef struct
{
    bool vibrate, accelerometer, dual_mode, light;
} BpmSettings;

void load_settings();
void save_settings();
void display_settings();
BpmSettings* get_settings();



#endif	
	

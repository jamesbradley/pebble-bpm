#include "pebble.h"
#include "settings.h"

static Window *window;
static SimpleMenuLayer *menu_layer;
static SimpleMenuSection menu_sections[2];
static SimpleMenuItem setting_items[4];
static SimpleMenuItem about_items[2];

GBitmap *menu_icon_os;
GBitmap *menu_icon_light;
GBitmap *menu_icon_accel;
GBitmap *menu_icon_vibrate;
GBitmap *menu_icon_dualmode;

BpmSettings _bpm_settings;


void save_settings () {

	persist_write_bool(SETTING_VIBRATE, _bpm_settings.vibrate);
	persist_write_bool(SETTING_ACCELEROMETER, _bpm_settings.accelerometer);
	persist_write_bool(SETTING_DUALMODE, _bpm_settings.dual_mode);
	persist_write_bool(SETTING_LIGHT, _bpm_settings.light);

	APP_LOG(APP_LOG_LEVEL_DEBUG, "SAVING %d", _bpm_settings.vibrate);
}

void settings_changed_callback(int index, void *context)
{
    
    switch (index) {
        case MENU_ITEM_VIBRATE:
            _bpm_settings.vibrate = !_bpm_settings.vibrate;
            setting_items[MENU_ITEM_VIBRATE].subtitle = (_bpm_settings.vibrate ? "On" : "Off");
            break;
		case MENU_ITEM_ACCELEROMETER:
            _bpm_settings.accelerometer = !_bpm_settings.accelerometer;
            setting_items[MENU_ITEM_ACCELEROMETER].subtitle = (_bpm_settings.accelerometer ? "On" : "Off");
            break;
		case MENU_ITEM_DUALMODE:
            _bpm_settings.dual_mode = !_bpm_settings.dual_mode;
            setting_items[MENU_ITEM_DUALMODE].subtitle = (_bpm_settings.dual_mode ? "On" : "Off");
            break;
		case MENU_ITEM_LIGHT:
            _bpm_settings.light = !_bpm_settings.light;
            setting_items[MENU_ITEM_LIGHT].subtitle = (_bpm_settings.light ? "On" : "Off");
            break;
        default:
            break;
    }
	
	save_settings();
    menu_layer_reload_data(simple_menu_layer_get_menu_layer(menu_layer));
}

void handle_appear(Window *window)
{
    //sroll_layer_set_frame(menu_layer.menu.scroll_layer, window->layer.bounds);
}

void handle_unload(Window *window)
{
	gbitmap_destroy(menu_icon_os);
    gbitmap_destroy(menu_icon_light);
    gbitmap_destroy(menu_icon_accel);
    gbitmap_destroy(menu_icon_vibrate);
    gbitmap_destroy(menu_icon_dualmode);
}

void init_settings_window()
{
	window = window_create();
    window_set_background_color(window, GColorWhite);
    window_set_window_handlers(window, (WindowHandlers) {
        .appear = (WindowHandler)handle_appear,
        .unload = (WindowHandler)handle_unload
    });
    
    // Don't forget to deinit
    menu_icon_os = gbitmap_create_with_resource(RESOURCE_ID_ICON_OS);
    menu_icon_light = gbitmap_create_with_resource(RESOURCE_ID_ICON_LIGHT);
    menu_icon_accel = gbitmap_create_with_resource(RESOURCE_ID_ICON_ACCEL);
    menu_icon_vibrate = gbitmap_create_with_resource(RESOURCE_ID_ICON_VIBRATE);
    menu_icon_dualmode = gbitmap_create_with_resource(RESOURCE_ID_ICON_DIUALMODE);
	
	// Section "Settings..."
    setting_items[MENU_ITEM_VIBRATE] = (SimpleMenuItem) {
        .title = "Vibrate",
        .icon = menu_icon_vibrate,
        .callback = settings_changed_callback
    };
	setting_items[MENU_ITEM_LIGHT] = (SimpleMenuItem) {
        .title = "Flash light",
        .icon = menu_icon_light,
        .callback = settings_changed_callback
    };
	setting_items[MENU_ITEM_ACCELEROMETER] = (SimpleMenuItem) {
        .title = "Tap mode",
        .icon = menu_icon_accel,
        .callback = settings_changed_callback
    };
	setting_items[MENU_ITEM_DUALMODE] = (SimpleMenuItem) {
        .title = "Dual mode",
        .icon = menu_icon_dualmode,
        .callback = settings_changed_callback
    };
    // Header
    menu_sections[0] = (SimpleMenuSection) {
        .title = "Settings",
        .items = setting_items,
        .num_items = ARRAY_LENGTH(setting_items)
    };
    
    // Section "About"
    about_items[0] = (SimpleMenuItem) {
        .title = "BPM v0.9",
        .subtitle = "(C)2014 James Bradley",
        .icon = NULL,
        .callback = NULL
    };
	about_items[1] = (SimpleMenuItem) {
        .title = "Source code",
        .subtitle = "git.io/uNtOTQ",
        .icon = menu_icon_os,
        .callback = NULL
    };
    // Header
    menu_sections[1] = (SimpleMenuSection) {
        .title = "About",
        .items = about_items,
        .num_items = ARRAY_LENGTH(about_items)
    };
    
	menu_layer = simple_menu_layer_create(layer_get_frame(window_get_root_layer(window)), window, menu_sections, 2, NULL);
    layer_add_child(window_get_root_layer(window), 
		scroll_layer_get_layer(menu_layer_get_scroll_layer(simple_menu_layer_get_menu_layer(menu_layer))) );
}


void load_settings () {

	_bpm_settings = (BpmSettings){
		.vibrate = true,
		.light = false,
		.accelerometer = false,
		.dual_mode = true,
	};
	
	if (persist_read_bool(SETTING_SETTINGS_ALREADY_SAVED)) {
		 
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "SETTINGS ALREADY SAVED %d", _bpm_settings.vibrate);
		_bpm_settings.vibrate = persist_read_bool(SETTING_VIBRATE);
		_bpm_settings.accelerometer = persist_read_bool(SETTING_ACCELEROMETER);
		_bpm_settings.dual_mode = persist_read_bool(SETTING_DUALMODE);
		_bpm_settings.light = persist_read_bool(SETTING_LIGHT);
	}	
	else {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "NO SETTINGS ALREADY SAVED flag:%d vib:%d", persist_read_bool(SETTING_SETTINGS_ALREADY_SAVED), _bpm_settings.vibrate);
		save_settings();
		persist_write_bool(SETTING_SETTINGS_ALREADY_SAVED, true);
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "After flag:%d vib:%d", persist_read_bool(SETTING_SETTINGS_ALREADY_SAVED), _bpm_settings.vibrate);
	}
}

void display_settings() {
	
    init_settings_window();
    
    setting_items[MENU_ITEM_VIBRATE].subtitle = (_bpm_settings.vibrate ? "On" : "Off");
	setting_items[MENU_ITEM_ACCELEROMETER].subtitle = (_bpm_settings.accelerometer ? "On" : "Off");
	setting_items[MENU_ITEM_DUALMODE].subtitle = (_bpm_settings.dual_mode ? "On" : "Off");
	setting_items[MENU_ITEM_LIGHT].subtitle = (_bpm_settings.light ? "On" : "Off");
    
    window_stack_push(window, true);
}

BpmSettings* get_settings() {
	
	return &_bpm_settings;
}

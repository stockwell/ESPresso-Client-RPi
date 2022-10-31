
/**
 * @file main
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lvgl.h"

#include <netdb.h>
#include <pthread.h>
#include <unistd.h>

#include "lvgl/lvgl.h"
#include "lv_drivers/sdl/sdl.h"

#include "EspressoUI.hpp"
#include "EspressoConnectionScreen.hpp"

static void hal_init();
static void timer_init();
static std::string resolveURL(const char* hostname);

namespace
{
	const char* kHostname = "coffee.local";
	const char* kHostnameScales = "espresso-scales.local";
	const auto kMinTicks = 0;//4800;
}

[[noreturn]]int main(int, char**)
{
	/*Initialize LVGL*/
	lv_init();
	hal_init();

	auto& settings = SettingsManager::get();
	settings.load();

	auto resolveFut = std::async(&resolveURL, kHostname);

	std::unique_ptr<BoilerController>	boiler;
	std::unique_ptr<ScalesController>	scales;
	std::unique_ptr<EspressoUI>			ui;

	EspressoConnectionScreen connectionScreen(kHostname);

	bool pendingResolve = true;

	printf("Starting ESPresso-Client, resolving %s... ", kHostname);

	/*Handle LitlevGL tasks (tickless mode)*/
	while (true)
	{
		if (boiler)
			boiler->tick();

		if (scales)
			scales->tick();

		lv_timer_handler();
		usleep(500);

		if (pendingResolve)
		{
			if (resolveFut.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
				continue;

			auto url = resolveFut.get();
			if (url.empty())
			{
				resolveFut = std::async(&resolveURL, kHostname);
				continue;
			}

			pendingResolve = false;

			boiler = std::make_unique<BoilerController>(url);
			scales = std::make_unique<ScalesController>(kHostnameScales);

			ui = std::make_unique<EspressoUI>();

			ui->init(boiler.get(), scales.get());
			boiler->setBoilerBrewTemp(settings["BrewTemp"].getAs<float>());
			boiler->setBoilerSteamTemp(settings["SteamTemp"].getAs<float>());
		}
	}
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Initialize the Hardware Abstraction Layer (HAL) for the LVGL graphics
 * library
 */
static void hal_init()
{
	/* Use the 'monitor' driver which creates window on PC's monitor to simulate a display*/
	sdl_init();

	/*Create a display buffer*/
	static lv_disp_draw_buf_t disp_buf1;
	static lv_color_t buf1_1[SDL_HOR_RES * 100];
	lv_disp_draw_buf_init(&disp_buf1, buf1_1, NULL, SDL_HOR_RES * 100);

	/*Create a display*/
	static lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv); /*Basic initialization*/
	disp_drv.draw_buf = &disp_buf1;
	disp_drv.flush_cb = sdl_display_flush;
	disp_drv.hor_res = SDL_HOR_RES;
	disp_drv.ver_res = SDL_VER_RES;

	lv_disp_t * disp = lv_disp_drv_register(&disp_drv);

	lv_theme_t * th = lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), LV_THEME_DEFAULT_DARK, LV_FONT_DEFAULT);
	lv_disp_set_theme(disp, th);

	lv_group_t * g = lv_group_create();
	lv_group_set_default(g);

	/* Add the mouse as input device
	 * Use the 'mouse' driver which reads the PC's mouse*/
	static lv_indev_drv_t indev_drv_1;
	lv_indev_drv_init(&indev_drv_1); /*Basic initialization*/
	indev_drv_1.type = LV_INDEV_TYPE_POINTER;

	/*This function will be called periodically (by the library) to get the mouse position and state*/
	indev_drv_1.read_cb = sdl_mouse_read;
	lv_indev_t *mouse_indev = lv_indev_drv_register(&indev_drv_1);

	static lv_indev_drv_t indev_drv_2;
	lv_indev_drv_init(&indev_drv_2); /*Basic initialization*/
	indev_drv_2.type = LV_INDEV_TYPE_KEYPAD;
	indev_drv_2.read_cb = sdl_keyboard_read;
	lv_indev_t *kb_indev = lv_indev_drv_register(&indev_drv_2);
	lv_indev_set_group(kb_indev, g);

	static lv_indev_drv_t indev_drv_3;
	lv_indev_drv_init(&indev_drv_3); /*Basic initialization*/
	indev_drv_3.type = LV_INDEV_TYPE_ENCODER;
	indev_drv_3.read_cb = sdl_mousewheel_read;
	lv_indev_t * enc_indev = lv_indev_drv_register(&indev_drv_3);
	lv_indev_set_group(enc_indev, g);

	/*Set a cursor for the mouse*/
	LV_IMG_DECLARE(mouse_cursor_icon); /*Declare the image file.*/
	lv_obj_t * cursor_obj = lv_img_create(lv_scr_act()); /*Create an image object for the cursor */
	lv_img_set_src(cursor_obj, &mouse_cursor_icon);           /*Set the image source*/
	lv_indev_set_cursor(mouse_indev, cursor_obj);             /*Connect the image  object to the driver*/
}

void alarmHandler(int sig_num)
{
	if (sig_num == SIGALRM)
		lv_tick_inc(5);
}

static void timer_init()
{
	signal(SIGALRM, alarmHandler);
	ualarm(5000, 5000);
}

static std::string resolveURL(const char* hostname)
{
	struct hostent* hp = gethostbyname(hostname);

	if (! hp)
		return "";

	return "http://" + std::string(inet_ntoa(*(struct in_addr*)(hp->h_addr_list[0])));
}

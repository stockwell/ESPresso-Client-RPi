
/**
 * @file main
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lvgl.h"
#include "lv_drivers/display/fbdev.h"
#include "lv_drivers/indev/evdev.h"

#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include <netdb.h>

#include "EspressoUI.hpp"

#define DISP_BUF_SIZE (800 * 480)

static void hal_init();
static void timer_init();

int main(int, char**)
{
	/*Initialize LVGL*/
	lv_init();

	/*Linux frame buffer device init*/
	fbdev_init();

	hal_init();
	timer_init();

	auto& settings = SettingsManager::get();

	settings["BrewTemp"] = 93;
	settings["SteamTemp"] = 130;

	struct hostent* hp = gethostbyname("coffee.local");
	std::string url = "http://";

	if (! hp)
		return 1;

	url += inet_ntoa(*(struct in_addr*)(hp->h_addr_list[0]));

	auto boiler = std::make_unique<BoilerController>(url);
	auto ui = std::make_unique<EspressoUI>();

	ui->init(boiler.get());
	boiler->setBoilerTargetTemp(settings["BrewTemp"].getAs<int>());

	settings.save();

	// Yeah, nah
	std::thread{ [&](){
		while(1)
		{
			boiler->tick();
			usleep(20000);
		}
	}}.detach();

	/*Handle LitlevGL tasks (tickless mode)*/
	while (1)
	{
		lv_timer_handler();
		usleep(500);
	}

	return 0;
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
	/*A small buffer for LittlevGL to draw the screen's content*/
	static lv_color_t buf[DISP_BUF_SIZE];
	static lv_color_t buf2[DISP_BUF_SIZE];

	/*Initialize a descriptor for the buffer*/
	static lv_disp_draw_buf_t disp_buf;
	lv_disp_draw_buf_init(&disp_buf, buf, buf2, DISP_BUF_SIZE);

	/*Initialize and register a display driver*/
	static lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.draw_buf = &disp_buf;
	disp_drv.flush_cb = fbdev_flush;
	disp_drv.hor_res = 800;
	disp_drv.ver_res = 480;
	lv_disp_drv_register(&disp_drv);

	evdev_init();
	static lv_indev_drv_t indev_drv_1;
	lv_indev_drv_init(&indev_drv_1); /*Basic initialization*/
	indev_drv_1.type = LV_INDEV_TYPE_POINTER;

	/*This function will be called periodically (by the library) to get the mouse position and state*/
	indev_drv_1.read_cb = evdev_read;
	lv_indev_t* mouse_indev = lv_indev_drv_register(&indev_drv_1);
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

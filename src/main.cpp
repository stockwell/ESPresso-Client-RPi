
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
#include "lv_drivers/indev/evdev2.h"

#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <sys/ioctl.h>

#include "EspressoUI.hpp"
#include "EspressoConnectionScreen.hpp"

#define DISP_BUF_SIZE (800 * 480)

static void hal_init();
static void timer_init();
static std::string resolveURL(const char* hostname);

namespace
{
	const char* kHostnameCore = "coffee.local";
	const char* kHostnameScales = "espresso-scales.local";
	char* kTouchscreenEvDev = "/dev/input/by-path/platform-fe205000.i2c-event";
	char* kKeyboardEvDev = "/dev/input/by-path/platform-fd500000.pcie-pci-0000:01:00.0-usb-0:1.2:1.0-event-kbd";
}

int main(int, char**)
{
	/*Initialize LVGL*/
	lv_init();

	/*Linux frame buffer device init*/
	fbdev_init();

	if (auto fd = open("/dev/tty0", O_RDWR | O_SYNC); fd < 0) {
		return -1;
	}
	else
	{
		ioctl(fd, KDSETMODE, KD_GRAPHICS);
	}

	if (auto fd = open(kTouchscreenEvDev, O_RDWR); fd < 0)
		return -1;

	hal_init();
	timer_init();

	auto& settings = SettingsManager::get();
	settings.load();

	auto resolveFut = std::async(&resolveURL, kHostnameCore);
	auto resolveScalesFut = std::async(&resolveURL, kHostnameScales);

	std::unique_ptr<BoilerController>	boiler;
	std::unique_ptr<ScalesController>	scales;
	std::unique_ptr<EspressoUI>			ui;

	EspressoConnectionScreen connectionScreen(kHostnameCore);

	bool pendingResolve = true;

	printf("Starting ESPresso-Client, resolving %s...\n", kHostnameCore);

	/*Handle LitlevGL tasks (tickless mode)*/
	while (1)
	{
		lv_timer_handler();
		usleep(500);

		if (boiler)
			boiler->tick();

		if (scales)
			scales->tick();

		if (pendingResolve)
		{
			if (lv_tick_get() < 4700 || resolveFut.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
				continue;

			auto url = resolveFut.get();
			if (url.empty())
			{
				resolveFut = std::async(&resolveURL, kHostnameCore);
				continue;
			}

			pendingResolve = false;

			boiler = std::make_unique<BoilerController>(url);
			scales = std::make_unique<ScalesController>(kHostnameScales);
			ui = std::make_unique<EspressoUI>();

			ui->init(boiler.get(), scales.get());

			boiler->setBoilerBrewTemp(settings["BrewTemp"].getAs<float>());
			boiler->setBoilerSteamTemp(settings["SteamTemp"].getAs<float>());
			boiler->setBoilerBrewPressure(settings["BrewPressure"].getAs<float>());
		}
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
	disp_drv.sw_rotate=1;
	lv_disp_drv_register(&disp_drv);

	lv_disp_set_rotation(NULL, LV_DISP_ROT_180);

	evdev_set_file(kTouchscreenEvDev);
	static lv_indev_drv_t indev_drv_1;
	lv_indev_drv_init(&indev_drv_1); /*Basic initialization*/
	indev_drv_1.type = LV_INDEV_TYPE_POINTER;

	/*This function will be called periodically (by the library) to get the mouse position and state*/
	indev_drv_1.read_cb = evdev_read;
	lv_indev_t* mouse_indev = lv_indev_drv_register(&indev_drv_1);

	evdev2_set_file(kKeyboardEvDev);
	static lv_indev_drv_t indev_drv_2;
	lv_indev_drv_init(&indev_drv_2); /*Basic initialization*/
	indev_drv_2.type = LV_INDEV_TYPE_KEYPAD;

	indev_drv_2.read_cb = evdev2_read;
	lv_indev_t* kb_indev = lv_indev_drv_register(&indev_drv_2);
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


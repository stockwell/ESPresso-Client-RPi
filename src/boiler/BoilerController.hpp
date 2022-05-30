#pragma once

#include "SettingsManager.hpp"

#include <set>
#include <future>

#include <httplib.h>

enum class BoilerState
{
	Heating,
	Ready,
	Brewing,
};

class BoilerTemperatureDelegate
{
public:
	virtual void onBoilerCurrentTempChanged(float temp)		{ };
	virtual void onBoilerTargetTempChanged(float temp)		{ };
	virtual void onBoilerBrewTempChanged(float temp)		{ };
	virtual void onBoilerSteamTempChanged(float temp)		{ };
	virtual void onBoilerStateChanged(BoilerState state)	{ };
};

class BoilerController : public SettingDelegate
{
public:

	BoilerController(const std::string& url);

	void registerBoilerTemperatureDelegate(BoilerTemperatureDelegate* delegate);
	void deregisterBoilerTemperatureDelegate(BoilerTemperatureDelegate* delegate);

	void setBoilerBrewTemp(float temp);
	void setBoilerSteamTemp(float temp);

	void updateBoilerTargetTemp(float temp);
	void updateBoilerCurrentTemp(float temp);
	void updateBoilerState(int state);

	void tick();

	// SettingDelegate i/f
	void onChanged(const std::string& key, float val) override;

private:
	struct PollData
	{
		float	currentTemp;
		float	targetTemp;
		int		state;
	};

	PollData pollRemoteServer();
	std::future<PollData>					m_pollFut;

	BoilerState								m_state;
	std::set<BoilerTemperatureDelegate*>	m_delegates;
	httplib::Client							m_httpClient;

	float m_targetTemp	= 0.0;
	float m_currentTemp	= 0.0;
	float m_brewTarget = 0.0;
	float m_steamTarget = 0.0;

};

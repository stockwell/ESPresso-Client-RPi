#pragma once

#include <set>
#include <future>

#include <httplib.h>

class BoilerTemperatureDelegate
{
public:
	virtual void onBoilerCurrentTempChanged(float temp)	{ };
	virtual void onBoilerTargetTempChanged(float temp) { };
	virtual void onBoilerBrewTempChanged(float temp) { };
	virtual void onBoilerSteamTempChanged(float temp) { };
};

class BoilerController
{
public:
	enum class BoilerState
	{
		Heating,
		Ready,
	};

	BoilerController(const std::string& url);

	void registerBoilerTemperatureDelegate(BoilerTemperatureDelegate* delegate);
	void deregisterBoilerTemperatureDelegate(BoilerTemperatureDelegate* delegate);

	void setBoilerBrewTemp(float temp);
	void setBoilerSteamTemp(float temp);

	void updateBoilerTargetTemp(float temp);
	void updateBoilerCurrentTemp(float temp);

	void tick();

private:
	struct PollData
	{
		float currentTemp;
		float targetTemp;
	};

	PollData pollRemoteServer();
	std::future<PollData> m_pollFut;

	std::set<BoilerTemperatureDelegate*> m_delegates;
	httplib::Client m_httpClient;

	float m_targetTemp	= 0.0;
	float m_currentTemp	= 0.0;
	float m_brewTarget = 0.0;
	float m_steamTarget = 0.0;

	float m_P;
	float m_I;
	float m_D;
};

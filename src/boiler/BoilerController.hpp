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
	Inhibited,
	Idle,
};

class BoilerTemperatureDelegate
{
public:
	virtual void onBoilerCurrentTempChanged(float temp)		{ };
	virtual void onBoilerTargetTempChanged(float temp)		{ };
	virtual void onBoilerBrewTempChanged(float temp)		{ };
	virtual void onBoilerSteamTempChanged(float temp)		{ };
	virtual void onBoilerStateChanged(BoilerState state)	{ };
	virtual void onBoilerPressureChanged(float temp)		{ };
};

class BoilerController : public SettingDelegate
{
public:

	BoilerController(const std::string& url);

	void registerBoilerTemperatureDelegate(BoilerTemperatureDelegate* delegate);
	void deregisterBoilerTemperatureDelegate(BoilerTemperatureDelegate* delegate);

	void setBoilerBrewTemp(float temp);
	void setBoilerSteamTemp(float temp);
	void setBoilerBrewPressure(float pressure);

	void updateBoilerTargetTemp(float temp);
	void updateBoilerCurrentTemp(float temp);
	void updateBoilerCurrentPressure(float pressure);
	void updatePumpDuty(float duty);
	void updateBoilerState(int state);

	void tick();

	// SettingDelegate i/f
	void onChanged(const std::string& key, float val) override;
	void onChanged(const std::string& key, bool val) override;

private:
	struct PollData
	{
		float	currentTemp;
		float	targetTemp;
		float 	currentPressure;
		float	pumpDuty;
		int		state;
	};

	struct PIDTerms
	{
		float Kp = 0.0f;
		float Ki = 0.0f;
		float Kd = 0.0f;

		auto operator<=>(const PIDTerms&) const = default;
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
	float m_brewTargetPressure = 0.0;
	float m_brewCurrentPressure = -1.0f;
	float m_pumpDuty = 0.0f;

	bool m_pumpManualMode = false;
	bool m_hotWaterMode = false;

	PIDTerms m_boilerPID	= {0, 0, 0};
	PIDTerms m_pumpPID		= {0, 0, 0};

	std::unordered_map<std::string, float&> m_floatSettings;
	std::unordered_map<std::string, bool&>  m_boolSettings;
};

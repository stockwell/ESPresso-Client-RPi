#pragma once

#include "SettingsManager.hpp"

#include <set>
#include <future>

#include <httplib.h>

class ScalesWeightDelegate
{
public:
	virtual void onScalesWeightChanged(float weight)		{ };
};

class ScalesController
{
public:
	ScalesController(const std::string& url);

	void registerWeightDelegate(ScalesWeightDelegate* delegate);
	void deregisterWeightDelegate(ScalesWeightDelegate* delegate);

	void tick();

private:
	void updateWeight(float weight);

private:
	struct PollData
	{
		float	currentWeight;
	};

	PollData pollRemoteServer();
	std::future<PollData>				m_pollFut;

	std::set<ScalesWeightDelegate*>		m_delegates;
	httplib::Client						m_httpClient;

	float 								m_currentWeight = 0.0f;
};

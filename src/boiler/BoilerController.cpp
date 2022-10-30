#include "BoilerController.hpp"
#include "SettingsManager.hpp"

#include "nlohmann/json.hpp"

BoilerController::BoilerController(const std::string& url)
	: m_httpClient(url)
{
	m_httpClient.set_keep_alive(true);

	auto res = m_httpClient.Get("/api/v1/temp/raw");
	auto tempJSON = nlohmann::json::parse(res->body);

	m_currentTemp = tempJSON["current"].get<float>();
	m_targetTemp = tempJSON["target"].get<float>();
	m_brewTarget = tempJSON["brew"].get<float>();
	m_steamTarget = tempJSON["steam"].get<float>();

	res = m_httpClient.Get("/api/v1/pressure/raw");
	auto pressureJSON = nlohmann::json::parse(res->body);

	auto pressureCurrent = pressureJSON["current"].get<float>();
	auto pressureTarget = pressureJSON["target"].get<float>();
	m_brewTargetPressure = pressureJSON["brew"].get<float>();

	auto& settings = SettingsManager::get();

	settings["BrewTemp"].registerDelegate(this);
	settings["SteamTemp"].registerDelegate(this);
	settings["BrewPressure"].registerDelegate(this);

	m_floatSettings.emplace("BrewTemp", m_brewTarget);
	m_floatSettings.emplace("SteamTemp", m_steamTarget);
	m_floatSettings.emplace("BrewPressure", m_brewTargetPressure);

	nlohmann::json pidSetJSON;

	m_boilerPID = {
		settings["BoilerKp"].getAs<float>(),
		settings["BoilerKi"].getAs<float>(),
		settings["BoilerKd"].getAs<float>(),
	};

	m_pumpPID = {
		settings["PumpKp"].getAs<float>(),
		settings["PumpKi"].getAs<float>(),
		settings["PumpKd"].getAs<float>(),
	};

	pidSetJSON["PumpPID"]   = { m_pumpPID.Kp, m_pumpPID.Ki, m_pumpPID.Kd };
	pidSetJSON["BoilerPID"] = { m_boilerPID.Kp, m_boilerPID.Ki, m_boilerPID.Kd };

	res = m_httpClient.Post("/api/v1/pid/terms", pidSetJSON.dump(), "application/json");

	settings["BoilerKp"].registerDelegate(this);
	settings["BoilerKi"].registerDelegate(this);
	settings["BoilerKd"].registerDelegate(this);

	m_floatSettings.emplace("BoilerKp", m_boilerPID.Kp);
	m_floatSettings.emplace("BoilerKi", m_boilerPID.Ki);
	m_floatSettings.emplace("BoilerKd", m_boilerPID.Kd);

	settings["PumpKp"].registerDelegate(this);
	settings["PumpKi"].registerDelegate(this);
	settings["PumpKd"].registerDelegate(this);

	m_floatSettings.emplace("PumpKp", m_pumpPID.Kp);
	m_floatSettings.emplace("PumpKi", m_pumpPID.Ki);
	m_floatSettings.emplace("PumpKd", m_pumpPID.Kd);

	res = m_httpClient.Post("/api/v1/boiler/clear-inhibit", "", "application/json");

	settings["ManualPumpControl"].registerDelegate(this);
	m_floatSettings.emplace("ManualPumpControl", m_pumpDuty);

	m_pollFut = std::async(&BoilerController::pollRemoteServer, this);
}

void BoilerController::registerBoilerTemperatureDelegate(BoilerTemperatureDelegate* delegate)
{
	if (m_delegates.find(delegate) != m_delegates.end())
		return;

	m_delegates.emplace(delegate);

	delegate->onBoilerCurrentTempChanged(m_currentTemp);
	delegate->onBoilerTargetTempChanged(m_targetTemp);
}

void BoilerController::deregisterBoilerTemperatureDelegate(BoilerTemperatureDelegate* delegate)
{
	if (auto it = m_delegates.find(delegate); it != m_delegates.end())
		m_delegates.erase(it);
}

void BoilerController::updateBoilerTargetTemp(float temp)
{
	if (m_targetTemp == temp)
		return;

	m_targetTemp = temp;

	for (auto delegate : m_delegates)
		delegate->onBoilerTargetTempChanged(temp);
}

void BoilerController::updateBoilerCurrentTemp(float temp)
{
	if (m_currentTemp == temp)
		return;

	m_currentTemp = temp;

	for (auto delegate : m_delegates)
		delegate->onBoilerCurrentTempChanged(temp);
}

void BoilerController::updateBoilerState(int state)
{
	auto currentState = static_cast<BoilerState>(state);

	if (m_state == currentState)
		return;

	m_state = currentState;

	for (auto delegate : m_delegates)
		delegate->onBoilerStateChanged(currentState);
}

void BoilerController::updatePumpDuty(float duty)
{
	if (m_pumpDuty == duty)
		return;

	m_pumpDuty = duty;
}

void BoilerController::setBoilerBrewTemp(float temp)
{
	if (m_brewTarget == temp)
		return;

	m_brewTarget = temp;

	nlohmann::json tempSetJSON;
	tempSetJSON["brewTarget"] = temp;

	auto res = m_httpClient.Post("/api/v1/temp/raw", tempSetJSON.dump(), "application/json");

	for (auto delegate : m_delegates)
		delegate->onBoilerBrewTempChanged(temp);
}

void BoilerController::setBoilerBrewPressure(float pressure)
{
	if (m_brewTargetPressure == pressure)
		return;

	m_brewTargetPressure = pressure;

	nlohmann::json pressureJSON;
	pressureJSON["brewTarget"] = pressure;

	auto res = m_httpClient.Post("/api/v1/pressure/raw", pressureJSON.dump(), "application/json");

//	for (auto delegate : m_delegates)
//		delegate->onBoilerBrewTempChanged(temp);
}

void BoilerController::updateBoilerCurrentPressure(float pressure)
{
	if (m_brewCurrentPressure == pressure)
		return;

	m_brewCurrentPressure = pressure;

	for (auto delegate : m_delegates)
		delegate->onBoilerPressureChanged(pressure);
}

void BoilerController::setBoilerSteamTemp(float temp)
{
	if (m_steamTarget == temp)
		return;

	m_steamTarget = temp;

	nlohmann::json tempSetJSON;
	tempSetJSON["steamTarget"] = temp;

	auto res = m_httpClient.Post("/api/v1/temp/raw", tempSetJSON.dump(), "application/json");

	for (auto delegate : m_delegates)
		delegate->onBoilerSteamTempChanged(temp);
}

void BoilerController::tick()
{
	if (m_pollFut.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
	{
		auto val = m_pollFut.get();
		updateBoilerCurrentTemp(val.currentTemp);
		updateBoilerTargetTemp(val.targetTemp);
		updateBoilerState(val.state);

		updateBoilerCurrentPressure(val.currentPressure);

		m_pollFut = std::async(std::launch::async, &BoilerController::pollRemoteServer, this);
	}
}

void BoilerController::onChanged(const std::string& key, float val)
{
	if (auto it = m_floatSettings.find(key); it != m_floatSettings.end())
		it->second = val;
}

BoilerController::PollData BoilerController::pollRemoteServer()
{
	auto res = m_httpClient.Get("/api/v1/temp/raw");
	auto tempJSON = nlohmann::json::parse(res->body);
	auto boilerTemp = tempJSON["current"].get<float>();
	auto targetTemp = tempJSON["target"].get<float>();
	auto boilerState = tempJSON["state"].get<int>();

	res = m_httpClient.Get("/api/v1/pressure/raw");
	auto pressureJSON = nlohmann::json::parse(res->body);
	auto pressureCurrent = pressureJSON["current"].get<float>();
	auto pressureTarget = pressureJSON["target"].get<float>();
	auto pressureBrewTarget = pressureJSON["brew"].get<float>();
	auto pumpDuty = pressureJSON["manual-duty"].get<float>();
	auto pumpState = pressureJSON["state"].get<int>();

	if (m_brewTarget != tempJSON["brew"].get<float>())
	{
		nlohmann::json brewTargetJSON;
		brewTargetJSON["brewTarget"] = m_brewTarget;

		res = m_httpClient.Post("/api/v1/temp/raw", brewTargetJSON.dump(), "application/json");
	}

	if (m_steamTarget != tempJSON["steam"].get<float>())
	{
		nlohmann::json steamTargetJSON;
		steamTargetJSON["steamTarget"] = m_steamTarget;

		res = m_httpClient.Post("/api/v1/temp/raw", steamTargetJSON.dump(), "application/json");
	}

	if (m_brewTargetPressure != pressureBrewTarget)
	{
		nlohmann::json brewTargetJSON;
		brewTargetJSON["brewTarget"] = m_brewTargetPressure;

		res = m_httpClient.Post("/api/v1/pressure/raw", brewTargetJSON.dump(), "application/json");
	}

	if (m_pumpDuty != pumpDuty)
	{
		nlohmann::json pumpControlJSON;
		pumpControlJSON["Duty"] = m_pumpDuty;

		res = m_httpClient.Post("/api/v1/pump/manual-control", pumpControlJSON.dump(), "application/json");
	}

	res = m_httpClient.Get("/api/v1/sys/info");
	auto sysinfoJSON = nlohmann::json::parse(res->body);
	auto freeHeap = sysinfoJSON["free_heap"].get<int>();
	auto minFreeHeap = sysinfoJSON["min_free_heap"].get<int>();

	static auto printTrigger = 20;
	if (!--printTrigger)
	{
		printf("Core -- Heap: %dKB (%dKB)\n", freeHeap/1024, minFreeHeap/1024);

		printTrigger = 20;
	}

	res = m_httpClient.Get("/api/v1/pid/terms");
	auto pidTermsJSON = nlohmann::json::parse(res->body);

	PIDTerms boilerPID = {
		 pidTermsJSON["BoilerPID"][0].get<float>(),
		 pidTermsJSON["BoilerPID"][1].get<float>(),
		 pidTermsJSON["BoilerPID"][2].get<float>(),
	};

	PIDTerms pumpPID = {
		 pidTermsJSON["PumpPID"][0].get<float>(),
		 pidTermsJSON["PumpPID"][1].get<float>(),
		 pidTermsJSON["PumpPID"][2].get<float>(),
	};

	if (boilerPID != m_boilerPID)
	{
		 nlohmann::json pidSetJSON;
		 pidSetJSON["BoilerPID"] = { m_boilerPID.Kp, m_boilerPID.Ki, m_boilerPID.Kd };

		 res = m_httpClient.Post("/api/v1/pid/terms", pidSetJSON.dump(), "application/json");
	}

	if (pumpPID != m_pumpPID)
	{
		 nlohmann::json pidSetJSON;
		 pidSetJSON["PumpPID"]   = { m_pumpPID.Kp, m_pumpPID.Ki, m_pumpPID.Kd };

		 res = m_httpClient.Post("/api/v1/pid/terms", pidSetJSON.dump(), "application/json");
	}

	return { boilerTemp, targetTemp, pressureCurrent, pumpDuty, boilerState};
}

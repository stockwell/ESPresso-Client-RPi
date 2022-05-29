#include "BoilerController.hpp"
#include "SettingsManager.hpp"

#include "nlohmann/json.hpp"

BoilerController::BoilerController(const std::string& url)
	: m_httpClient(url)
{
	m_httpClient.set_keep_alive(true);

	auto res = m_httpClient.Get("/api/v1/temp/raw");
	auto j3 = nlohmann::json::parse(res->body);

	m_currentTemp = j3["current"].get<float>();
	m_targetTemp = j3["target"].get<float>();
	m_brewTarget = j3["brew"].get<float>();
	m_steamTarget = j3["steam"].get<float>();

	auto& settings = SettingsManager::get();

	res = m_httpClient.Get("/api/v1/pid/terms");
	j3 = nlohmann::json::parse(res->body);

	printf("%s\n", to_string(j3).c_str());

	nlohmann::json pidSetJSON;
	pidSetJSON["Kp"] = 100.0f;
	pidSetJSON["Ki"] = 20.0f;
	pidSetJSON["Kd"] = 50.0f;

	res = m_httpClient.Post("/api/v1/pid/terms", pidSetJSON.dump(), "application/json");

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

		m_pollFut = std::async(std::launch::async, &BoilerController::pollRemoteServer, this);
	}
}

BoilerController::PollData BoilerController::pollRemoteServer()
{
	auto res = m_httpClient.Get("/api/v1/temp/raw");
	auto tempJSON = nlohmann::json::parse(res->body);
	auto boilerTemp = tempJSON["current"].get<float>();
	auto targetTemp = tempJSON["target"].get<float>();

	static int delay = 100;
	if (!--delay)
	{
		delay = 200;
		res = m_httpClient.Get("/api/v1/sys/info");
		auto sysinfoJSON = nlohmann::json::parse(res->body);
		auto freeHeap = sysinfoJSON["free_heap"].get<int>();

		res = m_httpClient.Get("/api/v1/pressure/raw");
		auto pressureJSON = nlohmann::json::parse(res->body);
		auto pressureRaw = pressureJSON["current"].get<float>();

		printf("Heap: %dKB\n", freeHeap/1024);
		printf("Pressure: %.02f\n\n", pressureRaw);
	}

	return { boilerTemp, targetTemp };
}

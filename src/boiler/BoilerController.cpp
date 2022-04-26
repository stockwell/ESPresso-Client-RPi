#include "BoilerController.hpp"

#include "nlohmann/json.hpp"

BoilerController::BoilerController(const std::string& url)
	: m_httpClient(url)
{
	m_httpClient.set_keep_alive(true);

	auto res = m_httpClient.Get("/api/v1/temp/raw");
	auto j3 = nlohmann::json::parse(res->body);

	m_currentTemp = j3["current"].get<float>();

	printf("%s\n", to_string(j3).c_str());

	res = m_httpClient.Get("/api/v1/pid/terms");
	j3 = nlohmann::json::parse(res->body);

	printf("%s\n", to_string(j3).c_str());

	nlohmann::json pidSetJSON;
	pidSetJSON["Kp"] = 10;
	pidSetJSON["Ki"] = 50;
	pidSetJSON["Kd"] = 0.9;

	res = m_httpClient.Post("/api/v1/pid/terms", pidSetJSON.dump(), "application/json");

	setBoilerTargetTemp(25);

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

void BoilerController::setBoilerCurrentTemp(float temp)
{
	if (m_currentTemp == temp)
		return;

	m_currentTemp = temp;

	for (auto delegate : m_delegates)
		delegate->onBoilerCurrentTempChanged(temp);
}

void BoilerController::setBoilerTargetTemp(float temp)
{
	if (m_targetTemp == temp)
		return;

	m_targetTemp = temp;

	nlohmann::json tempSetJSON;
	tempSetJSON["target"] = temp;

	auto res = m_httpClient.Post("/api/v1/temp/raw", tempSetJSON.dump().c_str(), "application/json");

	for (auto delegate : m_delegates)
		delegate->onBoilerTargetTempChanged(temp);
}

void BoilerController::tick()
{
	if (m_pollFut.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
	{
		auto val = m_pollFut.get();
		setBoilerCurrentTemp(val.currentTemp);

		if (val.currentTemp <= 26)
			setBoilerTargetTemp(93);

		m_pollFut = std::async(std::launch::async, &BoilerController::pollRemoteServer, this);
	}
}

BoilerController::PollData BoilerController::pollRemoteServer()
{
	auto res = m_httpClient.Get("/api/v1/temp/raw");
	auto tempJSON = nlohmann::json::parse(res->body);
	auto boilerTemp = tempJSON["current"].get<float>();

	static int delay = 100;

	if (!--delay)
	{
		delay = 100;
		res = m_httpClient.Get("/api/v1/sys/info");
		auto sysinfoJSON = nlohmann::json::parse(res->body);
		auto freeHeap = sysinfoJSON["free_heap"].get<int>();

		printf("Heap: %dKB\n", freeHeap/1024);
	}

	return { boilerTemp };
}

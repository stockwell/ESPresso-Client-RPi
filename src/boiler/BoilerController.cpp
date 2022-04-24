#include "BoilerController.hpp"

#include "nlohmann/json.hpp"


BoilerController::BoilerController(const std::string& url)
    : m_httpClient(url)
{

    auto res = m_httpClient.Get("/api/v1/temp/raw");
    auto j3 = nlohmann::json::parse(res->body);

    m_currentTemp = j3["current"].get<float>();

	printf("%s\n", to_string(j3).c_str());

	res = m_httpClient.Get("/api/v1/pid/terms");
	j3 = nlohmann::json::parse(res->body);

	printf("%s\n", to_string(j3).c_str());

	const char* body = "{\n"
					   "\"Kp\": 500,\n"
					   "\"Ki\": 200,\n"
					   "\"Kd\": 25\n"
					   "}";

	res = m_httpClient.Post("/api/v1/pid/terms", body, "application/json");

	const char* bodyTemp = "{\n"
					   "\"target\": 0\n"
					   "}";

	res = m_httpClient.Post("/api/v1/temp/raw", bodyTemp, "application/json");
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
    
    for (auto delegate : m_delegates)
        delegate->onBoilerTargetTempChanged(temp);
}

void BoilerController::tick()
{
	static int delay = 100;

	if (!--delay)
	{
		delay = 100;
		auto res = m_httpClient.Get("/api/v1/sys/info");
		auto j3 = nlohmann::json::parse(res->body);
		auto val = j3["free_heap"].get<int>();

		printf("Heap: %dKB\n", val/1024);
	}

    auto res = m_httpClient.Get("/api/v1/temp/raw");
    auto j3 = nlohmann::json::parse(res->body);
	auto val = j3["current"].get<float>();

    setBoilerCurrentTemp(val);

	if (val < 25)
	{
		const char* bodyTemp = "{\n"
							   "\"target\": 93\n"
							   "}";

		res = m_httpClient.Post("/api/v1/temp/raw", bodyTemp, "application/json");

	}

}

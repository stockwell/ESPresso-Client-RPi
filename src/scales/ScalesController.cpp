#include "ScalesController.hpp"
#include "SettingsManager.hpp"

#include "nlohmann/json.hpp"

ScalesController::ScalesController(const std::string& url)
	: m_httpClient(url)
{
	m_httpClient.set_keep_alive(true);

	m_pollFut = std::async(&ScalesController::pollRemoteServer, this);
}

void ScalesController::registerWeightDelegate(ScalesWeightDelegate* delegate)
{
	if (m_delegates.find(delegate) != m_delegates.end())
		return;

	m_delegates.emplace(delegate);

	delegate->onScalesWeightChanged(m_currentWeight);
}

void ScalesController::deregisterWeightDelegate(ScalesWeightDelegate* delegate)
{
	if (auto it = m_delegates.find(delegate); it != m_delegates.end())
		m_delegates.erase(it);
}

void ScalesController::updateWeight(float weight)
{
	if (m_currentWeight == weight)
		return;

	m_currentWeight = weight;

	for (auto delegate : m_delegates)
		delegate->onScalesWeightChanged(weight);
}

void ScalesController::tick()
{
	if (m_pollFut.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
	{
		auto val = m_pollFut.get();
		updateWeight(val.currentWeight);

		m_pollFut = std::async(std::launch::async, &ScalesController::pollRemoteServer, this);
	}
}

ScalesController::PollData ScalesController::pollRemoteServer()
{
	auto res = m_httpClient.Get("/api/v1/weight");
	auto tempJSON = nlohmann::json::parse(res->body);
	auto currentWeight = tempJSON["weight"].get<float>();

	res = m_httpClient.Get("/api/v1/sys/info");
	auto sysinfoJSON = nlohmann::json::parse(res->body);
	auto freeHeap = sysinfoJSON["free_heap"].get<int>();
	auto minFreeHeap = sysinfoJSON["min_free_heap"].get<int>();

	static auto printTrigger = 20;
	if (!--printTrigger)
	{
		printf("Scales -- Heap: %dKB (%dKB)\n", freeHeap/1024, minFreeHeap/1024);
		printTrigger = 20;
	}

	return { currentWeight };
}

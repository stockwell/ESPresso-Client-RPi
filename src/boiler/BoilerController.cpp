#include "BoilerController.hpp"

#include "nlohmann/json.hpp"


BoilerController::BoilerController(const std::string& url)
    : m_httpClient(url)
{

    auto res = m_httpClient.Get("/api/v1/temp/raw");
    auto j3 = nlohmann::json::parse(res->body);

    m_currentTemp = j3["raw"].get<float>();
}


void BoilerController::registerBoilerTemperatureDelegate(BoilerTemperatureDelegate* delegate)
{
    if (m_delegates.find(delegate) != m_delegates.end())
        return;

    m_delegates.emplace(delegate);
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
    auto res = m_httpClient.Get("/api/v1/temp/raw");
    auto j3 = nlohmann::json::parse(res->body);

    setBoilerCurrentTemp(j3["raw"].get<float>());
}
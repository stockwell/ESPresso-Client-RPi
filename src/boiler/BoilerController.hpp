#pragma once

#include <set>

#include <httplib.h>

class BoilerTemperatureDelegate
{
public:
    virtual void onBoilerCurrentTempChanged(float temp) { };
    virtual void onBoilerTargetTempChanged(float temp) { };
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

    void setBoilerTargetTemp(float temp);
    void setBoilerCurrentTemp(float temp);

    void tick();

private:
    std::set<BoilerTemperatureDelegate*>    m_delegates;
    httplib::Client                         m_httpClient;

    float m_targetTemp = 100.0;
    float m_currentTemp = 0.0;

    float m_P;
    float m_I;
    float m_D;
};
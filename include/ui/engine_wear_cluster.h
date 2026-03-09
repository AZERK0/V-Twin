#ifndef ATG_ENGINE_SIM_ENGINE_WEAR_CLUSTER_H
#define ATG_ENGINE_SIM_ENGINE_WEAR_CLUSTER_H

#include "ui/ui_element.h"

#include <array>
#include <string>

class Simulator;

class EngineWearCluster : public UiElement {
    public:
        EngineWearCluster();
        virtual ~EngineWearCluster();

        virtual void initialize(EngineSimApplication *app);
        virtual void destroy();
        virtual void update(float dt);
        virtual void render();

        void renderDashboard(const Bounds &bounds);
        void setSimulator(Simulator *simulator);

    private:
        static constexpr int HistorySamples = 36;

        void updateFakeTelemetry(float dt);
        void renderSummary(const Bounds &bounds);
        void drawMetricRow(
            const Bounds &bounds,
            const std::string &label,
            float displayValue,
            float severity);
        void drawHistoryChart(const Bounds &bounds);
        void drawStatusBadge(const Bounds &bounds, const std::string &label, float severity);
        void pushHistorySample(float sample);

        float getRpmLoad() const;
        float getThrottleLoad() const;
        float getSeverityIndex() const;
        ysVector getSeverityColor(float value) const;
        std::string getConditionLabel() const;
        std::string getPrimaryAlert() const;

    private:
        Simulator *m_simulator;
        std::array<float, HistorySamples> m_severityHistory;
        int m_historyCount;
        int m_historyWriteIndex;

        float m_elapsedTime;
        float m_engineHealth;
        float m_wearRatePerMinute;
        float m_thermalStress;
        float m_lubricationReserve;
        float m_knockExposure;
        float m_bearingStress;
        float m_ringSealLoss;
        float m_valvetrainFatigue;
        float m_headThermalLoad;
        float m_oilTemperature;
        float m_coolantTemperature;
        float m_blowBy;
};

#endif /* ATG_ENGINE_SIM_ENGINE_WEAR_CLUSTER_H */

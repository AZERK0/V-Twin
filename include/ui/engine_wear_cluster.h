#ifndef ATG_ENGINE_SIM_ENGINE_WEAR_CLUSTER_H
#define ATG_ENGINE_SIM_ENGINE_WEAR_CLUSTER_H

#include "simulation/engine_wear_model.h"
#include "ui/ui_element.h"

#include <cstdint>
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
        void refreshCachedStrings();
        void renderSummary(const Bounds &bounds);
        void renderOverviewPanel(const Bounds &bounds);
        void renderExposurePanel(const Bounds &bounds);
        void renderLiveStressPanel(const Bounds &bounds);
        void renderDamagePanel(const Bounds &bounds);
        void drawMetricRow(
            const Bounds &bounds,
            const std::string &label,
            const std::string &valueText,
            float displayRatio,
            float risk);
        void drawInfoRow(
            const Bounds &bounds,
            const std::string &label,
            const std::string &valueText,
            float textSize);

        ysVector getRiskColor(float risk) const;
        const char *getFailureModeLabel(EngineWearState::FailureMode mode) const;

    private:
        Simulator *m_simulator;
        EngineWearState m_state;
        std::uint64_t m_cachedRevision;

        std::string m_healthText;
        std::string m_damageRateText;
        std::string m_rulText;
        std::string m_confidenceText;
        std::string m_failureModeText;
        std::string m_oilTemperatureText;
        std::string m_coolantTemperatureText;

        std::string m_thermalMarginText;
        std::string m_lubricationMarginText;
        std::string m_detonationMarginText;
        std::string m_fatigueLoadText;
        std::string m_combustionStabilityText;

        std::string m_bottomEndDamageText;
        std::string m_ringSealDamageText;
        std::string m_valvetrainDamageText;
        std::string m_headGasketDamageText;
        std::string m_lubricationDamageText;

        std::string m_overRevText;
        std::string m_overTempText;
        std::string m_coldLoadText;
        std::string m_oilStarvationText;
        std::string m_knockEventText;
        std::string m_thermalCyclesText;
};

#endif /* ATG_ENGINE_SIM_ENGINE_WEAR_CLUSTER_H */

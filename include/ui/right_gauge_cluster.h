#ifndef ATG_ENGINE_SIM_RIGHT_GAUGE_CLUSTER_H
#define ATG_ENGINE_SIM_RIGHT_GAUGE_CLUSTER_H

#include "ui/ui_element.h"

#include <string>

class AfrCluster;
class Engine;
class FiringOrderDisplay;
class FuelCluster;
class LabeledGauge;
class Simulator;
class ThrottleDisplay;

class RightGaugeCluster : public UiElement {
    public:
        enum class Layout {
            Standard,
            CompactCondition
        };

        RightGaugeCluster();
        virtual ~RightGaugeCluster();

        virtual void initialize(EngineSimApplication *app);
        virtual void destroy();

        virtual void update(float dt);
        virtual void render();

        void setEngine(Engine *engine);
        void setLayout(Layout layout) { m_layout = layout; }
        void setUnits();
        double getManifoldPressureWithUnits(double ambientPressure);

        Simulator *m_simulator;

    private:
        double getRpm() const;
        double getRedline() const;
        double getSpeed() const;
        double getManifoldPressure() const;
        void setLayoutVisibility(bool compact);
        void renderStandard();
        void renderCompactCondition();
        void updateTachometer(const Bounds &bounds);
        void updateSpeedometer(const Bounds &bounds);
        void updateAirGauges(
            const Bounds &manifoldBounds,
            const Bounds &airFlowBounds,
            const Bounds &volumetricEfficiencyBounds);
        void updateManifoldGauge(const Bounds &bounds);
        double getFiniteIntakeFlowRate() const;
        double calculateVolumetricEfficiency(double intakeFlowRate) const;

    protected:
        Engine *m_engine;

        void renderTachSpeedCluster(const Bounds &bounds);
        void renderFuelAirCluster(const Bounds &bounds);

        LabeledGauge *m_tachometer;
        LabeledGauge *m_speedometer;
        LabeledGauge *m_manifoldVacuumGauge;
        LabeledGauge *m_intakeCfmGauge;
        LabeledGauge *m_volumetricEffGauge;
        FuelCluster *m_fuelCluster;
        ThrottleDisplay *m_throttleDisplay;
        AfrCluster *m_afrCluster;
        FiringOrderDisplay *m_combustionChamberStatus;
        std::string m_speedUnits;
        std::string m_pressureUnits;
        bool m_isAbsolute;
        Layout m_layout;
};

#endif /* ATG_ENGINE_SIM_GAUGE_CLUSTER_H */

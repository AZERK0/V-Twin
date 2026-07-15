#ifndef ATG_ENGINE_SIM_ENGINE_CONDITION_CLUSTER_H
#define ATG_ENGINE_SIM_ENGINE_CONDITION_CLUSTER_H

#include "simulation/engine_condition_model.h"
#include "ui/ui_element.h"

#include <cstdint>
#include <string>

class Gauge;
class Simulator;

class EngineConditionCluster : public UiElement {
public:
    EngineConditionCluster();
    virtual ~EngineConditionCluster();

    virtual void initialize(EngineSimApplication *app);
    virtual void destroy();
    virtual void update(float dt);
    virtual void render();

    void setSimulator(Simulator *simulator);

private:
    void refreshCachedStrings();
    void refreshTelemetryStrings();
    void refreshTemperatureStrings();
    void refreshOperatingPointStrings();
    void updateTemperatureGauges();

    void renderHeader(const Bounds &bounds);
    void renderStatusPanel(const Bounds &bounds);
    void renderStatusTile(const Bounds &bounds, const std::string &label, bool active, bool standby = false);
    void renderTelemetryPanel(const Bounds &bounds);
    void renderValueTile(const Bounds &bounds, const std::string &label, const std::string &value);
    void renderTemperaturePanel(const Bounds &bounds);
    void renderTemperatureTile(
        const Bounds &bounds,
        const std::string &label,
        const std::string &detail,
        const std::string &value,
        Gauge *gauge);
    void renderCylinderPanel(const Bounds &bounds);
    void renderCylinderColumn(const Bounds &bounds, int firstCylinder, int cylinderCount);
    void renderCylinderRow(const Bounds &bounds, int cylinderIndex, float textSize);
    void renderOperatingPointPanel(const Bounds &bounds);
    void renderModelCoveragePanel(const Bounds &bounds);
    void renderInfoRow(const Bounds &bounds, const std::string &label, const std::string &value);
    void renderUnavailableState(const Bounds &bounds);

private:
    Simulator *m_simulator;
    Gauge *m_oilTemperatureGauge;
    Gauge *m_pistonTemperatureGauge;
    Gauge *m_cylinderTemperatureGauge;
    EngineConditionState m_state;
    std::uint64_t m_cachedRevision;

    std::string m_engineSpeedText;
    std::string m_vehicleSpeedText;
    std::string m_gearText;
    std::string m_powerText;
    std::string m_torqueText;
    std::string m_manifoldPressureText;
    std::string m_airFlowText;
    std::string m_volumetricEfficiencyText;
    std::string m_oilTemperatureText;
    std::string m_pistonTemperatureText;
    std::string m_cylinderTemperatureText;
    std::string m_throttleText;
    std::string m_intakeAfrText;
    std::string m_exhaustOxygenText;
    std::string m_thermalInputText;
};

#endif

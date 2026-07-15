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
    void refreshTemperatureStrings();
    void refreshOperatingPointStrings();
    void refreshThermalPowerStrings();
    void refreshCoolingStrings();
    void updateTemperatureGauges();

    void renderHeader(const Bounds &bounds);
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
    void renderThermalPowerPanel(const Bounds &bounds);
    void renderCoolingSystemPanel(const Bounds &bounds);
    void renderInfoRow(const Bounds &bounds, const std::string &label, const std::string &value);
    void renderUnavailableState(const Bounds &bounds);

private:
    Simulator *m_simulator;
    Gauge *m_oilTemperatureGauge;
    Gauge *m_pistonTemperatureGauge;
    Gauge *m_cylinderTemperatureGauge;
    EngineConditionState m_state;
    std::uint64_t m_cachedRevision;

    std::string m_oilTemperatureText;
    std::string m_pistonTemperatureText;
    std::string m_cylinderTemperatureText;
    std::string m_throttleText;
    std::string m_intakeAfrText;
    std::string m_exhaustOxygenText;
    std::string m_thermalInputText;
    std::string m_heatInputText;
    std::string m_frictionHeatText;
    std::string m_heatRejectedText;
    std::string m_oilStoragePowerText;
    std::string m_coolantStoragePowerText;
    std::string m_energyResidualText;
    std::string m_coolantStatusText;
    std::string m_sumpStatusText;
    std::string m_airAndFanText;
    std::string m_pumpStatusText;
};

#endif

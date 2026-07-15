#ifndef ATG_ENGINE_SIM_ENGINE_CONDITION_MODEL_H
#define ATG_ENGINE_SIM_ENGINE_CONDITION_MODEL_H

#include "simulation/engine_thermal_model.h"

#include <cstdint>

class Simulator;

struct EngineConditionState {
    std::uint64_t revision = 0;
    bool valid = false;

    bool ignitionEnabled = false;
    bool starterEnabled = false;
    bool dynamometerEnabled = false;
    bool dynamometerHoldEnabled = false;

    double engineSpeedRadPerSecond = 0.0;
    double vehicleSpeedMetersPerSecond = 0.0;
    int transmissionGear = -1;
    double torqueNewtonMeters = 0.0;
    double powerWatts = 0.0;
    double manifoldPressurePascals = 0.0;
    double intakeFlowRate = 0.0;
    double volumetricEfficiency = 0.0;
    double throttlePosition = 0.0;
    double intakeAfr = 0.0;
    double exhaustOxygenFraction = 0.0;

    EngineThermalState thermal;
};

class EngineConditionModel {
public:
    void reset();
    void update(const Simulator &simulator);

    const EngineConditionState &getState() const { return m_state; }

    static double calculateVolumetricEfficiency(
        double intakeFlowRate,
        double engineDisplacementM3,
        double engineSpeedRpm);

private:
    EngineConditionState m_state;
};

#endif

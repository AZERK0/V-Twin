#ifndef ATG_ENGINE_SIM_ENGINE_THERMAL_MODEL_H
#define ATG_ENGINE_SIM_ENGINE_THERMAL_MODEL_H

#include "domain/engine/engine_thermal_parameters.h"

#include <cstdint>
#include <vector>

struct CylinderThermalProperties {
    double pistonMassKg = 0.0;
    double boreM = 0.0;
    double combustionChamberVolumeM3 = 0.0;
};

struct CylinderThermalSample {
    double gasTemperatureK = 0.0;
    double gasPressurePa = 0.0;
    double chamberVolumeM3 = 0.0;
    double meanPistonSpeedMPerS = 0.0;
    double frictionPowerW = 0.0;
};

struct CylinderThermalState {
    double pistonTemperatureK = 0.0;
    double cylinderTemperatureK = 0.0;
};

struct EngineThermalState {
    std::uint64_t revision = 0;
    std::uint64_t inputAnomalyCount = 0;
    bool valid = false;
    double oilTemperatureK = 0.0;
    double averagePistonTemperatureK = 0.0;
    double maximumPistonTemperatureK = 0.0;
    double averageCylinderTemperatureK = 0.0;
    double maximumCylinderTemperatureK = 0.0;
    std::vector<CylinderThermalState> cylinders;
};

class EngineThermalModel {
public:
    bool initialize(
        const EngineThermalParameters &parameters,
        const std::vector<CylinderThermalProperties> &cylinderProperties);
    void reset();
    void update(const std::vector<CylinderThermalSample> &samples, double dt);

    const EngineThermalState &getState() const { return m_state; }

    static double calculateHohenbergHeatTransferCoefficient(
        const EngineThermalParameters &parameters,
        const CylinderThermalSample &sample);

private:
    struct AccumulatedEnergy {
        double pistonGasJ = 0.0;
        double cylinderGasJ = 0.0;
        double frictionJ = 0.0;
    };

    bool propertiesAreValid() const;
    bool updateIntervalIsStable() const;
    bool sampleIsFinite(const CylinderThermalSample &sample) const;
    void accumulateSample(int cylinderIndex, const CylinderThermalSample &sample, double dt);
    void integrateAccumulatedEnergy();
    double calculatePistonPower(int cylinderIndex, double elapsedSeconds) const;
    double calculateCylinderPower(int cylinderIndex, double elapsedSeconds) const;
    double calculateOilPower(double elapsedSeconds) const;
    bool applyTemperatureChanges(const std::vector<double> &pistonPowersW,
        const std::vector<double> &cylinderPowersW, double oilPowerW, double dt);
    bool temperaturesAreValid(
        const std::vector<CylinderThermalState> &cylinderStates,
        double oilTemperatureK) const;
    void publishState();
    void clearAccumulator();
    double calculatePistonGasPower(int cylinderIndex, const CylinderThermalSample &sample) const;
    double calculateCylinderGasPower(int cylinderIndex, const CylinderThermalSample &sample) const;

private:
    EngineThermalParameters m_parameters;
    std::vector<CylinderThermalProperties> m_cylinderProperties;
    std::vector<CylinderThermalState> m_cylinderStates;
    std::vector<AccumulatedEnergy> m_accumulatedEnergy;
    EngineThermalState m_state;
    double m_oilTemperatureK = 0.0;
    double m_accumulatedTimeSeconds = 0.0;
    std::uint64_t m_inputAnomalyCount = 0;
    bool m_initialized = false;
};

#endif

#ifndef ATG_ENGINE_SIM_ENGINE_THERMAL_MODEL_H
#define ATG_ENGINE_SIM_ENGINE_THERMAL_MODEL_H

#include "domain/engine/engine_thermal_parameters.h"
#include "simulation/engine_cooling_model.h"

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

struct EngineThermalPowerBalance {
    double gasToPistonsW = 0.0;
    double gasToCylindersW = 0.0;
    double frictionW = 0.0;
    double frictionToPistonsW = 0.0;
    double frictionToCylindersW = 0.0;
    double frictionToOilW = 0.0;
    double pistonToCylinderW = 0.0;
    double pistonToOilW = 0.0;
    double cylinderToOilW = 0.0;
    double cylinderToCoolantW = 0.0;
    double oilToSumpW = 0.0;
    double oilToCoolantW = 0.0;
    double cylinderToAmbientW = 0.0;
    double oilToAmbientW = 0.0;
    double sumpToAmbientW = 0.0;
    double oilCoolerToAmbientW = 0.0;
    double radiatorToAmbientW = 0.0;
    double pistonStorageW = 0.0;
    double cylinderStorageW = 0.0;
    double oilStorageW = 0.0;
    double sumpStorageW = 0.0;
    double coolantStorageW = 0.0;
    double ambientRejectionW = 0.0;
    double energyResidualW = 0.0;
    double relativeEnergyResidual = 0.0;
};

struct EngineThermalState {
    std::uint64_t revision = 0;
    std::uint64_t inputAnomalyCount = 0;
    bool valid = false;
    double oilTemperatureK = 0.0;
    double sumpTemperatureK = 0.0;
    double coolantTemperatureK = 0.0;
    bool sumpModeled = false;
    bool coolantModeled = false;
    double averagePistonTemperatureK = 0.0;
    double maximumPistonTemperatureK = 0.0;
    double averageCylinderTemperatureK = 0.0;
    double maximumCylinderTemperatureK = 0.0;
    EngineCoolingState cooling;
    EngineThermalPowerBalance powerBalance;
    std::vector<CylinderThermalState> cylinders;
};

class EngineThermalModel {
public:
    bool initialize(
        const EngineThermalParameters &parameters,
        const std::vector<CylinderThermalProperties> &cylinderProperties);
    void reset();
    void update(const std::vector<CylinderThermalSample> &samples, double dt);
    void update(
        const std::vector<CylinderThermalSample> &samples,
        const EngineThermalOperatingPoint &operatingPoint,
        double dt);

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

    struct CylinderPowerFlow {
        double gasToPistonW = 0.0;
        double gasToCylinderW = 0.0;
        double frictionW = 0.0;
        double pistonToCylinderW = 0.0;
        double pistonToOilW = 0.0;
        double cylinderToOilW = 0.0;
        double cylinderToAmbientW = 0.0;
        double cylinderToCoolantW = 0.0;
        double pistonStorageW = 0.0;
        double cylinderStorageW = 0.0;
    };

    struct PowerSolution {
        std::vector<CylinderPowerFlow> cylinders;
        EngineCoolingState cooling;
        EngineThermalPowerBalance balance;
        double oilStorageW = 0.0;
        double sumpStorageW = 0.0;
        double coolantStorageW = 0.0;
    };

    struct FluidTemperatures {
        double oilK = 0.0;
        double sumpK = 0.0;
        double coolantK = 0.0;
    };

    bool propertiesAreValid() const;
    bool updateIntervalIsStable() const;
    EngineCoolingState calculateMaximumCoolingState() const;
    double calculateMinimumPistonCapacityJPerK() const;
    bool baseNodesAreStable(const EngineCoolingState &cooling) const;
    bool extendedNodesAreStable(const EngineCoolingState &cooling) const;
    bool sampleIsFinite(const CylinderThermalSample &sample) const;
    bool operatingPointIsFinite(const EngineThermalOperatingPoint &operatingPoint) const;
    void accumulateSample(int cylinderIndex, const CylinderThermalSample &sample, double dt);
    void accumulateOperatingPoint(const EngineThermalOperatingPoint &operatingPoint, double dt);
    void integrateAccumulatedEnergy();
    EngineThermalOperatingPoint calculateAverageOperatingPoint() const;
    PowerSolution calculatePowerSolution(double elapsedSeconds) const;
    CylinderPowerFlow calculateCylinderPowerFlow(
        int cylinderIndex,
        double elapsedSeconds,
        const EngineCoolingState &cooling) const;
    void calculateCylinderStoragePowers(CylinderPowerFlow &flow) const;
    void calculateFluidPowerFlows(PowerSolution &solution) const;
    double calculateOilStoragePower(PowerSolution &solution) const;
    double calculateSumpStoragePower(PowerSolution &solution) const;
    double calculateCoolantStoragePower(PowerSolution &solution) const;
    void addCylinderFlowToBalance(
        const CylinderPowerFlow &flow,
        EngineThermalPowerBalance &balance) const;
    void populatePowerBalance(PowerSolution &solution) const;
    bool applyTemperatureChanges(const PowerSolution &solution, double dt);
    std::vector<CylinderThermalState> calculateNextCylinderStates(
        const PowerSolution &solution,
        double dt) const;
    FluidTemperatures calculateNextFluidTemperatures(
        const PowerSolution &solution,
        double dt) const;
    bool temperaturesAreValid(
        const std::vector<CylinderThermalState> &cylinderStates,
        double oilTemperatureK,
        double sumpTemperatureK,
        double coolantTemperatureK) const;
    bool energyBalanceIsValid(const EngineThermalPowerBalance &balance) const;
    void publishState();
    void publishAverageTemperatures();
    void publishMaximumTemperatures();
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
    double m_sumpTemperatureK = 0.0;
    double m_coolantTemperatureK = 0.0;
    double m_accumulatedTimeSeconds = 0.0;
    double m_accumulatedVehicleSpeedMeters = 0.0;
    double m_accumulatedEngineAngleRadians = 0.0;
    EngineCoolingState m_lastCoolingState;
    EngineThermalPowerBalance m_lastPowerBalance;
    std::uint64_t m_inputAnomalyCount = 0;
    bool m_initialized = false;
};

#endif

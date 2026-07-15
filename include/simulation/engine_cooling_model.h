#ifndef ATG_ENGINE_SIM_ENGINE_COOLING_MODEL_H
#define ATG_ENGINE_SIM_ENGINE_COOLING_MODEL_H

#include "domain/engine/engine_thermal_parameters.h"

struct EngineThermalOperatingPoint {
    double vehicleSpeedMPerS = 0.0;
    double engineSpeedRadPerSecond = 0.0;
};

struct EngineCoolingState {
    double vehicleSpeedMPerS = 0.0;
    double engineSpeedRadPerSecond = 0.0;
    double effectiveRadiatorAirSpeedMPerS = 0.0;
    double coolantThermostatOpening = 0.0;
    double oilThermostatOpening = 0.0;
    double fanDuty = 0.0;
    double coolantPumpFactor = 0.0;
    double oilPumpFactor = 0.0;
    double cylinderAmbientConductanceWPerK = 0.0;
    double cylinderCoolantConductanceWPerK = 0.0;
    double oilCoolantConductanceWPerK = 0.0;
    double sumpAmbientConductanceWPerK = 0.0;
    double oilCoolerAmbientConductanceWPerK = 0.0;
    double radiatorAmbientConductanceWPerK = 0.0;
};

class EngineCoolingModel {
public:
    static EngineCoolingState calculate(
        const EngineThermalParameters &parameters,
        const EngineThermalOperatingPoint &operatingPoint,
        double oilTemperatureK,
        double coolantTemperatureK);
    static double calculateAirConductance(
        const EngineCoolingParameters &parameters,
        double naturalConductanceWPerK,
        double forcedConductanceAtReferenceWPerK,
        double airSpeedMPerS);
    static double calculateThermostatOpening(
        double temperatureK,
        double startTemperatureK,
        double fullOpenTemperatureK);
    static double calculatePumpFactor(
        double engineSpeedRadPerSecond,
        double referenceSpeedRadPerSecond,
        double exponent,
        double minimumFactor,
        double maximumFactor);
};

#endif

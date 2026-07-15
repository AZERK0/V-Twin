#ifndef ATG_ENGINE_SIM_ENGINE_COOLING_PARAMETERS_H
#define ATG_ENGINE_SIM_ENGINE_COOLING_PARAMETERS_H

#include "units.h"

#include <cmath>

struct EngineCoolingParameters {
    double airReferenceSpeedMPerS = 30.0;
    double airSpeedExponent = 0.6;
    double maximumAirSpeedMPerS = 70.0;
    double cylinderForcedAirConductanceAtReferenceWPerK = 0.0;

    double sumpThermalCapacityJPerK = 0.0;
    double oilSumpConductanceWPerK = 0.0;
    double sumpNaturalAirConductanceWPerK = 0.0;
    double sumpForcedAirConductanceAtReferenceWPerK = 0.0;

    double oilCoolerNaturalAirConductanceWPerK = 0.0;
    double oilCoolerForcedAirConductanceAtReferenceWPerK = 0.0;
    double oilThermostatStartTemperatureK = units::celcius(80.0);
    double oilThermostatFullOpenTemperatureK = units::celcius(90.0);

    double coolantThermalCapacityJPerK = 0.0;
    double cylinderCoolantStaticConductanceWPerK = 0.0;
    double cylinderCoolantPumpedConductanceAtReferenceWPerK = 0.0;
    double oilCoolantStaticConductanceWPerK = 0.0;
    double oilCoolantPumpedConductanceAtReferenceWPerK = 0.0;
    double radiatorNaturalAirConductanceWPerK = 0.0;
    double radiatorForcedAirConductanceAtReferenceWPerK = 0.0;
    double radiatorRamAirSpeedFactor = 0.85;
    double coolantThermostatStartTemperatureK = units::celcius(82.0);
    double coolantThermostatFullOpenTemperatureK = units::celcius(95.0);
    double fanTurnOnTemperatureK = units::celcius(95.0);
    double fanFullSpeedTemperatureK = units::celcius(105.0);
    double fanMaximumAirSpeedMPerS = 8.0;

    double coolantPumpReferenceSpeedRadPerSecond = units::rpm(3000.0);
    double oilPumpReferenceSpeedRadPerSecond = units::rpm(3000.0);
    double pumpSpeedExponent = 0.8;
    double coolantPumpMinimumFactor = 0.0;
    double coolantPumpMaximumFactor = 1.5;
    double oilPumpMinimumFactor = 0.0;
    double oilPumpMaximumFactor = 1.5;

    bool hasSump() const {
        return sumpThermalCapacityJPerK > 0.0;
    }

    bool hasCoolant() const {
        return coolantThermalCapacityJPerK > 0.0;
    }

    bool isValid() const {
        return airParametersAreValid()
            && sumpParametersAreValid()
            && oilCoolerParametersAreValid()
            && coolantParametersAreValid()
            && pumpParametersAreValid();
    }

private:
    static bool isPositive(double value) {
        return std::isfinite(value) && value > 0.0;
    }

    static bool isNonNegative(double value) {
        return std::isfinite(value) && value >= 0.0;
    }

    static bool orderedTemperatures(double start, double end) {
        return isPositive(start) && isPositive(end) && end > start;
    }

    bool airParametersAreValid() const {
        return isPositive(airReferenceSpeedMPerS)
            && isPositive(airSpeedExponent)
            && isPositive(maximumAirSpeedMPerS)
            && maximumAirSpeedMPerS >= airReferenceSpeedMPerS
            && isNonNegative(cylinderForcedAirConductanceAtReferenceWPerK);
    }

    bool sumpParametersAreValid() const {
        const bool pathsRequireSump = oilSumpConductanceWPerK > 0.0
            || sumpNaturalAirConductanceWPerK > 0.0
            || sumpForcedAirConductanceAtReferenceWPerK > 0.0;
        return isNonNegative(sumpThermalCapacityJPerK)
            && isNonNegative(oilSumpConductanceWPerK)
            && isNonNegative(sumpNaturalAirConductanceWPerK)
            && isNonNegative(sumpForcedAirConductanceAtReferenceWPerK)
            && (!pathsRequireSump || hasSump());
    }

    bool oilCoolerParametersAreValid() const {
        return isNonNegative(oilCoolerNaturalAirConductanceWPerK)
            && isNonNegative(oilCoolerForcedAirConductanceAtReferenceWPerK)
            && orderedTemperatures(
                oilThermostatStartTemperatureK,
                oilThermostatFullOpenTemperatureK);
    }

    bool coolantParametersAreValid() const {
        const bool pathsRequireCoolant = cylinderCoolantStaticConductanceWPerK > 0.0
            || cylinderCoolantPumpedConductanceAtReferenceWPerK > 0.0
            || oilCoolantStaticConductanceWPerK > 0.0
            || oilCoolantPumpedConductanceAtReferenceWPerK > 0.0
            || radiatorNaturalAirConductanceWPerK > 0.0
            || radiatorForcedAirConductanceAtReferenceWPerK > 0.0;
        return isNonNegative(coolantThermalCapacityJPerK)
            && coolantConductancesAreValid()
            && isNonNegative(radiatorRamAirSpeedFactor)
            && isNonNegative(fanMaximumAirSpeedMPerS)
            && orderedTemperatures(
                coolantThermostatStartTemperatureK,
                coolantThermostatFullOpenTemperatureK)
            && orderedTemperatures(fanTurnOnTemperatureK, fanFullSpeedTemperatureK)
            && (!pathsRequireCoolant || hasCoolant());
    }

    bool coolantConductancesAreValid() const {
        return isNonNegative(cylinderCoolantStaticConductanceWPerK)
            && isNonNegative(cylinderCoolantPumpedConductanceAtReferenceWPerK)
            && isNonNegative(oilCoolantStaticConductanceWPerK)
            && isNonNegative(oilCoolantPumpedConductanceAtReferenceWPerK)
            && isNonNegative(radiatorNaturalAirConductanceWPerK)
            && isNonNegative(radiatorForcedAirConductanceAtReferenceWPerK);
    }

    bool pumpParametersAreValid() const {
        return isPositive(coolantPumpReferenceSpeedRadPerSecond)
            && isPositive(oilPumpReferenceSpeedRadPerSecond)
            && isPositive(pumpSpeedExponent)
            && isNonNegative(coolantPumpMinimumFactor)
            && isNonNegative(coolantPumpMaximumFactor)
            && isNonNegative(oilPumpMinimumFactor)
            && isNonNegative(oilPumpMaximumFactor)
            && coolantPumpMaximumFactor >= coolantPumpMinimumFactor
            && oilPumpMaximumFactor >= oilPumpMinimumFactor;
    }
};

#endif

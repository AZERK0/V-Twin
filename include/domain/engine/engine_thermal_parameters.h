#ifndef ATG_ENGINE_SIM_ENGINE_THERMAL_PARAMETERS_H
#define ATG_ENGINE_SIM_ENGINE_THERMAL_PARAMETERS_H

#include "units.h"

#include <cmath>

struct EngineThermalParameters {
    double ambientTemperatureK = units::celcius(25.0);
    double pistonSpecificHeatJPerKgK = 900.0;
    double cylinderThermalCapacityJPerK = 2000.0;
    double oilVolumeM3 = units::volume(3.0, units::L);
    double oilDensityKgPerM3 = 850.0;
    double oilSpecificHeatJPerKgK = 2000.0;
    double pistonCylinderConductanceWPerK = 6.0;
    double pistonOilConductanceWPerK = 8.0;
    double cylinderOilConductanceWPerK = 10.0;
    double cylinderAmbientConductanceWPerK = 10.0;
    double oilAmbientConductanceWPerK = 5.0;
    double pistonFrictionHeatFraction = 0.35;
    double cylinderFrictionHeatFraction = 0.45;
    double oilFrictionHeatFraction = 0.20;
    double pistonGasSurfaceFactor = 1.0;
    double cylinderGasSurfaceFactor = 1.0;
    double pistonGasHeatTransferFactor = 1.0;
    double cylinderGasHeatTransferFactor = 1.0;
    double hohenbergCoefficient = 130.0;
    double maximumHeatTransferCoefficientWPerM2K = 5000.0;
    double updateIntervalSeconds = 0.02;

    bool isValid() const {
        const double frictionFractionSum = pistonFrictionHeatFraction
            + cylinderFrictionHeatFraction
            + oilFrictionHeatFraction;
        return isPositive(ambientTemperatureK)
            && isPositive(pistonSpecificHeatJPerKgK)
            && isPositive(cylinderThermalCapacityJPerK)
            && isPositive(oilVolumeM3)
            && isPositive(oilDensityKgPerM3)
            && isPositive(oilSpecificHeatJPerKgK)
            && conductancesAreValid()
            && heatTransferParametersAreValid()
            && std::abs(frictionFractionSum - 1.0) <= 1e-9;
    }

    double oilThermalCapacityJPerK() const {
        return oilVolumeM3 * oilDensityKgPerM3 * oilSpecificHeatJPerKgK;
    }

private:
    static bool isPositive(double value) {
        return std::isfinite(value) && value > 0.0;
    }

    static bool isNonNegative(double value) {
        return std::isfinite(value) && value >= 0.0;
    }

    bool conductancesAreValid() const {
        return isNonNegative(pistonCylinderConductanceWPerK)
            && isNonNegative(pistonOilConductanceWPerK)
            && isNonNegative(cylinderOilConductanceWPerK)
            && isNonNegative(cylinderAmbientConductanceWPerK)
            && isNonNegative(oilAmbientConductanceWPerK);
    }

    bool heatTransferParametersAreValid() const {
        return isNonNegative(pistonFrictionHeatFraction)
            && isNonNegative(cylinderFrictionHeatFraction)
            && isNonNegative(oilFrictionHeatFraction)
            && isNonNegative(pistonGasSurfaceFactor)
            && isNonNegative(cylinderGasSurfaceFactor)
            && isNonNegative(pistonGasHeatTransferFactor)
            && isNonNegative(cylinderGasHeatTransferFactor)
            && isPositive(hohenbergCoefficient)
            && isPositive(maximumHeatTransferCoefficientWPerM2K)
            && isPositive(updateIntervalSeconds);
    }
};

#endif

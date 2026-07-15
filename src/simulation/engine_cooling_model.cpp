#include "simulation/engine_cooling_model.h"

#include <algorithm>
#include <cmath>

namespace {
    double smoothStep(double value) {
        const double normalized = std::clamp(value, 0.0, 1.0);
        return normalized * normalized * (3.0 - 2.0 * normalized);
    }

    double calculateRadiatorAirSpeed(
        const EngineCoolingParameters &parameters,
        double vehicleSpeedMPerS,
        double fanDuty)
    {
        const double ramAirSpeed = parameters.radiatorRamAirSpeedFactor
            * std::abs(vehicleSpeedMPerS);
        const double fanAirSpeed = fanDuty * parameters.fanMaximumAirSpeedMPerS;
        return std::hypot(ramAirSpeed, fanAirSpeed);
    }

    void calculateThermalControls(
        const EngineCoolingParameters &parameters,
        double oilTemperatureK,
        double coolantTemperatureK,
        EngineCoolingState &state)
    {
        state.oilThermostatOpening = EngineCoolingModel::calculateThermostatOpening(
            oilTemperatureK,
            parameters.oilThermostatStartTemperatureK,
            parameters.oilThermostatFullOpenTemperatureK);
        state.coolantThermostatOpening = EngineCoolingModel::calculateThermostatOpening(
            coolantTemperatureK,
            parameters.coolantThermostatStartTemperatureK,
            parameters.coolantThermostatFullOpenTemperatureK);
        state.fanDuty = EngineCoolingModel::calculateThermostatOpening(
            coolantTemperatureK,
            parameters.fanTurnOnTemperatureK,
            parameters.fanFullSpeedTemperatureK);
    }

    void calculatePumpControls(
        const EngineCoolingParameters &parameters,
        double engineSpeedRadPerSecond,
        EngineCoolingState &state)
    {
        state.coolantPumpFactor = EngineCoolingModel::calculatePumpFactor(
            engineSpeedRadPerSecond,
            parameters.coolantPumpReferenceSpeedRadPerSecond,
            parameters.pumpSpeedExponent,
            parameters.coolantPumpMinimumFactor,
            parameters.coolantPumpMaximumFactor);
        state.oilPumpFactor = EngineCoolingModel::calculatePumpFactor(
            engineSpeedRadPerSecond,
            parameters.oilPumpReferenceSpeedRadPerSecond,
            parameters.pumpSpeedExponent,
            parameters.oilPumpMinimumFactor,
            parameters.oilPumpMaximumFactor);
    }

    void calculateAirPaths(
        const EngineThermalParameters &parameters,
        double vehicleAirSpeed,
        EngineCoolingState &state)
    {
        const EngineCoolingParameters &cooling = parameters.cooling;
        state.cylinderAmbientConductanceWPerK = EngineCoolingModel::calculateAirConductance(
            cooling,
            parameters.cylinderAmbientConductanceWPerK,
            cooling.cylinderForcedAirConductanceAtReferenceWPerK,
            vehicleAirSpeed);
        state.sumpAmbientConductanceWPerK = EngineCoolingModel::calculateAirConductance(
            cooling,
            cooling.sumpNaturalAirConductanceWPerK,
            cooling.sumpForcedAirConductanceAtReferenceWPerK,
            vehicleAirSpeed);
        state.oilCoolerAmbientConductanceWPerK = state.oilThermostatOpening
            * EngineCoolingModel::calculateAirConductance(
                cooling,
                cooling.oilCoolerNaturalAirConductanceWPerK,
                cooling.oilCoolerForcedAirConductanceAtReferenceWPerK,
                vehicleAirSpeed);
    }

    void calculateCoolantPaths(
        const EngineCoolingParameters &parameters,
        EngineCoolingState &state)
    {
        state.radiatorAmbientConductanceWPerK = state.coolantThermostatOpening
            * EngineCoolingModel::calculateAirConductance(
                parameters,
                parameters.radiatorNaturalAirConductanceWPerK,
                parameters.radiatorForcedAirConductanceAtReferenceWPerK,
                state.effectiveRadiatorAirSpeedMPerS);
        if (!parameters.hasCoolant()) {
            return;
        }
        state.cylinderCoolantConductanceWPerK =
            parameters.cylinderCoolantStaticConductanceWPerK
            + parameters.cylinderCoolantPumpedConductanceAtReferenceWPerK
                * state.coolantPumpFactor;
        state.oilCoolantConductanceWPerK = parameters.oilCoolantStaticConductanceWPerK
            + parameters.oilCoolantPumpedConductanceAtReferenceWPerK
                * std::sqrt(state.coolantPumpFactor * state.oilPumpFactor);
    }
}

EngineCoolingState EngineCoolingModel::calculate(
    const EngineThermalParameters &parameters,
    const EngineThermalOperatingPoint &operatingPoint,
    double oilTemperatureK,
    double coolantTemperatureK)
{
    const EngineCoolingParameters &cooling = parameters.cooling;
    EngineCoolingState state;
    state.vehicleSpeedMPerS = operatingPoint.vehicleSpeedMPerS;
    state.engineSpeedRadPerSecond = operatingPoint.engineSpeedRadPerSecond;
    calculateThermalControls(cooling, oilTemperatureK, coolantTemperatureK, state);
    state.effectiveRadiatorAirSpeedMPerS = calculateRadiatorAirSpeed(
        cooling,
        operatingPoint.vehicleSpeedMPerS,
        state.fanDuty);
    calculatePumpControls(cooling, operatingPoint.engineSpeedRadPerSecond, state);
    const double vehicleAirSpeed = std::abs(operatingPoint.vehicleSpeedMPerS);
    calculateAirPaths(parameters, vehicleAirSpeed, state);
    calculateCoolantPaths(cooling, state);
    return state;
}

double EngineCoolingModel::calculateAirConductance(
    const EngineCoolingParameters &parameters,
    double naturalConductanceWPerK,
    double forcedConductanceAtReferenceWPerK,
    double airSpeedMPerS)
{
    const double boundedSpeed = std::clamp(
        std::abs(airSpeedMPerS),
        0.0,
        parameters.maximumAirSpeedMPerS);
    const double forcedFactor = std::pow(
        boundedSpeed / parameters.airReferenceSpeedMPerS,
        parameters.airSpeedExponent);
    return naturalConductanceWPerK
        + forcedConductanceAtReferenceWPerK * forcedFactor;
}

double EngineCoolingModel::calculateThermostatOpening(
    double temperatureK,
    double startTemperatureK,
    double fullOpenTemperatureK)
{
    return smoothStep(
        (temperatureK - startTemperatureK)
        / (fullOpenTemperatureK - startTemperatureK));
}

double EngineCoolingModel::calculatePumpFactor(
    double engineSpeedRadPerSecond,
    double referenceSpeedRadPerSecond,
    double exponent,
    double minimumFactor,
    double maximumFactor)
{
    const double speedRatio = std::abs(engineSpeedRadPerSecond)
        / referenceSpeedRadPerSecond;
    return std::clamp(
        std::pow(speedRatio, exponent),
        minimumFactor,
        maximumFactor);
}

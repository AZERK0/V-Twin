#include "simulation/engine_cooling_model.h"
#include "simulation/engine_thermal_model.h"

#include "units.h"

#include <gtest/gtest.h>

namespace {
    EngineThermalParameters createCoolingParameters() {
        EngineThermalParameters parameters;
        parameters.cylinderAmbientConductanceWPerK = 0.0;
        parameters.oilAmbientConductanceWPerK = 0.0;
        auto &cooling = parameters.cooling;
        cooling.sumpThermalCapacityJPerK = 4000.0;
        cooling.oilSumpConductanceWPerK = 40.0;
        cooling.sumpNaturalAirConductanceWPerK = 5.0;
        cooling.sumpForcedAirConductanceAtReferenceWPerK = 25.0;
        cooling.oilCoolerNaturalAirConductanceWPerK = 2.0;
        cooling.oilCoolerForcedAirConductanceAtReferenceWPerK = 40.0;
        cooling.coolantThermalCapacityJPerK = 30000.0;
        cooling.cylinderCoolantStaticConductanceWPerK = 5.0;
        cooling.cylinderCoolantPumpedConductanceAtReferenceWPerK = 80.0;
        cooling.oilCoolantStaticConductanceWPerK = 2.0;
        cooling.oilCoolantPumpedConductanceAtReferenceWPerK = 30.0;
        cooling.radiatorNaturalAirConductanceWPerK = 10.0;
        cooling.radiatorForcedAirConductanceAtReferenceWPerK = 300.0;
        return parameters;
    }

    CylinderThermalProperties createCylinderProperties() {
        CylinderThermalProperties properties;
        properties.pistonMassKg = 0.5;
        properties.boreM = units::distance(86.0, units::mm);
        properties.combustionChamberVolumeM3 = units::volume(50.0, units::cc);
        return properties;
    }

    CylinderThermalSample createLoadedSample() {
        CylinderThermalSample sample;
        sample.gasTemperatureK = 900.0;
        sample.gasPressurePa = units::pressure(5.0, units::bar);
        sample.chamberVolumeM3 = units::volume(250.0, units::cc);
        sample.meanPistonSpeedMPerS = 12.0;
        sample.frictionPowerW = 1000.0;
        return sample;
    }
}

TEST(EngineCoolingModelTests, ScalesAirConductanceAtReferenceSpeed) {
    EngineCoolingParameters parameters;

    const double stopped = EngineCoolingModel::calculateAirConductance(
        parameters, 5.0, 25.0, 0.0);
    const double reference = EngineCoolingModel::calculateAirConductance(
        parameters, 5.0, 25.0, parameters.airReferenceSpeedMPerS);

    EXPECT_DOUBLE_EQ(stopped, 5.0);
    EXPECT_NEAR(reference, 30.0, 1e-12);
}

TEST(EngineCoolingModelTests, CapsAirConductanceOutsideCalibrationDomain) {
    EngineCoolingParameters parameters;

    const double atMaximum = EngineCoolingModel::calculateAirConductance(
        parameters, 5.0, 25.0, parameters.maximumAirSpeedMPerS);
    const double beyondMaximum = EngineCoolingModel::calculateAirConductance(
        parameters, 5.0, 25.0, parameters.maximumAirSpeedMPerS * 10.0);

    EXPECT_DOUBLE_EQ(atMaximum, beyondMaximum);
}

TEST(EngineCoolingModelTests, OpensThermostatContinuously) {
    const double start = units::celcius(80.0);
    const double end = units::celcius(90.0);

    EXPECT_DOUBLE_EQ(EngineCoolingModel::calculateThermostatOpening(start - 1.0, start, end), 0.0);
    EXPECT_DOUBLE_EQ(EngineCoolingModel::calculateThermostatOpening((start + end) / 2.0, start, end), 0.5);
    EXPECT_DOUBLE_EQ(EngineCoolingModel::calculateThermostatOpening(end + 1.0, start, end), 1.0);
}

TEST(EngineCoolingModelTests, BoundsPumpFactorAcrossEngineSpeed) {
    const double referenceSpeed = units::rpm(3000.0);

    EXPECT_DOUBLE_EQ(EngineCoolingModel::calculatePumpFactor(0.0, referenceSpeed, 0.8, 0.1, 1.5), 0.1);
    EXPECT_NEAR(EngineCoolingModel::calculatePumpFactor(referenceSpeed, referenceSpeed, 0.8, 0.1, 1.5), 1.0, 1e-12);
    EXPECT_DOUBLE_EQ(EngineCoolingModel::calculatePumpFactor(referenceSpeed * 100.0, referenceSpeed, 0.8, 0.1, 1.5), 1.5);
}

TEST(EngineCoolingModelTests, VehicleSpeedAndFanIncreaseCoolingPaths) {
    const EngineThermalParameters parameters = createCoolingParameters();
    const EngineThermalOperatingPoint stopped { 0.0, units::rpm(3000.0) };
    const EngineThermalOperatingPoint moving { 30.0, units::rpm(3000.0) };
    const double hotOilK = units::celcius(110.0);
    const double hotCoolantK = units::celcius(110.0);

    const EngineCoolingState stoppedState = EngineCoolingModel::calculate(
        parameters, stopped, hotOilK, hotCoolantK);
    const EngineCoolingState movingState = EngineCoolingModel::calculate(
        parameters, moving, hotOilK, hotCoolantK);

    EXPECT_GT(movingState.sumpAmbientConductanceWPerK, stoppedState.sumpAmbientConductanceWPerK);
    EXPECT_GT(movingState.oilCoolerAmbientConductanceWPerK, stoppedState.oilCoolerAmbientConductanceWPerK);
    EXPECT_GT(movingState.radiatorAmbientConductanceWPerK, stoppedState.radiatorAmbientConductanceWPerK);
    EXPECT_DOUBLE_EQ(stoppedState.fanDuty, 1.0);
}

TEST(EngineCoolingModelTests, PublishesConservativeExtendedPowerBalance) {
    EngineThermalModel model;
    const EngineThermalParameters parameters = createCoolingParameters();
    ASSERT_TRUE(model.initialize(parameters, { createCylinderProperties() }));
    const EngineThermalOperatingPoint operatingPoint { 30.0, units::rpm(3000.0) };

    model.update(
        { createLoadedSample() },
        operatingPoint,
        parameters.updateIntervalSeconds);

    const EngineThermalState &state = model.getState();
    EXPECT_TRUE(state.sumpModeled);
    EXPECT_TRUE(state.coolantModeled);
    EXPECT_GT(state.powerBalance.gasToPistonsW, 0.0);
    EXPECT_GT(state.powerBalance.frictionW, 0.0);
    EXPECT_LT(state.powerBalance.relativeEnergyResidual, 1e-12);
}

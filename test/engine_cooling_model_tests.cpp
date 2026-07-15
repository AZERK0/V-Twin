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

    void configure2JzOilCooling(EngineCoolingParameters &cooling) {
        cooling.sumpThermalCapacityJPerK = 6000.0;
        cooling.oilSumpConductanceWPerK = 90.0;
        cooling.sumpNaturalAirConductanceWPerK = 4.0;
        cooling.sumpForcedAirConductanceAtReferenceWPerK = 25.0;
        cooling.oilCoolerForcedAirConductanceAtReferenceWPerK = 35.0;
        cooling.oilThermostatStartTemperatureK = units::celcius(90.0);
        cooling.oilThermostatFullOpenTemperatureK = units::celcius(105.0);
    }

    void configure2JzCoolant(EngineCoolingParameters &cooling) {
        cooling.coolantThermalCapacityJPerK = 32000.0;
        cooling.cylinderCoolantStaticConductanceWPerK = 10.0;
        cooling.cylinderCoolantPumpedConductanceAtReferenceWPerK = 140.0;
        cooling.oilCoolantStaticConductanceWPerK = 5.0;
        cooling.oilCoolantPumpedConductanceAtReferenceWPerK = 100.0;
        cooling.radiatorNaturalAirConductanceWPerK = 20.0;
        cooling.radiatorForcedAirConductanceAtReferenceWPerK = 650.0;
        cooling.coolantThermostatFullOpenTemperatureK = units::celcius(92.0);
        cooling.fanTurnOnTemperatureK = units::celcius(94.0);
        cooling.fanFullSpeedTemperatureK = units::celcius(102.0);
    }

    EngineThermalParameters create2JzThermalParameters() {
        EngineThermalParameters parameters;
        parameters.cylinderThermalCapacityJPerK = 18000.0;
        parameters.oilVolumeM3 = units::volume(5.0, units::L);
        parameters.pistonCylinderConductanceWPerK = 18.0;
        parameters.pistonOilConductanceWPerK = 12.0;
        parameters.cylinderOilConductanceWPerK = 8.0;
        parameters.cylinderAmbientConductanceWPerK = 0.0;
        parameters.oilAmbientConductanceWPerK = 0.0;
        parameters.pistonGasHeatTransferFactor = 0.45;
        parameters.cylinderGasHeatTransferFactor = 0.35;
        configure2JzOilCooling(parameters.cooling);
        configure2JzCoolant(parameters.cooling);
        return parameters;
    }

    CylinderThermalSample create2JzHighLoadSample() {
        CylinderThermalSample sample;
        sample.gasTemperatureK = 1400.0;
        sample.gasPressurePa = units::pressure(7.0, units::bar);
        sample.chamberVolumeM3 = units::volume(300.0, units::cc);
        sample.meanPistonSpeedMPerS = 18.6;
        sample.frictionPowerW = 3700.0;
        return sample;
    }

    CylinderThermalSample createOperatingSample(
        double gasTemperatureK,
        double gasPressureBar,
        double chamberVolumeCc,
        double meanPistonSpeedMPerS,
        double frictionPowerW)
    {
        CylinderThermalSample sample;
        sample.gasTemperatureK = gasTemperatureK;
        sample.gasPressurePa = units::pressure(gasPressureBar, units::bar);
        sample.chamberVolumeM3 = units::volume(chamberVolumeCc, units::cc);
        sample.meanPistonSpeedMPerS = meanPistonSpeedMPerS;
        sample.frictionPowerW = frictionPowerW;
        return sample;
    }

    EngineThermalModel run2JzScenario(
        const CylinderThermalSample &sample,
        double vehicleSpeedMPerS,
        double engineSpeedRpm,
        double durationSeconds)
    {
        const EngineThermalParameters parameters = create2JzThermalParameters();
        const CylinderThermalProperties cylinder {
            0.25,
            units::distance(86.0, units::mm),
            units::volume(50.0, units::cc)
        };
        EngineThermalModel model;
        EXPECT_TRUE(model.initialize(parameters, std::vector<CylinderThermalProperties>(6, cylinder)));
        const std::vector<CylinderThermalSample> samples(6, sample);
        const EngineThermalOperatingPoint operatingPoint {
            vehicleSpeedMPerS,
            units::rpm(engineSpeedRpm)
        };
        const int updateCount = static_cast<int>(durationSeconds / parameters.updateIntervalSeconds);
        for (int i = 0; i < updateCount; ++i) {
            model.update(samples, operatingPoint, parameters.updateIntervalSeconds);
        }
        return model;
    }

    EngineThermalModel run2JzHighLoad(double vehicleSpeedMPerS, double durationSeconds) {
        return run2JzScenario(
            create2JzHighLoadSample(), vehicleSpeedMPerS, 6500.0, durationSeconds);
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

TEST(EngineCoolingModelTests, KeepsLegacyNetworkIndependentFromVehicleSpeed) {
    const EngineThermalParameters parameters;
    EngineThermalModel stopped;
    EngineThermalModel moving;
    ASSERT_TRUE(stopped.initialize(parameters, { createCylinderProperties() }));
    ASSERT_TRUE(moving.initialize(parameters, { createCylinderProperties() }));
    const CylinderThermalSample sample = createLoadedSample();
    stopped.update({ sample }, { 0.0, units::rpm(3000.0) }, parameters.updateIntervalSeconds);
    moving.update({ sample }, { 60.0, units::rpm(3000.0) }, parameters.updateIntervalSeconds);

    EXPECT_DOUBLE_EQ(stopped.getState().oilTemperatureK, moving.getState().oilTemperatureK);
    EXPECT_DOUBLE_EQ(
        stopped.getState().averagePistonTemperatureK,
        moving.getState().averagePistonTemperatureK);
    EXPECT_DOUBLE_EQ(
        stopped.getState().averageCylinderTemperatureK,
        moving.getState().averageCylinderTemperatureK);
}

TEST(EngineCoolingModelTests, Keeps2JzOilBoundedDuringHighSpeedHighLoadRun) {
    const EngineThermalModel model = run2JzHighLoad(47.0, 60.0);
    const EngineThermalState &state = model.getState();

    EXPECT_GT(state.oilTemperatureK, units::celcius(25.0));
    EXPECT_LT(state.oilTemperatureK, units::celcius(120.0));
    EXPECT_LT(state.coolantTemperatureK, units::celcius(110.0));
    EXPECT_GT(state.averagePistonTemperatureK, state.averageCylinderTemperatureK);
    EXPECT_GT(state.cooling.sumpAmbientConductanceWPerK, 30.0);
    EXPECT_DOUBLE_EQ(state.cooling.coolantPumpFactor, 1.5);
    EXPECT_LT(state.powerBalance.relativeEnergyResidual, 1e-12);
}

TEST(EngineCoolingModelTests, Keeps2JzIdleAndCruiseInsideCalibrationEnvelope) {
    const CylinderThermalSample idleSample = createOperatingSample(
        550.0, 1.1, 400.0, 2.6, 300.0);
    const CylinderThermalSample cruiseSample = createOperatingSample(
        900.0, 2.0, 350.0, 7.2, 800.0);
    const EngineThermalState idle = run2JzScenario(
        idleSample, 0.0, 900.0, 600.0).getState();
    const EngineThermalState cruise = run2JzScenario(
        cruiseSample, 27.0, 2500.0, 900.0).getState();

    EXPECT_LT(idle.oilTemperatureK, units::celcius(110.0));
    EXPECT_LT(idle.coolantTemperatureK, units::celcius(110.0));
    EXPECT_LT(cruise.oilTemperatureK, units::celcius(130.0));
    EXPECT_LT(cruise.coolantTemperatureK, units::celcius(105.0));
    EXPECT_GT(cruise.cooling.sumpAmbientConductanceWPerK, idle.cooling.sumpAmbientConductanceWPerK);
}

TEST(EngineCoolingModelTests, UsesPlausibleHighSpeedFrictionCalibrationEnvelope) {
    constexpr double displacementM3 = 0.002998;
    constexpr double engineSpeedRpm = 6500.0;
    constexpr double frictionPowerW = 6.0 * 3700.0;
    const double fmepPa = 120.0 * frictionPowerW
        / (displacementM3 * engineSpeedRpm);

    EXPECT_GT(fmepPa, units::pressure(0.5, units::bar));
    EXPECT_LT(fmepPa, units::pressure(2.0, units::bar));
}

TEST(EngineCoolingModelTests, VehicleAirflowLowers2JzOilTemperatureAtSustainedLoad) {
    const EngineThermalModel stopped = run2JzHighLoad(0.0, 300.0);
    const EngineThermalModel moving = run2JzHighLoad(47.0, 300.0);

    EXPECT_LT(moving.getState().oilTemperatureK, stopped.getState().oilTemperatureK);
    EXPECT_LT(moving.getState().sumpTemperatureK, stopped.getState().sumpTemperatureK);
    EXPECT_LT(moving.getState().coolantTemperatureK, stopped.getState().coolantTemperatureK);
}

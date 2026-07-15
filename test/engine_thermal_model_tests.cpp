#include "simulation/engine_thermal_model.h"

#include "units.h"

#include <gtest/gtest.h>

namespace {
    CylinderThermalProperties createCylinderProperties() {
        CylinderThermalProperties properties;
        properties.pistonMassKg = 0.5;
        properties.boreM = units::distance(3.5, units::inch);
        properties.combustionChamberVolumeM3 = units::volume(100.0, units::cc);
        return properties;
    }

    CylinderThermalSample createAmbientSample(double frictionPowerW = 0.0) {
        CylinderThermalSample sample;
        sample.gasTemperatureK = units::celcius(25.0);
        sample.gasPressurePa = units::pressure(1.0, units::bar);
        sample.chamberVolumeM3 = units::volume(100.0, units::cc);
        sample.meanPistonSpeedMPerS = 0.0;
        sample.frictionPowerW = frictionPowerW;
        return sample;
    }
}

TEST(EngineThermalModelTests, CalculatesDocumentedHohenbergCoefficient) {
    EngineThermalParameters parameters;
    CylinderThermalSample sample;
    sample.gasTemperatureK = 900.0;
    sample.gasPressurePa = units::pressure(5.0, units::bar);
    sample.chamberVolumeM3 = 0.0004350;
    sample.meanPistonSpeedMPerS = 10.795;

    const double coefficient = EngineThermalModel::calculateHohenbergHeatTransferCoefficient(
        parameters,
        sample);

    EXPECT_NEAR(coefficient, 364.8, 0.2);
}

TEST(EngineThermalModelTests, KeepsAllNodesAtAmbientEquilibrium) {
    EngineThermalModel model;
    EngineThermalParameters parameters;
    ASSERT_TRUE(model.initialize(parameters, { createCylinderProperties() }));

    model.update({ createAmbientSample() }, parameters.updateIntervalSeconds);

    const EngineThermalState &state = model.getState();
    EXPECT_DOUBLE_EQ(state.oilTemperatureK, parameters.ambientTemperatureK);
    EXPECT_DOUBLE_EQ(state.cylinders[0].pistonTemperatureK, parameters.ambientTemperatureK);
    EXPECT_DOUBLE_EQ(state.cylinders[0].cylinderTemperatureK, parameters.ambientTemperatureK);
}

TEST(EngineThermalModelTests, ConservesFrictionEnergyAcrossThermalNodes) {
    EngineThermalModel model;
    EngineThermalParameters parameters;
    const CylinderThermalProperties properties = createCylinderProperties();
    ASSERT_TRUE(model.initialize(parameters, { properties }));

    constexpr double frictionPowerW = 1000.0;
    model.update({ createAmbientSample(frictionPowerW) }, parameters.updateIntervalSeconds);

    const EngineThermalState &state = model.getState();
    const double ambientTemperatureK = parameters.ambientTemperatureK;
    const double pistonCapacityJPerK = properties.pistonMassKg * parameters.pistonSpecificHeatJPerKgK;
    const double storedEnergyJ = pistonCapacityJPerK
        * (state.cylinders[0].pistonTemperatureK - ambientTemperatureK)
        + parameters.cylinderThermalCapacityJPerK
            * (state.cylinders[0].cylinderTemperatureK - ambientTemperatureK)
        + parameters.oilThermalCapacityJPerK()
            * (state.oilTemperatureK - ambientTemperatureK);

    EXPECT_NEAR(storedEnergyJ, frictionPowerW * parameters.updateIntervalSeconds, 1e-9);
}

TEST(EngineThermalModelTests, ProducesSymmetricCylinderTemperatures) {
    EngineThermalModel model;
    EngineThermalParameters parameters;
    const CylinderThermalProperties properties = createCylinderProperties();
    ASSERT_TRUE(model.initialize(parameters, { properties, properties }));

    CylinderThermalSample sample = createAmbientSample(300.0);
    sample.gasTemperatureK = 900.0;
    sample.gasPressurePa = units::pressure(5.0, units::bar);
    sample.chamberVolumeM3 = 0.0004350;
    sample.meanPistonSpeedMPerS = 10.795;
    model.update({ sample, sample }, parameters.updateIntervalSeconds);

    const EngineThermalState &state = model.getState();
    EXPECT_DOUBLE_EQ(
        state.cylinders[0].pistonTemperatureK,
        state.cylinders[1].pistonTemperatureK);
    EXPECT_DOUBLE_EQ(
        state.cylinders[0].cylinderTemperatureK,
        state.cylinders[1].cylinderTemperatureK);
}

TEST(EngineThermalModelTests, RejectsInvalidFrictionHeatFractions) {
    EngineThermalModel model;
    EngineThermalParameters parameters;
    parameters.oilFrictionHeatFraction = 0.21;

    EXPECT_FALSE(model.initialize(parameters, { createCylinderProperties() }));
    EXPECT_FALSE(model.getState().valid);
}

TEST(EngineThermalModelTests, ClampsOutOfRangeHeatTransferCoefficient) {
    EngineThermalParameters parameters;
    CylinderThermalSample sample = createAmbientSample();
    sample.gasPressurePa = units::pressure(500000.0, units::bar);

    const double coefficient = EngineThermalModel::calculateHohenbergHeatTransferCoefficient(
        parameters,
        sample);

    EXPECT_DOUBLE_EQ(coefficient, parameters.maximumHeatTransferCoefficientWPerM2K);
}

TEST(EngineThermalModelTests, IsIndependentFromFastSampleSubdivision) {
    EngineThermalModel singleSampleModel;
    EngineThermalModel subdividedSampleModel;
    EngineThermalParameters parameters;
    const CylinderThermalProperties properties = createCylinderProperties();
    ASSERT_TRUE(singleSampleModel.initialize(parameters, { properties }));
    ASSERT_TRUE(subdividedSampleModel.initialize(parameters, { properties }));

    CylinderThermalSample sample = createAmbientSample(400.0);
    sample.gasTemperatureK = 900.0;
    sample.gasPressurePa = units::pressure(5.0, units::bar);
    sample.chamberVolumeM3 = 0.0004350;
    sample.meanPistonSpeedMPerS = 10.795;
    singleSampleModel.update({ sample }, parameters.updateIntervalSeconds);
    for (int i = 0; i < 20; ++i) {
        subdividedSampleModel.update({ sample }, parameters.updateIntervalSeconds / 20.0);
    }

    const EngineThermalState &singleState = singleSampleModel.getState();
    const EngineThermalState &subdividedState = subdividedSampleModel.getState();
    EXPECT_NEAR(singleState.oilTemperatureK, subdividedState.oilTemperatureK, 1e-12);
    EXPECT_NEAR(
        singleState.cylinders[0].pistonTemperatureK,
        subdividedState.cylinders[0].pistonTemperatureK,
        1e-12);
    EXPECT_NEAR(
        singleState.cylinders[0].cylinderTemperatureK,
        subdividedState.cylinders[0].cylinderTemperatureK,
        1e-12);
}

TEST(EngineThermalModelTests, RejectsNumericallyUnstableUpdateInterval) {
    EngineThermalModel model;
    EngineThermalParameters parameters;
    parameters.pistonCylinderConductanceWPerK = 1000000.0;

    EXPECT_FALSE(model.initialize(parameters, { createCylinderProperties() }));
}

TEST(EngineThermalModelTests, CountsGuardedPhysicalInputs) {
    EngineThermalModel model;
    EngineThermalParameters parameters;
    ASSERT_TRUE(model.initialize(parameters, { createCylinderProperties() }));
    CylinderThermalSample sample = createAmbientSample();
    sample.gasTemperatureK = 0.0;
    sample.gasPressurePa = 0.0;
    sample.chamberVolumeM3 = 0.0;
    sample.meanPistonSpeedMPerS = -1.0;

    model.update({ sample }, parameters.updateIntervalSeconds);

    EXPECT_GT(model.getState().inputAnomalyCount, 0u);
    EXPECT_GT(model.getState().oilTemperatureK, 0.0);
    EXPECT_GT(model.getState().cylinders[0].pistonTemperatureK, 0.0);
    EXPECT_GT(model.getState().cylinders[0].cylinderTemperatureK, 0.0);
}

#include "simulation/engine_condition_model.h"

#include "constants.h"
#include "units.h"

#include <gtest/gtest.h>

TEST(EngineConditionModelTests, CalculatesVolumetricEfficiencyAtReferenceConditions) {
    constexpr double displacementM3 = 0.003;
    constexpr double engineSpeedRpm = 3000.0;
    constexpr double ambientPressurePa = units::pressure(1.0, units::atm);
    constexpr double ambientTemperatureK = units::celcius(25.0);
    const double referenceIntakeFlow = 0.5
        * ambientPressurePa
        * displacementM3
        / (constants::R * ambientTemperatureK)
        * engineSpeedRpm
        / 60.0;

    EXPECT_DOUBLE_EQ(
        EngineConditionModel::calculateVolumetricEfficiency(
            referenceIntakeFlow,
            displacementM3,
            engineSpeedRpm),
        1.0);
}

TEST(EngineConditionModelTests, ReturnsZeroVolumetricEfficiencyWhenStopped) {
    EXPECT_DOUBLE_EQ(
        EngineConditionModel::calculateVolumetricEfficiency(1.0, 0.003, 0.0),
        0.0);
}

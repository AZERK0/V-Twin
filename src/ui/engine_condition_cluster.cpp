#include "ui/engine_condition_cluster.h"

#include "app/engine_sim_application.h"
#include "constants.h"
#include "domain/engine/engine.h"
#include "simulation/simulator.h"
#include "ui/gauge.h"
#include "ui/ui_utilities.h"
#include "units.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
    std::string formatValue(double value, int precision, const char *unit) {
        char buffer[48];
        std::snprintf(buffer, sizeof(buffer), "%.*f%s%s", precision, value, unit[0] == '\0' ? "" : " ", unit);
        return buffer;
    }

    std::string formatTemperature(double temperatureK) {
        return formatValue(temperatureK - units::K0, 0, "C");
    }

    std::string formatPower(double powerW) {
        return std::abs(powerW) >= 1000.0
            ? formatValue(powerW / 1000.0, 1, "kW")
            : formatValue(powerW, 0, "W");
    }

    std::string formatPercent(double fraction) {
        return formatValue(fraction * 100.0, 0, "%");
    }

    void configureGauge(Gauge *gauge, float minimum, float maximum, int minorStep, int majorStep) {
        gauge->m_min = minimum;
        gauge->m_max = maximum;
        gauge->m_minorStep = minorStep;
        gauge->m_majorStep = majorStep;
        gauge->m_maxMinorTick = static_cast<int>(maximum);
        gauge->m_thetaMin = static_cast<float>(constants::pi) * 1.2f;
        gauge->m_thetaMax = -static_cast<float>(constants::pi) * 0.2f;
        gauge->m_minorTickLength = 5.0f;
        gauge->m_majorTickLength = 11.0f;
        gauge->m_needleWidth = 4.0f;
        gauge->m_needleKs = 1000.0f;
        gauge->m_needleKd = 24.0f;
        gauge->m_needleMaxVelocity = 2.5f;
        gauge->m_center = { 0.0f, -8.0f };
    }

    void configureOilBands(Gauge *gauge, const EngineSimApplication *app) {
        gauge->setBandCount(4);
        gauge->setBand({ app->getBlue(), 20.0f, 70.0f, 4.0f, 7.0f }, 0);
        gauge->setBand({ app->getGreen(), 70.0f, 120.0f, 4.0f, 7.0f }, 1);
        gauge->setBand({ app->getOrange(), 120.0f, 140.0f, 4.0f, 7.0f }, 2);
        gauge->setBand({ app->getRed(), 140.0f, 180.0f, 4.0f, 7.0f }, 3);
    }

    void configurePistonBands(Gauge *gauge, const EngineSimApplication *app) {
        gauge->setBandCount(4);
        gauge->setBand({ app->getBlue(), 20.0f, 120.0f, 4.0f, 7.0f }, 0);
        gauge->setBand({ app->getGreen(), 120.0f, 300.0f, 4.0f, 7.0f }, 1);
        gauge->setBand({ app->getOrange(), 300.0f, 360.0f, 4.0f, 7.0f }, 2);
        gauge->setBand({ app->getRed(), 360.0f, 450.0f, 4.0f, 7.0f }, 3);
    }

    void configureCylinderBands(Gauge *gauge, const EngineSimApplication *app) {
        gauge->setBandCount(4);
        gauge->setBand({ app->getBlue(), 20.0f, 70.0f, 4.0f, 7.0f }, 0);
        gauge->setBand({ app->getGreen(), 70.0f, 180.0f, 4.0f, 7.0f }, 1);
        gauge->setBand({ app->getOrange(), 180.0f, 220.0f, 4.0f, 7.0f }, 2);
        gauge->setBand({ app->getRed(), 220.0f, 300.0f, 4.0f, 7.0f }, 3);
    }
}

EngineConditionCluster::EngineConditionCluster() {
    m_simulator = nullptr;
    m_oilTemperatureGauge = nullptr;
    m_pistonTemperatureGauge = nullptr;
    m_cylinderTemperatureGauge = nullptr;
    m_cachedRevision = 0;
}

EngineConditionCluster::~EngineConditionCluster() {
}

void EngineConditionCluster::initialize(EngineSimApplication *app) {
    UiElement::initialize(app);
    m_oilTemperatureGauge = addElement<Gauge>();
    m_pistonTemperatureGauge = addElement<Gauge>();
    m_cylinderTemperatureGauge = addElement<Gauge>();
    configureGauge(m_oilTemperatureGauge, 20.0f, 180.0f, 5, 20);
    configureGauge(m_pistonTemperatureGauge, 20.0f, 450.0f, 10, 50);
    configureGauge(m_cylinderTemperatureGauge, 20.0f, 300.0f, 10, 40);
    configureOilBands(m_oilTemperatureGauge, app);
    configurePistonBands(m_pistonTemperatureGauge, app);
    configureCylinderBands(m_cylinderTemperatureGauge, app);
}

void EngineConditionCluster::destroy() {
    UiElement::destroy();
}

void EngineConditionCluster::update(float dt) {
    if (m_simulator != nullptr) {
        const EngineConditionState &state = m_simulator->getEngineConditionState();
        if (state.revision != m_cachedRevision || state.valid != m_state.valid) {
            m_state = state;
            refreshCachedStrings();
        }
    }

    const bool thermalDataAvailable = m_state.valid && m_state.thermal.valid;
    m_oilTemperatureGauge->setVisible(thermalDataAvailable);
    m_pistonTemperatureGauge->setVisible(thermalDataAvailable);
    m_cylinderTemperatureGauge->setVisible(thermalDataAvailable);
    UiElement::update(dt);
}

void EngineConditionCluster::render() {
    const Bounds inner = m_bounds.inset(12.0f);
    drawFrame(m_bounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor(), false);
    renderHeader(inner.verticalSplit(0.93f, 1.0f));
    const Bounds middle = inner.verticalSplit(0.31f, 0.70f);
    const Bounds temperatureBounds = middle.horizontalSplit(0.40f, 1.0f);
    if (!m_state.valid) {
        renderUnavailableState(temperatureBounds);
        return;
    }

    renderTemperaturePanel(temperatureBounds);
    const Bounds bottom = inner.verticalSplit(0.0f, 0.29f);
    renderCylinderPanel(bottom.horizontalSplit(0.0f, 0.60f));
    const Bounds diagnostics = bottom.horizontalSplit(0.61f, 1.0f);
    renderOperatingPointPanel(diagnostics.verticalSplit(0.70f, 1.0f));
    renderThermalPowerPanel(diagnostics.verticalSplit(0.30f, 0.68f));
    renderCoolingSystemPanel(diagnostics.verticalSplit(0.0f, 0.28f));
    UiElement::render();
}

void EngineConditionCluster::setSimulator(Simulator *simulator) {
    m_simulator = simulator;
    m_state = EngineConditionState{};
    m_cachedRevision = 0;
}

void EngineConditionCluster::refreshCachedStrings() {
    m_cachedRevision = m_state.revision;
    refreshTemperatureStrings();
    refreshOperatingPointStrings();
    refreshThermalPowerStrings();
    refreshCoolingStrings();
    updateTemperatureGauges();
}

void EngineConditionCluster::refreshTemperatureStrings() {
    if (!m_state.thermal.valid) {
        m_oilTemperatureText = m_pistonTemperatureText = m_cylinderTemperatureText = "NO DATA";
        return;
    }

    m_oilTemperatureText = formatTemperature(m_state.thermal.oilTemperatureK);
    m_pistonTemperatureText = formatTemperature(m_state.thermal.maximumPistonTemperatureK);
    m_cylinderTemperatureText = formatTemperature(m_state.thermal.maximumCylinderTemperatureK);
}

void EngineConditionCluster::refreshOperatingPointStrings() {
    m_throttleText = formatValue(m_state.throttlePosition * 100.0, 0, "%");
    m_intakeAfrText = formatValue(m_state.intakeAfr, 1, "AFR");
    m_exhaustOxygenText = formatValue(m_state.exhaustOxygenFraction * 100.0, 1, "%");
    if (!m_state.thermal.valid) {
        m_thermalInputText = "UNAVAILABLE";
    }
    else if (m_state.thermal.inputAnomalyCount == 0) {
        m_thermalInputText = "VALID";
    }
    else {
        m_thermalInputText = std::to_string(m_state.thermal.inputAnomalyCount) + " ANOMALIES";
    }
}

void EngineConditionCluster::refreshThermalPowerStrings() {
    const EngineThermalPowerBalance &power = m_state.thermal.powerBalance;
    m_heatInputText = formatPower(power.gasToPistonsW + power.gasToCylindersW);
    m_frictionHeatText = formatPower(power.frictionW);
    m_heatRejectedText = formatPower(power.ambientRejectionW);
    m_oilStoragePowerText = formatPower(power.oilStorageW);
    m_coolantStoragePowerText = m_state.thermal.coolantModeled
        ? formatPower(power.coolantStorageW)
        : "N/A";
    m_energyResidualText = formatValue(power.relativeEnergyResidual * 1e6, 2, "ppm");
}

void EngineConditionCluster::refreshCoolingStrings() {
    const EngineThermalState &thermal = m_state.thermal;
    const EngineCoolingState &cooling = thermal.cooling;
    m_coolantStatusText = thermal.coolantModeled
        ? formatTemperature(thermal.coolantTemperatureK)
            + " // THERM " + formatPercent(cooling.coolantThermostatOpening)
        : "NOT MODELED";
    m_sumpStatusText = thermal.sumpModeled
        ? formatTemperature(thermal.sumpTemperatureK)
            + " // OIL THERM " + formatPercent(cooling.oilThermostatOpening)
        : "NOT MODELED";
    m_airAndFanText = thermal.coolantModeled
        ? formatValue(cooling.effectiveRadiatorAirSpeedMPerS, 1, "m/s")
            + " // FAN " + formatPercent(cooling.fanDuty)
        : "NOT MODELED";
    m_pumpStatusText = thermal.coolantModeled
        ? "W " + formatPercent(cooling.coolantPumpFactor)
            + " // O " + formatPercent(cooling.oilPumpFactor)
        : "NOT MODELED";
}

void EngineConditionCluster::updateTemperatureGauges() {
    if (!m_state.thermal.valid) {
        return;
    }

    m_oilTemperatureGauge->m_value = static_cast<float>(m_state.thermal.oilTemperatureK - units::K0);
    m_pistonTemperatureGauge->m_value = static_cast<float>(m_state.thermal.maximumPistonTemperatureK - units::K0);
    m_cylinderTemperatureGauge->m_value = static_cast<float>(m_state.thermal.maximumCylinderTemperatureK - units::K0);
}

void EngineConditionCluster::renderHeader(const Bounds &bounds) {
    drawFrame(bounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());
    const Bounds inner = bounds.inset(10.0f);
    drawAlignedText("ENGINE CONDITION", inner, 28.0f, Bounds::lm, Bounds::lm);
    std::string engineName = "NO ENGINE";
    if (m_simulator != nullptr && m_simulator->getEngine() != nullptr) {
        engineName = m_simulator->getEngine()->getName();
    }
    drawAlignedText(engineName, inner, 18.0f, Bounds::center, Bounds::center);
    drawAlignedText("PHYSICAL TELEMETRY  //  [J] ANALYSIS", inner, 16.0f, Bounds::rm, Bounds::rm);
}

void EngineConditionCluster::renderValueTile(
    const Bounds &bounds,
    const std::string &label,
    const std::string &value)
{
    drawFrame(bounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());
    const Bounds inner = bounds.inset(8.0f);
    drawText(label, inner.verticalSplit(0.68f, 1.0f), 13.0f, Bounds::tl);
    const float valueSize = std::clamp(bounds.height() * 0.28f, 17.0f, 25.0f);
    drawAlignedText(value, inner.verticalSplit(0.0f, 0.64f), valueSize, Bounds::br, Bounds::br);
}

void EngineConditionCluster::renderTemperaturePanel(const Bounds &bounds) {
    Grid grid{ 3, 1 };
    renderTemperatureTile(grid.get(bounds, 0, 0).inset(4.0f), "OIL TEMPERATURE", "GLOBAL MEAN",
        m_oilTemperatureText, m_oilTemperatureGauge);
    renderTemperatureTile(grid.get(bounds, 1, 0).inset(4.0f), "PISTON TEMPERATURE", "HOTTEST MEAN",
        m_pistonTemperatureText, m_pistonTemperatureGauge);
    renderTemperatureTile(grid.get(bounds, 2, 0).inset(4.0f), "CYLINDER TEMPERATURE", "HOTTEST LINER",
        m_cylinderTemperatureText, m_cylinderTemperatureGauge);
}

void EngineConditionCluster::renderTemperatureTile(
    const Bounds &bounds,
    const std::string &label,
    const std::string &detail,
    const std::string &value,
    Gauge *gauge)
{
    drawFrame(bounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());
    const Bounds inner = bounds.inset(10.0f);
    drawText(label, inner.verticalSplit(0.88f, 1.0f), 18.0f, Bounds::tl);
    drawAlignedText(detail, inner.verticalSplit(0.88f, 1.0f), 12.0f, Bounds::tr, Bounds::tr);
    const Bounds gaugeBounds = inner.verticalSplit(0.18f, 0.86f);
    gauge->m_bounds = gaugeBounds;
    gauge->m_outerRadius = std::min(gaugeBounds.width(), gaugeBounds.height()) * 0.47f;
    gauge->m_needleOuterRadius = gauge->m_outerRadius * 0.72f;
    gauge->m_needleInnerRadius = -gauge->m_outerRadius * 0.08f;
    drawAlignedText(value, inner.verticalSplit(0.0f, 0.18f), 30.0f, Bounds::center, Bounds::center);
}

void EngineConditionCluster::renderCylinderPanel(const Bounds &bounds) {
    drawFrame(bounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());
    const Bounds inner = bounds.inset(9.0f);
    drawText("CYLINDER THERMAL MAP", inner.verticalSplit(0.88f, 1.0f), 15.0f, Bounds::tl);
    drawAlignedText("MEAN LUMPED TEMPERATURES", inner.verticalSplit(0.88f, 1.0f), 11.0f, Bounds::tr, Bounds::tr);
    const int count = static_cast<int>(m_state.thermal.cylinders.size());
    if (count == 0) {
        drawAlignedText("NO THERMAL CYLINDER DATA", inner, 16.0f, Bounds::center, Bounds::center);
        return;
    }

    const Bounds body = inner.verticalSplit(0.0f, 0.84f);
    const int firstColumnCount = count > 6 ? (count + 1) / 2 : count;
    renderCylinderColumn(body.horizontalSplit(0.0f, count > 6 ? 0.49f : 1.0f), 0, firstColumnCount);
    if (count > 6) {
        renderCylinderColumn(body.horizontalSplit(0.51f, 1.0f), firstColumnCount, count - firstColumnCount);
    }
}

void EngineConditionCluster::renderCylinderColumn(
    const Bounds &bounds,
    int firstCylinder,
    int cylinderCount)
{
    const float textSize = std::clamp(bounds.height() / (cylinderCount + 1) * 0.34f, 10.0f, 14.0f);
    Grid grid{ 1, cylinderCount + 1 };
    const Bounds header = grid.get(bounds, 0, 0).inset(4.0f);
    drawText("CYL", header.horizontalSplit(0.0f, 0.16f), textSize, Bounds::lm);
    drawAlignedText("PISTON", header.horizontalSplit(0.16f, 0.50f), textSize, Bounds::rm, Bounds::rm);
    drawAlignedText("CYLINDER", header.horizontalSplit(0.50f, 0.82f), textSize, Bounds::rm, Bounds::rm);
    drawAlignedText("DELTA", header.horizontalSplit(0.82f, 1.0f), textSize, Bounds::rm, Bounds::rm);
    for (int row = 0; row < cylinderCount; ++row) {
        renderCylinderRow(grid.get(bounds, 0, row + 1).inset(2.0f), firstCylinder + row, textSize);
    }
}

void EngineConditionCluster::renderCylinderRow(const Bounds &bounds, int cylinderIndex, float textSize) {
    const CylinderThermalState &cylinder = m_state.thermal.cylinders[cylinderIndex];
    const bool hottest = cylinder.pistonTemperatureK == m_state.thermal.maximumPistonTemperatureK
        || cylinder.cylinderTemperatureK == m_state.thermal.maximumCylinderTemperatureK;
    const ysVector fill = hottest
        ? mix(m_app->getOrange(), m_app->getBackgroundColor(), 0.92f)
        : m_app->getBackgroundColor();
    drawFrame(bounds, 1.0f, m_app->getForegroundColor(), fill);
    const Bounds inner = bounds.inset(5.0f);
    drawText("C" + std::to_string(cylinderIndex + 1), inner.horizontalSplit(0.0f, 0.16f), textSize, Bounds::lm);
    drawAlignedText(formatTemperature(cylinder.pistonTemperatureK), inner.horizontalSplit(0.16f, 0.50f),
        textSize, Bounds::rm, Bounds::rm);
    drawAlignedText(formatTemperature(cylinder.cylinderTemperatureK), inner.horizontalSplit(0.50f, 0.82f),
        textSize, Bounds::rm, Bounds::rm);
    drawAlignedText(formatValue(cylinder.pistonTemperatureK - cylinder.cylinderTemperatureK, 0, "K"),
        inner.horizontalSplit(0.82f, 1.0f), textSize, Bounds::rm, Bounds::rm);
}

void EngineConditionCluster::renderOperatingPointPanel(const Bounds &bounds) {
    drawFrame(bounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());
    const Bounds inner = bounds.inset(8.0f);
    drawText("OPERATING POINT", inner.verticalSplit(0.78f, 1.0f), 14.0f, Bounds::tl);
    Grid grid{ 4, 1 };
    const Bounds body = inner.verticalSplit(0.0f, 0.72f);
    renderValueTile(grid.get(body, 0, 0).inset(2.0f), "THROTTLE", m_throttleText);
    renderValueTile(grid.get(body, 1, 0).inset(2.0f), "INTAKE", m_intakeAfrText);
    renderValueTile(grid.get(body, 2, 0).inset(2.0f), "EXHAUST O2", m_exhaustOxygenText);
    renderValueTile(grid.get(body, 3, 0).inset(2.0f), "THERMAL INPUT", m_thermalInputText);
}

void EngineConditionCluster::renderThermalPowerPanel(const Bounds &bounds) {
    drawFrame(bounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());
    const Bounds inner = bounds.inset(8.0f);
    drawText("THERMAL POWER BALANCE", inner.verticalSplit(0.78f, 1.0f), 14.0f, Bounds::tl);
    const Bounds body = inner.verticalSplit(0.0f, 0.72f);
    Grid grid{ 3, 2 };
    renderValueTile(grid.get(body, 0, 0).inset(2.0f), "COMBUSTION", m_heatInputText);
    renderValueTile(grid.get(body, 1, 0).inset(2.0f), "FRICTION", m_frictionHeatText);
    renderValueTile(grid.get(body, 2, 0).inset(2.0f), "REJECTED", m_heatRejectedText);
    renderValueTile(grid.get(body, 0, 1).inset(2.0f), "OIL STORAGE", m_oilStoragePowerText);
    renderValueTile(grid.get(body, 1, 1).inset(2.0f), "COOLANT STORAGE", m_coolantStoragePowerText);
    renderValueTile(grid.get(body, 2, 1).inset(2.0f), "RESIDUAL", m_energyResidualText);
}

void EngineConditionCluster::renderCoolingSystemPanel(const Bounds &bounds) {
    drawFrame(bounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());
    const Bounds inner = bounds.inset(8.0f);
    drawText("COOLING SYSTEM", inner.verticalSplit(0.78f, 1.0f), 14.0f, Bounds::tl);
    const Bounds body = inner.verticalSplit(0.0f, 0.72f);
    Grid grid{ 1, 4 };
    renderInfoRow(grid.get(body, 0, 0), "COOLANT", m_coolantStatusText);
    renderInfoRow(grid.get(body, 0, 1), "SUMP", m_sumpStatusText);
    renderInfoRow(grid.get(body, 0, 2), "RADIATOR AIR", m_airAndFanText);
    renderInfoRow(grid.get(body, 0, 3), "PUMP FACTORS", m_pumpStatusText);
}

void EngineConditionCluster::renderInfoRow(
    const Bounds &bounds,
    const std::string &label,
    const std::string &value)
{
    const Bounds inner = bounds.inset(3.0f);
    drawText(label, inner, 11.0f, Bounds::lm);
    drawAlignedText(value, inner, 11.0f, Bounds::rm, Bounds::rm);
}

void EngineConditionCluster::renderUnavailableState(const Bounds &bounds) {
    drawFrame(bounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());
    drawAlignedText("ENGINE CONDITION DATA UNAVAILABLE", bounds, 22.0f, Bounds::center, Bounds::center);
}

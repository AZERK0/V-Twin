#include "ui/engine_wear_cluster.h"

#include "app/engine_sim_application.h"
#include "simulation/simulator.h"
#include "ui/ui_utilities.h"

#include <algorithm>
#include <cstdio>

namespace {
    float clamp01(float value) {
        return std::clamp(value, 0.0f, 1.0f);
    }

    std::string formatPercent(float value, int precision = 0) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.*f%%", precision, value * 100.0f);
        return buffer;
    }

    std::string formatRate(float valuePerHour) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.2f%%/h", valuePerHour);
        return buffer;
    }

    std::string formatHours(float hours) {
        char buffer[32];
        if (hours >= 999.0f) {
            std::snprintf(buffer, sizeof(buffer), "999+h");
        }
        else if (hours >= 100.0f) {
            std::snprintf(buffer, sizeof(buffer), "%.0fh", hours);
        }
        else {
            std::snprintf(buffer, sizeof(buffer), "%.1fh", hours);
        }

        return buffer;
    }

    std::string formatTemperature(float temperatureC) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.0fC", temperatureC);
        return buffer;
    }

    std::string formatDuration(float seconds) {
        char buffer[32];
        if (seconds >= 3600.0f) {
            std::snprintf(buffer, sizeof(buffer), "%.1fh", seconds / 3600.0f);
        }
        else if (seconds >= 60.0f) {
            std::snprintf(buffer, sizeof(buffer), "%.0fm", seconds / 60.0f);
        }
        else {
            std::snprintf(buffer, sizeof(buffer), "%.0fs", seconds);
        }

        return buffer;
    }

    std::string formatCount(int value) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%d", value);
        return buffer;
    }
}

EngineWearCluster::EngineWearCluster() {
    m_simulator = nullptr;
    m_cachedRevision = 0;
    m_state = EngineWearState{};

    m_healthText = "100%";
    m_damageRateText = "0.00%/h";
    m_rulText = "999+h";
    m_confidenceText = "100%";
    m_failureModeText = "balanced";
    m_oilTemperatureText = "90C";
    m_coolantTemperatureText = "88C";

    m_thermalMarginText = "100%";
    m_lubricationMarginText = "100%";
    m_detonationMarginText = "100%";
    m_fatigueLoadText = "0%";
    m_combustionStabilityText = "100%";

    m_bottomEndDamageText = "0%";
    m_ringSealDamageText = "0%";
    m_valvetrainDamageText = "0%";
    m_headGasketDamageText = "0%";
    m_lubricationDamageText = "0%";

    m_overRevText = "0s";
    m_overTempText = "0s";
    m_coldLoadText = "0s";
    m_oilStarvationText = "0s";
    m_knockEventText = "0";
    m_thermalCyclesText = "0";
}

EngineWearCluster::~EngineWearCluster() {
    /* void */
}

void EngineWearCluster::initialize(EngineSimApplication *app) {
    UiElement::initialize(app);
}

void EngineWearCluster::destroy() {
    UiElement::destroy();
}

void EngineWearCluster::update(float dt) {
    m_mouseBounds = m_bounds;

    if (m_simulator != nullptr) {
        const EngineWearState &state = m_simulator->getEngineWearState();
        if (state.revision != m_cachedRevision) {
            m_state = state;
            refreshCachedStrings();
        }
    }

    UiElement::update(dt);
}

void EngineWearCluster::render() {
    renderSummary(m_bounds);
}

void EngineWearCluster::renderDashboard(const Bounds &bounds) {
    if (!m_state.valid) {
        drawFrame(bounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());
        drawAlignedText("NO ENGINE WEAR DATA", bounds, 20.0f, Bounds::center, Bounds::center);
        return;
    }

    const float sectionSize = std::clamp(bounds.height() * 0.026f, 13.0f, 18.0f);
    const float bodySize = std::clamp(bounds.height() * 0.022f, 11.0f, 15.0f);
    const ysVector fg = m_app->getForegroundColor();
    const ysVector bg = m_app->getBackgroundColor();

    drawFrame(bounds, 1.0f, fg, bg);

    const Bounds inner = bounds.inset(10.0f);
    const Bounds headerBounds = inner.verticalSplit(0.94f, 1.0f);
    const Bounds bodyBounds = inner.verticalSplit(0.0f, 0.92f);
    drawText("ENGINE WEAR", headerBounds, sectionSize, Bounds::tl);
    drawAlignedText("[J]", headerBounds, bodySize, Bounds::tr, Bounds::tr);

    const Bounds leftColumn = bodyBounds.horizontalSplit(0.0f, 0.36f);
    const Bounds rightColumn = bodyBounds.horizontalSplit(0.39f, 1.0f);

    const Bounds overviewBounds = leftColumn.verticalSplit(0.42f, 1.0f);
    const Bounds exposureBounds = leftColumn.verticalSplit(0.0f, 0.39f);
    const Bounds liveStressBounds = rightColumn.verticalSplit(0.48f, 1.0f);
    const Bounds damageBounds = rightColumn.verticalSplit(0.0f, 0.45f);

    drawFrame(overviewBounds, 1.0f, fg, mix(getRiskColor(1.0f - m_state.globalHealth), bg, 0.90f));
    drawFrame(exposureBounds, 1.0f, fg, bg);
    drawFrame(liveStressBounds, 1.0f, fg, bg);
    drawFrame(damageBounds, 1.0f, fg, bg);

    renderOverviewPanel(overviewBounds.inset(10.0f));
    renderExposurePanel(exposureBounds.inset(10.0f));
    renderLiveStressPanel(liveStressBounds.inset(10.0f));
    renderDamagePanel(damageBounds.inset(10.0f));
}

void EngineWearCluster::setSimulator(Simulator *simulator) {
    m_simulator = simulator;
    m_cachedRevision = 0;
    m_state = EngineWearState{};
}

void EngineWearCluster::refreshCachedStrings() {
    m_cachedRevision = m_state.revision;

    m_healthText = formatPercent(m_state.globalHealth);
    m_damageRateText = formatRate(m_state.damageRatePerHour);
    m_rulText = formatHours(m_state.remainingUsefulLifeHours);
    m_confidenceText = formatPercent(m_state.confidence);
    m_failureModeText = getFailureModeLabel(m_state.dominantFailureMode);
    m_oilTemperatureText = formatTemperature(m_state.oilTemperatureC);
    m_coolantTemperatureText = formatTemperature(m_state.coolantTemperatureC);

    m_thermalMarginText = formatPercent(m_state.thermalMargin);
    m_lubricationMarginText = formatPercent(m_state.lubricationMargin);
    m_detonationMarginText = formatPercent(m_state.detonationMargin);
    m_fatigueLoadText = formatPercent(m_state.mechanicalFatigueLoad);
    m_combustionStabilityText = formatPercent(m_state.combustionStability);

    m_bottomEndDamageText = formatPercent(m_state.bottomEndDamage);
    m_ringSealDamageText = formatPercent(m_state.ringSealDamage);
    m_valvetrainDamageText = formatPercent(m_state.valvetrainDamage);
    m_headGasketDamageText = formatPercent(m_state.headGasketDamage);
    m_lubricationDamageText = formatPercent(m_state.lubricationSystemDamage);

    m_overRevText = formatDuration(m_state.overRevSeconds);
    m_overTempText = formatDuration(m_state.overTempSeconds);
    m_coldLoadText = formatDuration(m_state.coldHighLoadSeconds);
    m_oilStarvationText = formatDuration(m_state.oilStarvationSeconds);
    m_knockEventText = formatCount(m_state.severeKnockEvents);
    m_thermalCyclesText = formatCount(m_state.thermalCycles);
}

void EngineWearCluster::renderSummary(const Bounds &bounds) {
    drawFrame(bounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    const Bounds inner = bounds.inset(10.0f);
    drawText("WEAR", inner.verticalSplit(0.80f, 1.0f), 18.0f, Bounds::tl);
    drawAlignedText("[J]", inner.verticalSplit(0.80f, 1.0f), 14.0f, Bounds::tr, Bounds::tr);

    if (!m_state.valid) {
        drawAlignedText("NO DATA", inner.verticalSplit(0.36f, 0.70f), 24.0f, Bounds::center, Bounds::center);
        return;
    }

    drawAlignedText(m_healthText, inner.verticalSplit(0.50f, 0.78f), 40.0f, Bounds::center, Bounds::center);
    drawAlignedText(m_damageRateText, inner.verticalSplit(0.32f, 0.44f), 14.0f, Bounds::center, Bounds::center);
    drawText(m_failureModeText, inner.verticalSplit(0.18f, 0.28f), 13.0f, Bounds::lm);

    const Bounds barBounds = inner.verticalSplit(0.06f, 0.16f);
    drawBox(barBounds, mix(m_app->getForegroundColor(), m_app->getBackgroundColor(), 0.88f));
    drawBox(
        Bounds(barBounds.width() * m_state.globalHealth, barBounds.height(), barBounds.getPosition(Bounds::bl), Bounds::bl),
        getRiskColor(1.0f - m_state.globalHealth));
}

void EngineWearCluster::renderOverviewPanel(const Bounds &bounds) {
    const float titleSize = std::clamp(bounds.height() * 0.10f, 14.0f, 18.0f);
    const float bodySize = std::clamp(bounds.height() * 0.07f, 11.0f, 15.0f);
    const float valueSize = std::clamp(bounds.height() * 0.20f, 34.0f, 54.0f);

    drawText("OVERVIEW", bounds.verticalSplit(0.90f, 1.0f), titleSize, Bounds::tl);
    drawAlignedText(m_confidenceText, bounds.verticalSplit(0.90f, 1.0f), bodySize, Bounds::tr, Bounds::tr);
    drawAlignedText(m_healthText, bounds.verticalSplit(0.56f, 0.86f), valueSize, Bounds::center, Bounds::center);
    drawAlignedText(m_failureModeText, bounds.verticalSplit(0.46f, 0.54f), bodySize, Bounds::center, Bounds::center);

    Grid infoGrid{ 1, 4 };
    const Bounds infoRows = bounds.verticalSplit(0.0f, 0.42f);
    drawInfoRow(infoGrid.get(infoRows, 0, 3), "RATE", m_damageRateText, bodySize);
    drawInfoRow(infoGrid.get(infoRows, 0, 2), "RUL", m_rulText, bodySize);
    drawInfoRow(infoGrid.get(infoRows, 0, 1), "OIL", m_oilTemperatureText, bodySize);
    drawInfoRow(infoGrid.get(infoRows, 0, 0), "COOLANT", m_coolantTemperatureText, bodySize);
}

void EngineWearCluster::renderExposurePanel(const Bounds &bounds) {
    const float titleSize = std::clamp(bounds.height() * 0.12f, 14.0f, 18.0f);
    const float bodySize = std::clamp(bounds.height() * 0.10f, 11.0f, 15.0f);

    drawText("EXPOSURE", bounds.verticalSplit(0.88f, 1.0f), titleSize, Bounds::tl);

    Grid infoGrid{ 1, 6 };
    const Bounds rows = bounds.verticalSplit(0.0f, 0.82f);
    drawInfoRow(infoGrid.get(rows, 0, 5), "OVERREV", m_overRevText, bodySize);
    drawInfoRow(infoGrid.get(rows, 0, 4), "OVERTEMP", m_overTempText, bodySize);
    drawInfoRow(infoGrid.get(rows, 0, 3), "COLD LOAD", m_coldLoadText, bodySize);
    drawInfoRow(infoGrid.get(rows, 0, 2), "OIL STARVE", m_oilStarvationText, bodySize);
    drawInfoRow(infoGrid.get(rows, 0, 1), "KNOCK EV", m_knockEventText, bodySize);
    drawInfoRow(infoGrid.get(rows, 0, 0), "HEAT CYC", m_thermalCyclesText, bodySize);
}

void EngineWearCluster::renderLiveStressPanel(const Bounds &bounds) {
    const float titleSize = std::clamp(bounds.height() * 0.08f, 14.0f, 18.0f);
    drawText("LIVE STRESS", bounds.verticalSplit(0.90f, 1.0f), titleSize, Bounds::tl);

    Grid metricGrid{ 1, 5 };
    const Bounds rows = bounds.verticalSplit(0.0f, 0.84f);
    drawMetricRow(metricGrid.get(rows, 0, 4), "THERMAL", m_thermalMarginText, m_state.thermalMargin, 1.0f - m_state.thermalMargin);
    drawMetricRow(metricGrid.get(rows, 0, 3), "LUBE", m_lubricationMarginText, m_state.lubricationMargin, 1.0f - m_state.lubricationMargin);
    drawMetricRow(metricGrid.get(rows, 0, 2), "DET MARGIN", m_detonationMarginText, m_state.detonationMargin, 1.0f - m_state.detonationMargin);
    drawMetricRow(metricGrid.get(rows, 0, 1), "FATIGUE", m_fatigueLoadText, m_state.mechanicalFatigueLoad, m_state.mechanicalFatigueLoad);
    drawMetricRow(metricGrid.get(rows, 0, 0), "STABILITY", m_combustionStabilityText, m_state.combustionStability, 1.0f - m_state.combustionStability);
}

void EngineWearCluster::renderDamagePanel(const Bounds &bounds) {
    const float titleSize = std::clamp(bounds.height() * 0.08f, 14.0f, 18.0f);
    drawText("DAMAGE BREAKDOWN", bounds.verticalSplit(0.90f, 1.0f), titleSize, Bounds::tl);

    Grid metricGrid{ 1, 5 };
    const Bounds rows = bounds.verticalSplit(0.0f, 0.84f);
    drawMetricRow(metricGrid.get(rows, 0, 4), "BOTTOM END", m_bottomEndDamageText, m_state.bottomEndDamage, m_state.bottomEndDamage);
    drawMetricRow(metricGrid.get(rows, 0, 3), "RING SEAL", m_ringSealDamageText, m_state.ringSealDamage, m_state.ringSealDamage);
    drawMetricRow(metricGrid.get(rows, 0, 2), "VALVETRAIN", m_valvetrainDamageText, m_state.valvetrainDamage, m_state.valvetrainDamage);
    drawMetricRow(metricGrid.get(rows, 0, 1), "HEAD/GASKET", m_headGasketDamageText, m_state.headGasketDamage, m_state.headGasketDamage);
    drawMetricRow(metricGrid.get(rows, 0, 0), "LUBE SYS", m_lubricationDamageText, m_state.lubricationSystemDamage, m_state.lubricationSystemDamage);
}

void EngineWearCluster::drawMetricRow(
    const Bounds &bounds,
    const std::string &label,
    const std::string &valueText,
    float displayRatio,
    float risk)
{
    const Bounds row = bounds.inset(2.0f);
    const Bounds barBounds = row.horizontalSplit(0.32f, 0.82f).verticalSplit(0.22f, 0.72f);
    const float textSize = std::clamp(row.height() * 0.42f, 11.0f, 15.0f);

    drawText(label, row.horizontalSplit(0.0f, 0.30f), textSize, Bounds::lm);
    drawAlignedText(valueText, row.horizontalSplit(0.84f, 1.0f), textSize, Bounds::rm, Bounds::rm);
    drawBox(barBounds, mix(m_app->getForegroundColor(), m_app->getBackgroundColor(), 0.88f));
    drawBox(
        Bounds(barBounds.width() * clamp01(displayRatio), barBounds.height(), barBounds.getPosition(Bounds::bl), Bounds::bl),
        getRiskColor(risk));
}

void EngineWearCluster::drawInfoRow(
    const Bounds &bounds,
    const std::string &label,
    const std::string &valueText,
    float textSize)
{
    drawText(label, bounds, textSize, Bounds::lm);
    drawAlignedText(valueText, bounds, textSize, Bounds::rm, Bounds::rm);
}

ysVector EngineWearCluster::getRiskColor(float risk) const {
    const float clamped = clamp01(risk);
    if (clamped < 0.33f) {
        return mix(m_app->getGreen(), m_app->getYellow(), clamped / 0.33f);
    }
    else if (clamped < 0.66f) {
        return mix(m_app->getYellow(), m_app->getOrange(), (clamped - 0.33f) / 0.33f);
    }

    return mix(m_app->getOrange(), m_app->getRed(), (clamped - 0.66f) / 0.34f);
}

const char *EngineWearCluster::getFailureModeLabel(EngineWearState::FailureMode mode) const {
    switch (mode) {
    case EngineWearState::FailureMode::Balanced:
        return "balanced";
    case EngineWearState::FailureMode::ThermalOverload:
        return "thermal overload";
    case EngineWearState::FailureMode::LubricationBreakdown:
        return "lubrication breakdown";
    case EngineWearState::FailureMode::DetonationDamage:
        return "detonation damage";
    case EngineWearState::FailureMode::BottomEndFatigue:
        return "bottom-end fatigue";
    case EngineWearState::FailureMode::RingSealWear:
        return "ring seal wear";
    case EngineWearState::FailureMode::ValvetrainFatigue:
        return "valvetrain fatigue";
    case EngineWearState::FailureMode::HeadGasketRisk:
        return "head gasket risk";
    }

    return "balanced";
}

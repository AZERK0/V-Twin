#include "ui/engine_wear_cluster.h"

#include "app/engine_sim_application.h"
#include "constants.h"
#include "simulation/simulator.h"
#include "ui/gauge.h"
#include "ui/ui_utilities.h"

#include <algorithm>
#include <array>
#include <cstdio>

namespace {
    struct RankedMetric {
        const char *label;
        const std::string *valueText;
        float ratio;
        float risk;
    };

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

    void configureReserveGauge(Gauge *gauge, const EngineSimApplication *app, bool inverseBands) {
        gauge->m_min = 0.0f;
        gauge->m_max = 100.0f;
        gauge->m_minorStep = 5;
        gauge->m_majorStep = 10;
        gauge->m_maxMinorTick = 100;
        gauge->m_thetaMin = static_cast<float>(constants::pi) * 1.2f;
        gauge->m_thetaMax = -static_cast<float>(constants::pi) * 0.2f;
        gauge->m_minorTickWidth = 1.0f;
        gauge->m_majorTickWidth = 2.0f;
        gauge->m_minorTickLength = 5.0f;
        gauge->m_majorTickLength = 10.0f;
        gauge->m_needleWidth = 3.0f;
        gauge->m_gamma = 1.0f;
        gauge->m_needleKs = 1000.0f;
        gauge->m_needleKd = 20.0f;
        gauge->m_needleMaxVelocity = 2.0f;
        gauge->m_center = { 0.0f, -6.0f };
        gauge->setBandCount(3);

        if (inverseBands) {
            gauge->setBand({ app->getRed(), 0.0f, 40.0f, 3.0f, 6.0f }, 0);
            gauge->setBand({ app->getOrange(), 40.0f, 70.0f, 3.0f, 6.0f }, 1);
            gauge->setBand({ app->getGreen(), 70.0f, 100.0f, 3.0f, 6.0f }, 2);
        }
        else {
            gauge->setBand({ app->getGreen(), 0.0f, 40.0f, 3.0f, 6.0f }, 0);
            gauge->setBand({ app->getYellow(), 40.0f, 70.0f, 3.0f, 6.0f }, 1);
            gauge->setBand({ app->getRed(), 70.0f, 100.0f, 3.0f, 6.0f }, 2);
        }
    }
}

EngineWearCluster::EngineWearCluster() {
    m_simulator = nullptr;
    m_healthGauge = nullptr;
    m_thermalGauge = nullptr;
    m_lubricationGauge = nullptr;
    m_detonationGauge = nullptr;
    m_cachedRevision = 0;
    m_state = EngineWearState{};

    m_healthText = "100%";
    m_damageRateText = "0.00%/h";
    m_rulText = "999+h";
    m_confidenceText = "100%";
    m_failureModeText = "BALANCED";
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

    m_healthGauge = addElement<Gauge>();
    m_thermalGauge = addElement<Gauge>();
    m_lubricationGauge = addElement<Gauge>();
    m_detonationGauge = addElement<Gauge>();

    configureReserveGauge(m_healthGauge, app, true);
    configureReserveGauge(m_thermalGauge, app, true);
    configureReserveGauge(m_lubricationGauge, app, true);
    configureReserveGauge(m_detonationGauge, app, true);
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
    drawAlignedText("[J] MODEL LIVE", headerBounds, bodySize, Bounds::tr, Bounds::tr);

    const Bounds kpiBounds = bodyBounds.verticalSplit(0.76f, 1.0f);
    const Bounds detailBounds = bodyBounds.verticalSplit(0.0f, 0.72f);
    const Bounds leftColumn = detailBounds.horizontalSplit(0.0f, 0.34f);
    const Bounds rightColumn = detailBounds.horizontalSplit(0.37f, 1.0f);
    const Bounds focusBounds = leftColumn.verticalSplit(0.44f, 1.0f);
    const Bounds exposureBounds = leftColumn.verticalSplit(0.0f, 0.40f);
    const Bounds stressBounds = rightColumn.verticalSplit(0.50f, 1.0f);
    const Bounds damageBounds = rightColumn.verticalSplit(0.0f, 0.46f);

    renderKpiStrip(kpiBounds);
    drawFrame(focusBounds, 1.0f, fg, mix(getRiskColor(1.0f - m_state.globalHealth), bg, 0.92f));
    drawFrame(exposureBounds, 1.0f, fg, bg);
    drawFrame(stressBounds, 1.0f, fg, bg);
    drawFrame(damageBounds, 1.0f, fg, bg);

    renderOverviewPanel(focusBounds.inset(10.0f));
    renderExposurePanel(exposureBounds.inset(10.0f));
    renderLiveStressPanel(stressBounds.inset(10.0f));
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

    if (m_healthGauge != nullptr) {
        m_healthGauge->m_value = m_state.globalHealth * 100.0f;
        m_thermalGauge->m_value = m_state.thermalMargin * 100.0f;
        m_lubricationGauge->m_value = m_state.lubricationMargin * 100.0f;
        m_detonationGauge->m_value = m_state.detonationMargin * 100.0f;
    }
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

    drawAlignedText(m_healthText, inner.verticalSplit(0.52f, 0.80f), 40.0f, Bounds::center, Bounds::center);
    drawAlignedText(m_failureModeText, inner.verticalSplit(0.38f, 0.48f), 16.0f, Bounds::center, Bounds::center);
    drawInfoRow(inner.verticalSplit(0.18f, 0.30f), "RATE", m_damageRateText, 13.0f);
    drawInfoRow(inner.verticalSplit(0.08f, 0.18f), "RUL", m_rulText, 13.0f);

    const Bounds barBounds = inner.verticalSplit(0.00f, 0.06f);
    drawBox(barBounds, mix(m_app->getForegroundColor(), m_app->getBackgroundColor(), 0.88f));
    drawBox(
        Bounds(barBounds.width() * m_state.globalHealth, barBounds.height(), barBounds.getPosition(Bounds::bl), Bounds::bl),
        getRiskColor(1.0f - m_state.globalHealth));
}

void EngineWearCluster::renderKpiStrip(const Bounds &bounds) {
    const ysVector bg = m_app->getBackgroundColor();
    const float valueSize = std::clamp(bounds.height() * 0.20f, 16.0f, 24.0f);
    const float modeValueSize = std::clamp(bounds.height() * 0.16f, 13.0f, 20.0f);

    renderGaugeTile(
        bounds.horizontalSplit(0.00f, 0.18f),
        "HEALTH",
        m_healthText,
        m_healthGauge,
        m_state.globalHealth,
        mix(getRiskColor(1.0f - m_state.globalHealth), bg, 0.92f));
    renderGaugeTile(
        bounds.horizontalSplit(0.20f, 0.38f),
        "THERMAL RES",
        m_thermalMarginText,
        m_thermalGauge,
        m_state.thermalMargin,
        mix(getRiskColor(1.0f - m_state.thermalMargin), bg, 0.95f));
    renderGaugeTile(
        bounds.horizontalSplit(0.40f, 0.58f),
        "LUBE RES",
        m_lubricationMarginText,
        m_lubricationGauge,
        m_state.lubricationMargin,
        mix(getRiskColor(1.0f - m_state.lubricationMargin), bg, 0.95f));
    renderGaugeTile(
        bounds.horizontalSplit(0.60f, 0.78f),
        "DET RES",
        m_detonationMarginText,
        m_detonationGauge,
        m_state.detonationMargin,
        mix(getRiskColor(1.0f - m_state.detonationMargin), bg, 0.95f));
    renderKpiTile(
        bounds.horizontalSplit(0.80f, 1.00f),
        "MODEL OUTPUT",
        m_rulText,
        m_damageRateText + "  //  CONF " + m_confidenceText,
        mix(m_app->getBlue(), bg, 0.95f),
        modeValueSize);
}

void EngineWearCluster::renderKpiTile(
    const Bounds &bounds,
    const std::string &label,
    const std::string &valueText,
    const std::string &detailText,
    const ysVector &fillColor,
    float valueSize)
{
    const Bounds tile = bounds.inset(1.0f);
    const float titleSize = std::clamp(tile.height() * 0.16f, 11.0f, 14.0f);
    const float detailSize = std::clamp(tile.height() * 0.14f, 10.0f, 13.0f);

    drawFrame(tile, 1.0f, m_app->getForegroundColor(), fillColor);
    drawText(label, tile.inset(8.0f).verticalSplit(0.76f, 1.0f), titleSize, Bounds::tl);
    drawAlignedText(valueText, tile.inset(8.0f).verticalSplit(0.28f, 0.72f), valueSize, Bounds::center, Bounds::center);
    drawAlignedText(detailText, tile.inset(8.0f).verticalSplit(0.04f, 0.18f), detailSize, Bounds::center, Bounds::center);
}

void EngineWearCluster::renderGaugeTile(
    const Bounds &bounds,
    const std::string &label,
    const std::string &valueText,
    Gauge *gauge,
    float gaugeValue,
    const ysVector &fillColor)
{
    const Bounds tile = bounds.inset(1.0f);
    const float titleSize = std::clamp(tile.height() * 0.12f, 10.0f, 13.0f);
    const float valueSize = std::clamp(tile.height() * 0.14f, 11.0f, 16.0f);
    const Bounds gaugeBounds = tile.inset(8.0f).verticalSplit(0.18f, 0.84f);

    drawFrame(tile, 1.0f, m_app->getForegroundColor(), fillColor);
    drawText(label, tile.inset(8.0f).verticalSplit(0.82f, 1.0f), titleSize, Bounds::tl);
    drawAlignedText(valueText, tile.inset(8.0f).verticalSplit(0.02f, 0.14f), valueSize, Bounds::center, Bounds::center);

    gauge->m_value = gaugeValue * 100.0f;
    gauge->m_bounds = gaugeBounds;
    gauge->m_outerRadius = std::fmin(gaugeBounds.width(), gaugeBounds.height()) * 0.5f;
    gauge->m_needleOuterRadius = gauge->m_outerRadius * 0.72f;
    gauge->m_needleInnerRadius = -gauge->m_outerRadius * 0.08f;
    gauge->render();
}

void EngineWearCluster::renderOverviewPanel(const Bounds &bounds) {
    const float titleSize = std::clamp(bounds.height() * 0.11f, 14.0f, 18.0f);
    const float bodySize = std::clamp(bounds.height() * 0.07f, 11.0f, 15.0f);
    const float modeSize = std::clamp(bounds.height() * 0.11f, 18.0f, 26.0f);

    drawText("INSPECTION FOCUS", bounds.verticalSplit(0.90f, 1.0f), titleSize, Bounds::tl);
    drawAlignedText(m_failureModeText, bounds.verticalSplit(0.64f, 0.82f), modeSize, Bounds::center, Bounds::center);
    drawAlignedText(getFailureModeAction(m_state.dominantFailureMode), bounds.verticalSplit(0.50f, 0.60f), bodySize, Bounds::center, Bounds::center);

    Grid infoGrid{ 1, 4 };
    const Bounds rows = bounds.verticalSplit(0.0f, 0.42f);
    drawInfoRow(infoGrid.get(rows, 0, 3), "TOP DAMAGE", getTopDamageLabel(), bodySize);
    drawInfoRow(infoGrid.get(rows, 0, 2), "TOP STRESS", getTopStressLabel(), bodySize);
    drawInfoRow(infoGrid.get(rows, 0, 1), "OIL TEMP", m_oilTemperatureText, bodySize);
    drawInfoRow(infoGrid.get(rows, 0, 0), "COOLANT", m_coolantTemperatureText, bodySize);
}

void EngineWearCluster::renderExposurePanel(const Bounds &bounds) {
    const float titleSize = std::clamp(bounds.height() * 0.12f, 14.0f, 18.0f);
    const float bodySize = std::clamp(bounds.height() * 0.10f, 11.0f, 15.0f);

    drawText("DUTY HISTORY", bounds.verticalSplit(0.88f, 1.0f), titleSize, Bounds::tl);

    Grid infoGrid{ 1, 6 };
    const Bounds rows = bounds.verticalSplit(0.0f, 0.82f);
    drawInfoRow(infoGrid.get(rows, 0, 5), "OVERREV", m_overRevText, bodySize);
    drawInfoRow(infoGrid.get(rows, 0, 4), "OVERTEMP", m_overTempText, bodySize);
    drawInfoRow(infoGrid.get(rows, 0, 3), "COLD LOAD", m_coldLoadText, bodySize);
    drawInfoRow(infoGrid.get(rows, 0, 2), "OIL STARVE", m_oilStarvationText, bodySize);
    drawInfoRow(infoGrid.get(rows, 0, 1), "SEV KNOCK", m_knockEventText, bodySize);
    drawInfoRow(infoGrid.get(rows, 0, 0), "HEAT CYC", m_thermalCyclesText, bodySize);
}

void EngineWearCluster::renderLiveStressPanel(const Bounds &bounds) {
    std::array<RankedMetric, 5> metrics = {{
        { "THERMAL RES", &m_thermalMarginText, m_state.thermalMargin, 1.0f - m_state.thermalMargin },
        { "LUBE RES", &m_lubricationMarginText, m_state.lubricationMargin, 1.0f - m_state.lubricationMargin },
        { "DET RES", &m_detonationMarginText, m_state.detonationMargin, 1.0f - m_state.detonationMargin },
        { "FATIGUE LOAD", &m_fatigueLoadText, m_state.mechanicalFatigueLoad, m_state.mechanicalFatigueLoad },
        { "COMB STAB", &m_combustionStabilityText, m_state.combustionStability, 1.0f - m_state.combustionStability }
    }};

    std::sort(metrics.begin(), metrics.end(), [](const RankedMetric &a, const RankedMetric &b) {
        return a.risk > b.risk;
    });

    const float titleSize = std::clamp(bounds.height() * 0.08f, 14.0f, 18.0f);
    drawText("OPERATING MARGINS", bounds.verticalSplit(0.90f, 1.0f), titleSize, Bounds::tl);

    Grid metricGrid{ 1, 5 };
    const Bounds rows = bounds.verticalSplit(0.0f, 0.84f);
    for (int i = 0; i < 5; ++i) {
        drawMetricRow(metricGrid.get(rows, 0, 4 - i), metrics[i].label, *metrics[i].valueText, metrics[i].ratio, metrics[i].risk);
    }
}

void EngineWearCluster::renderDamagePanel(const Bounds &bounds) {
    std::array<RankedMetric, 5> metrics = {{
        { "BOTTOM END", &m_bottomEndDamageText, m_state.bottomEndDamage, m_state.bottomEndDamage },
        { "RING SEAL", &m_ringSealDamageText, m_state.ringSealDamage, m_state.ringSealDamage },
        { "VALVETRAIN", &m_valvetrainDamageText, m_state.valvetrainDamage, m_state.valvetrainDamage },
        { "HEAD/GASKET", &m_headGasketDamageText, m_state.headGasketDamage, m_state.headGasketDamage },
        { "LUBE SYS", &m_lubricationDamageText, m_state.lubricationSystemDamage, m_state.lubricationSystemDamage }
    }};

    std::sort(metrics.begin(), metrics.end(), [](const RankedMetric &a, const RankedMetric &b) {
        return a.risk > b.risk;
    });

    const float titleSize = std::clamp(bounds.height() * 0.08f, 14.0f, 18.0f);
    drawText("ACCUMULATED DAMAGE", bounds.verticalSplit(0.90f, 1.0f), titleSize, Bounds::tl);

    Grid metricGrid{ 1, 5 };
    const Bounds rows = bounds.verticalSplit(0.0f, 0.84f);
    for (int i = 0; i < 5; ++i) {
        drawMetricRow(metricGrid.get(rows, 0, 4 - i), metrics[i].label, *metrics[i].valueText, metrics[i].ratio, metrics[i].risk);
    }
}

void EngineWearCluster::drawMetricRow(
    const Bounds &bounds,
    const std::string &label,
    const std::string &valueText,
    float displayRatio,
    float risk)
{
    const Bounds row = bounds.inset(2.0f);
    const Bounds barBounds = row.horizontalSplit(0.34f, 0.84f).verticalSplit(0.22f, 0.72f);
    const float textSize = std::clamp(row.height() * 0.42f, 11.0f, 15.0f);

    drawText(label, row.horizontalSplit(0.0f, 0.32f), textSize, Bounds::lm);
    drawAlignedText(valueText, row.horizontalSplit(0.86f, 1.0f), textSize, Bounds::rm, Bounds::rm);
    drawBox(barBounds, mix(m_app->getForegroundColor(), m_app->getBackgroundColor(), 0.85f));
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
        return "BALANCED";
    case EngineWearState::FailureMode::ThermalOverload:
        return "THERMAL OVERLOAD";
    case EngineWearState::FailureMode::LubricationBreakdown:
        return "LUBRICATION BREAKDOWN";
    case EngineWearState::FailureMode::DetonationDamage:
        return "DETONATION DAMAGE";
    case EngineWearState::FailureMode::BottomEndFatigue:
        return "BOTTOM-END FATIGUE";
    case EngineWearState::FailureMode::RingSealWear:
        return "RING SEAL WEAR";
    case EngineWearState::FailureMode::ValvetrainFatigue:
        return "VALVETRAIN FATIGUE";
    case EngineWearState::FailureMode::HeadGasketRisk:
        return "HEAD GASKET RISK";
    }

    return "BALANCED";
}

const char *EngineWearCluster::getFailureModeAction(EngineWearState::FailureMode mode) const {
    switch (mode) {
    case EngineWearState::FailureMode::Balanced:
        return "NO DOMINANT FAILURE VECTOR";
    case EngineWearState::FailureMode::ThermalOverload:
        return "TRACK COOLING RESERVE + HOT ZONES";
    case EngineWearState::FailureMode::LubricationBreakdown:
        return "CHECK OIL FILM + STARVATION RISK";
    case EngineWearState::FailureMode::DetonationDamage:
        return "REDUCE KNOCK ENERGY AT LOAD";
    case EngineWearState::FailureMode::BottomEndFatigue:
        return "INSPECT ROD AND MAIN BEARING LOAD";
    case EngineWearState::FailureMode::RingSealWear:
        return "CHECK COMPRESSION AND BLOW-BY TREND";
    case EngineWearState::FailureMode::ValvetrainFatigue:
        return "CHECK VALVE MOTION AND SEAT LOAD";
    case EngineWearState::FailureMode::HeadGasketRisk:
        return "CHECK HEAD CLAMP AND THERMAL GRADIENT";
    }

    return "NO DOMINANT FAILURE VECTOR";
}

const char *EngineWearCluster::getTopDamageLabel() const {
    const float damage[] = {
        m_state.bottomEndDamage,
        m_state.ringSealDamage,
        m_state.valvetrainDamage,
        m_state.headGasketDamage,
        m_state.lubricationSystemDamage
    };
    const char *labels[] = {
        "BOTTOM END",
        "RING SEAL",
        "VALVETRAIN",
        "HEAD/GASKET",
        "LUBE SYS"
    };

    int maxIndex = 0;
    for (int i = 1; i < 5; ++i) {
        if (damage[i] > damage[maxIndex]) {
            maxIndex = i;
        }
    }

    return labels[maxIndex];
}

const char *EngineWearCluster::getTopStressLabel() const {
    const float stress[] = {
        1.0f - m_state.thermalMargin,
        1.0f - m_state.lubricationMargin,
        1.0f - m_state.detonationMargin,
        m_state.mechanicalFatigueLoad,
        1.0f - m_state.combustionStability
    };
    const char *labels[] = {
        "THERMAL RESERVE",
        "LUBE RESERVE",
        "DET RESERVE",
        "FATIGUE LOAD",
        "COMB STABILITY"
    };

    int maxIndex = 0;
    for (int i = 1; i < 5; ++i) {
        if (stress[i] > stress[maxIndex]) {
            maxIndex = i;
        }
    }

    return labels[maxIndex];
}

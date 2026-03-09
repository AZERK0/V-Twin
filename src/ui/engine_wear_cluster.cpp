#include "ui/engine_wear_cluster.h"

#include "app/engine_sim_application.h"
#include "simulation/simulator.h"
#include "ui/ui_utilities.h"
#include "units.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace {
    float clamp01(float value) {
        return std::clamp(value, 0.0f, 1.0f);
    }

    float smoothValue(float current, float target, float dt, float response) {
        const float alpha = 1.0f - std::exp(-dt * response);
        return current + (target - current) * alpha;
    }

    std::string formatValue(float value, int precision, const std::string &suffix = "") {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(precision) << value << suffix;
        return stream.str();
    }
}

EngineWearCluster::EngineWearCluster() {
    m_simulator = nullptr;
    m_severityHistory.fill(0.0f);
    m_historyCount = 0;
    m_historyWriteIndex = 0;
    m_elapsedTime = 0.0f;
    m_engineHealth = 0.82f;
    m_wearRatePerMinute = 0.35f;
    m_thermalStress = 0.38f;
    m_lubricationReserve = 0.74f;
    m_knockExposure = 0.22f;
    m_bearingStress = 0.34f;
    m_ringSealLoss = 0.18f;
    m_valvetrainFatigue = 0.27f;
    m_headThermalLoad = 0.31f;
    m_oilTemperature = 97.0f;
    m_coolantTemperature = 91.0f;
    m_blowBy = 0.14f;
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
    updateFakeTelemetry(dt);
    UiElement::update(dt);
}

void EngineWearCluster::render() {
    renderSummary(m_bounds);
}

void EngineWearCluster::renderDashboard(const Bounds &bounds) {
    const Bounds inner = bounds.inset(16.0f);
    const Bounds left = inner.horizontalSplit(0.0f, 0.34f);
    const Bounds right = inner.horizontalSplit(0.34f, 1.0f);

    const Bounds healthBounds = left.verticalSplit(0.42f, 1.0f);
    const Bounds advisoryBounds = left.verticalSplit(0.0f, 0.38f);
    const Bounds liveFactorsBounds = right.verticalSplit(0.52f, 1.0f);
    const Bounds hotspotBounds = right.verticalSplit(0.0f, 0.48f);

    drawFrame(healthBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());
    drawFrame(advisoryBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());
    drawFrame(liveFactorsBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());
    drawFrame(hotspotBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    const Bounds healthInner = healthBounds.inset(14.0f);
    const Bounds healthTitle = healthInner.verticalSplit(0.78f, 1.0f);
    const Bounds healthBody = healthInner.verticalSplit(0.0f, 0.76f);
    drawText("EST. HEALTH", healthTitle, 20.0f, Bounds::tl);
    drawStatusBadge(healthTitle.horizontalSplit(0.55f, 1.0f), getConditionLabel(), getSeverityIndex());
    drawAlignedText(formatValue(m_engineHealth * 100.0f, 0, "%"), healthBody, 72.0f, Bounds::tm, Bounds::tm);
    drawAlignedText(
        formatValue(m_wearRatePerMinute, 2, "%/min") + " wear rate",
        healthBody.inset(8.0f).move({ 0.0f, -28.0f }),
        20.0f,
        Bounds::bm,
        Bounds::bm);

    const Bounds historyBounds = healthInner.verticalSplit(0.0f, 0.24f).inset(2.0f);
    drawHistoryChart(historyBounds);

    const Bounds advisoryInner = advisoryBounds.inset(14.0f);
    drawText("SERVICE WINDOW", advisoryInner.verticalSplit(0.8f, 1.0f), 20.0f, Bounds::tl);
    drawText(
        formatValue(m_engineHealth * 1800.0f, 0, " km estimated margin"),
        advisoryInner.verticalSplit(0.56f, 0.74f),
        18.0f,
        Bounds::tl);
    drawText(
        formatValue(m_oilTemperature, 0, " C oil") + " / "
            + formatValue(m_coolantTemperature, 0, " C coolant"),
        advisoryInner.verticalSplit(0.34f, 0.5f),
        18.0f,
        Bounds::tl);
    drawText(
        "Primary alert: " + getPrimaryAlert(),
        advisoryInner.verticalSplit(0.16f, 0.3f),
        18.0f,
        Bounds::tl);
    drawText(
        formatValue(m_blowBy * 100.0f, 0, "%") + " blow-by trend",
        advisoryInner.verticalSplit(0.0f, 0.14f),
        18.0f,
        Bounds::tl);

    const Bounds liveFactorsInner = liveFactorsBounds.inset(14.0f);
    drawText("LIVE FACTORS", liveFactorsInner.verticalSplit(0.84f, 1.0f), 20.0f, Bounds::tl);
    Grid liveGrid{ 1, 5 };
    drawMetricRow(liveGrid.get(liveFactorsInner.verticalSplit(0.0f, 0.8f), 0, 4), "THERMAL LOAD", m_thermalStress, m_thermalStress);
    drawMetricRow(liveGrid.get(liveFactorsInner.verticalSplit(0.0f, 0.8f), 0, 3), "OIL FILM", m_lubricationReserve, 1.0f - m_lubricationReserve);
    drawMetricRow(liveGrid.get(liveFactorsInner.verticalSplit(0.0f, 0.8f), 0, 2), "KNOCK EXPOSURE", m_knockExposure, m_knockExposure);
    drawMetricRow(liveGrid.get(liveFactorsInner.verticalSplit(0.0f, 0.8f), 0, 1), "BEARING LOAD", m_bearingStress, m_bearingStress);
    drawMetricRow(liveGrid.get(liveFactorsInner.verticalSplit(0.0f, 0.8f), 0, 0), "BLOW-BY", m_blowBy, m_blowBy);

    const Bounds hotspotInner = hotspotBounds.inset(14.0f);
    drawText("HOTSPOTS", hotspotInner.verticalSplit(0.84f, 1.0f), 20.0f, Bounds::tl);
    Grid hotspotGrid{ 1, 4 };
    drawMetricRow(hotspotGrid.get(hotspotInner.verticalSplit(0.0f, 0.8f), 0, 3), "BOTTOM END", m_bearingStress, m_bearingStress);
    drawMetricRow(hotspotGrid.get(hotspotInner.verticalSplit(0.0f, 0.8f), 0, 2), "RING PACK", m_ringSealLoss, m_ringSealLoss);
    drawMetricRow(hotspotGrid.get(hotspotInner.verticalSplit(0.0f, 0.8f), 0, 1), "VALVETRAIN", m_valvetrainFatigue, m_valvetrainFatigue);
    drawMetricRow(hotspotGrid.get(hotspotInner.verticalSplit(0.0f, 0.8f), 0, 0), "HEAD COOLING", m_headThermalLoad, m_headThermalLoad);
}

void EngineWearCluster::setSimulator(Simulator *simulator) {
    m_simulator = simulator;
}

void EngineWearCluster::updateFakeTelemetry(float dt) {
    m_elapsedTime += dt;

    const float rpmLoad = getRpmLoad();
    const float throttleLoad = getThrottleLoad();
    const float dynoBias = (m_simulator != nullptr && m_simulator->m_dyno.m_enabled) ? 0.12f : 0.0f;
    const float slowPulse = 0.5f + 0.5f * std::sin(m_elapsedTime * 0.35f);
    const float fastPulse = 0.5f + 0.5f * std::sin(m_elapsedTime * 1.7f);

    const float thermalTarget = clamp01(0.18f + 0.46f * rpmLoad + 0.28f * throttleLoad + 0.10f * slowPulse + dynoBias);
    const float lubricationTarget = clamp01(0.92f - 0.38f * rpmLoad - 0.18f * throttleLoad + 0.06f * std::cos(m_elapsedTime * 0.4f));
    const float knockTarget = clamp01(0.08f + 0.24f * throttleLoad + 0.22f * rpmLoad + 0.12f * fastPulse + dynoBias * 0.5f);
    const float bearingTarget = clamp01(0.14f + 0.42f * rpmLoad + 0.22f * throttleLoad + dynoBias);
    const float ringTarget = clamp01(0.08f + thermalTarget * 0.42f + throttleLoad * 0.18f + fastPulse * 0.08f);
    const float valvetrainTarget = clamp01(0.10f + rpmLoad * 0.54f + slowPulse * 0.06f);
    const float headTarget = clamp01(0.12f + thermalTarget * 0.62f + throttleLoad * 0.10f);
    const float blowByTarget = clamp01(0.04f + ringTarget * 0.46f + throttleLoad * 0.10f);
    const float oilTemperatureTarget = 82.0f + 46.0f * thermalTarget + 4.0f * std::sin(m_elapsedTime * 0.25f);
    const float coolantTemperatureTarget = 78.0f + 38.0f * headTarget + 3.0f * std::cos(m_elapsedTime * 0.18f);

    m_thermalStress = smoothValue(m_thermalStress, thermalTarget, dt, 2.0f);
    m_lubricationReserve = smoothValue(m_lubricationReserve, lubricationTarget, dt, 1.8f);
    m_knockExposure = smoothValue(m_knockExposure, knockTarget, dt, 2.8f);
    m_bearingStress = smoothValue(m_bearingStress, bearingTarget, dt, 2.2f);
    m_ringSealLoss = smoothValue(m_ringSealLoss, ringTarget, dt, 2.0f);
    m_valvetrainFatigue = smoothValue(m_valvetrainFatigue, valvetrainTarget, dt, 1.9f);
    m_headThermalLoad = smoothValue(m_headThermalLoad, headTarget, dt, 1.7f);
    m_blowBy = smoothValue(m_blowBy, blowByTarget, dt, 2.4f);
    m_oilTemperature = smoothValue(m_oilTemperature, oilTemperatureTarget, dt, 1.6f);
    m_coolantTemperature = smoothValue(m_coolantTemperature, coolantTemperatureTarget, dt, 1.4f);

    const float severity = getSeverityIndex();
    const float wearRateTarget = 0.06f + severity * 1.42f;
    m_wearRatePerMinute = smoothValue(m_wearRatePerMinute, wearRateTarget, dt, 2.1f);
    m_engineHealth = clamp01(m_engineHealth - severity * dt * 0.00085f);

    pushHistorySample(severity);
}

void EngineWearCluster::renderSummary(const Bounds &bounds) {
    drawFrame(bounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    const Bounds inner = bounds.inset(12.0f);
    const Bounds title = inner.verticalSplit(0.76f, 1.0f);
    const Bounds body = inner.verticalSplit(0.0f, 0.74f);

    drawText("ENGINE WEAR", title, 22.0f, Bounds::tl);
    drawAlignedText("[J] MODE", title, 16.0f, Bounds::tr, Bounds::tr);

    const Bounds healthBounds = body.verticalSplit(0.54f, 1.0f);
    Bounds healthStatusBounds = healthBounds;
    healthStatusBounds.move({ 0.0f, -32.0f });
    drawAlignedText(formatValue(m_engineHealth * 100.0f, 0, "%"), healthBounds, 52.0f, Bounds::tm, Bounds::tm);
    drawAlignedText(getConditionLabel(), healthStatusBounds, 18.0f, Bounds::bm, Bounds::bm);

    const Bounds severityBounds = body.verticalSplit(0.38f, 0.5f);
    drawFrame(severityBounds, 1.0f, mix(m_app->getForegroundColor(), m_app->getBackgroundColor(), 0.55f), m_app->getBackgroundColor());
    drawBox(
        Bounds(severityBounds.width() * getSeverityIndex(), severityBounds.height(), severityBounds.getPosition(Bounds::bl), Bounds::bl).inset(1.0f),
        getSeverityColor(getSeverityIndex()));
    drawText("CURRENT SEVERITY", severityBounds.inset(4.0f).move({ 0.0f, 10.0f }), 14.0f, Bounds::tl);
    drawAlignedText(formatValue(m_wearRatePerMinute, 2, "%/min"), severityBounds.inset(4.0f), 14.0f, Bounds::br, Bounds::br);

    Grid metricGrid{ 1, 3 };
    const Bounds metricsBounds = body.verticalSplit(0.0f, 0.32f);
    drawMetricRow(metricGrid.get(metricsBounds, 0, 2), "THERMAL", m_thermalStress, m_thermalStress);
    drawMetricRow(metricGrid.get(metricsBounds, 0, 1), "OIL FILM", m_lubricationReserve, 1.0f - m_lubricationReserve);
    drawMetricRow(metricGrid.get(metricsBounds, 0, 0), "KNOCK", m_knockExposure, m_knockExposure);
}

void EngineWearCluster::drawMetricRow(
    const Bounds &bounds,
    const std::string &label,
    float displayValue,
    float severity)
{
    const Bounds row = bounds.inset(2.0f);
    const Bounds labelBounds = row.horizontalSplit(0.0f, 0.28f);
    const Bounds valueBounds = row.horizontalSplit(0.84f, 1.0f);
    const Bounds barBounds = row.horizontalSplit(0.3f, 0.82f).verticalSplit(0.18f, 0.82f);
    const ysVector barColor = getSeverityColor(severity);
    const ysVector frameColor = mix(m_app->getForegroundColor(), m_app->getBackgroundColor(), 0.6f);

    drawText(label, labelBounds, 16.0f, Bounds::lm);
    drawAlignedText(formatValue(displayValue * 100.0f, 0, "%"), valueBounds, 16.0f, Bounds::rm, Bounds::rm);
    drawFrame(barBounds, 1.0f, frameColor, m_app->getBackgroundColor());
    drawBox(
        Bounds(barBounds.width() * clamp01(displayValue), barBounds.height(), barBounds.getPosition(Bounds::bl), Bounds::bl).inset(1.0f),
        barColor);
}

void EngineWearCluster::drawHistoryChart(const Bounds &bounds) {
    drawFrame(bounds, 1.0f, mix(m_app->getForegroundColor(), m_app->getBackgroundColor(), 0.55f), m_app->getBackgroundColor());

    const Bounds inner = bounds.inset(6.0f);
    const int sampleCount = std::max(m_historyCount, 1);
    const float slotWidth = inner.width() / sampleCount;

    for (int i = 0; i < sampleCount; ++i) {
        const int historyIndex = (m_historyWriteIndex - sampleCount + i + HistorySamples) % HistorySamples;
        const float sample = clamp01(m_severityHistory[historyIndex]);
        const float width = std::max(slotWidth - 2.0f, 1.0f);
        const float height = std::max(sample * inner.height(), 2.0f);
        const float x = inner.left() + i * slotWidth;
        drawBox(Bounds(width, height, { x, inner.bottom() }, Bounds::bl), getSeverityColor(sample));
    }

    drawText("wear history", inner, 14.0f, Bounds::tl);
}

void EngineWearCluster::drawStatusBadge(const Bounds &bounds, const std::string &label, float severity) {
    const Bounds badgeBounds = bounds.verticalSplit(0.2f, 0.78f);
    drawBox(badgeBounds, mix(getSeverityColor(severity), m_app->getBackgroundColor(), 0.25f));
    drawFrame(badgeBounds, 1.0f, getSeverityColor(severity), m_app->getBackgroundColor(), false);
    drawAlignedText(label, badgeBounds, 16.0f, Bounds::center, Bounds::center);
}

void EngineWearCluster::pushHistorySample(float sample) {
    m_severityHistory[m_historyWriteIndex] = sample;
    m_historyWriteIndex = (m_historyWriteIndex + 1) % HistorySamples;
    m_historyCount = std::min(m_historyCount + 1, HistorySamples);
}

float EngineWearCluster::getRpmLoad() const {
    if (m_simulator == nullptr || m_simulator->getEngine() == nullptr) {
        return 0.25f;
    }

    const double redline = std::max(m_simulator->getEngine()->getRedline(), units::rpm(1000));
    return clamp01(static_cast<float>(std::abs(m_simulator->getEngine()->getRpm()) / redline));
}

float EngineWearCluster::getThrottleLoad() const {
    if (m_simulator == nullptr || m_simulator->getEngine() == nullptr) {
        return 0.3f;
    }

    return clamp01(static_cast<float>(m_simulator->getEngine()->getThrottle()));
}

float EngineWearCluster::getSeverityIndex() const {
    return clamp01(
        0.28f * m_thermalStress
        + 0.22f * (1.0f - m_lubricationReserve)
        + 0.18f * m_knockExposure
        + 0.14f * m_bearingStress
        + 0.10f * m_ringSealLoss
        + 0.08f * m_valvetrainFatigue);
}

ysVector EngineWearCluster::getSeverityColor(float value) const {
    const float clamped = clamp01(value);
    if (clamped < 0.33f) {
        return mix(m_app->getGreen(), m_app->getYellow(), clamped / 0.33f);
    }
    else if (clamped < 0.66f) {
        return mix(m_app->getYellow(), m_app->getOrange(), (clamped - 0.33f) / 0.33f);
    }

    return mix(m_app->getOrange(), m_app->getRed(), (clamped - 0.66f) / 0.34f);
}

std::string EngineWearCluster::getConditionLabel() const {
    if (m_engineHealth > 0.78f) {
        return "STABLE";
    }
    else if (m_engineHealth > 0.6f) {
        return "WATCH";
    }
    else if (m_engineHealth > 0.4f) {
        return "AT RISK";
    }

    return "CRITICAL";
}

std::string EngineWearCluster::getPrimaryAlert() const {
    const float hotspots[] = {
        m_bearingStress,
        m_ringSealLoss,
        m_valvetrainFatigue,
        m_headThermalLoad
    };
    const char *labels[] = {
        "bottom-end fatigue",
        "ring seal erosion",
        "valvetrain loading",
        "head heat soak"
    };

    int maxIndex = 0;
    for (int i = 1; i < 4; ++i) {
        if (hotspots[i] > hotspots[maxIndex]) {
            maxIndex = i;
        }
    }

    return labels[maxIndex];
}

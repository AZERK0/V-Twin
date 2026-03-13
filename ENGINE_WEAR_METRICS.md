# Engine Wear Metrics

This document explains the engine wear metrics currently exposed by the fake-data wear model.

The current implementation is fake-data driven, but each metric is intentionally designed to map to a future real-world signal, estimation, or degradation model.

Relevant code:

- `include/simulation/engine_wear_model.h`
- `src/simulation/engine_wear_model.cpp`
- `include/ui/engine_wear_cluster.h`
- `src/ui/engine_wear_cluster.cpp`

## Philosophy

The wear system is split into three layers:

- `EngineWearModel`: computes wear-related state from simulation signals
- `EngineWearState`: immutable snapshot published to the UI
- `EngineWearCluster`: renders the snapshot without doing wear logic

The metrics are grouped into four engineering questions:

1. What is the current health state?
2. What is stressing the engine right now?
3. What damage has already accumulated?
4. What operating history explains that damage?

## KPI Strip

### Health

- Meaning: overall state of health of the engine model
- Range: `0% -> 100%`
- Interpretation:
  - `100%` means no effective damage accumulated yet
  - lower values mean the weighted subsystem damage is increasing
- Current model: derived from weighted accumulated damage across bottom end, ring seal, valvetrain, head/gasket, and lubrication system

### Thermal Res

- Meaning: remaining thermal reserve before thermal loading becomes critical
- Range: `0% -> 100%`
- Interpretation:
  - high = thermal operating point is healthy
  - low = chamber temperature, pressure, rpm, and cooling stress are converging toward risk
- Future real mapping:
  - coolant temp
  - oil temp
  - cylinder temp estimate
  - thermal gradients / hot spots

### Lube Res

- Meaning: remaining lubrication reserve before oil film quality becomes critical
- Range: `0% -> 100%`
- Interpretation:
  - high = oil thermal window and loading are acceptable
  - low = increased risk of film collapse, starvation, or boundary friction
- Future real mapping:
  - oil temp
  - oil pressure
  - load and rpm
  - cold high-load events

### Det Res

- Meaning: remaining detonation reserve before combustion becomes knock-limited
- Range: `0% -> 100%`
- Interpretation:
  - high = combustion is comfortably away from knock risk
  - low = pressure, temperature, load, and lean behavior are pushing the engine toward detonation damage
- Future real mapping:
  - knock sensor activity
  - ignition corrections
  - lambda / AFR
  - boost / manifold pressure

### Model Output

This tile summarizes the predictive side of the model.

#### RUL

- Meaning: remaining useful life at the current modeled duty severity
- Unit: hours
- Interpretation:
  - not absolute calendar life
  - it is a projected life horizon if the current wear regime continues

#### Damage Rate

- Meaning: current rate of degradation accumulation
- Unit: `%/h`
- Interpretation:
  - low = current operating point is sustainable
  - high = the engine is consuming life quickly right now

#### Confidence

- Meaning: trust level in the metric package
- Range: `0% -> 100%`
- Current model: fixed at `100%`
- Future use:
  - simulator mode can stay near `100%`
  - OBD twin mode can reduce confidence when values are inferred rather than directly observed

## Inspection Focus

This panel is intended to answer: where should an engineer look first?

### Primary Mode

- Meaning: dominant failure vector selected by the model
- Possible modes:
  - `BALANCED`
  - `THERMAL OVERLOAD`
  - `LUBRICATION BREAKDOWN`
  - `DETONATION DAMAGE`
  - `BOTTOM-END FATIGUE`
  - `RING SEAL WEAR`
  - `VALVETRAIN FATIGUE`
  - `HEAD GASKET RISK`

The model picks the dominant mode from a set of weighted scores combining instantaneous margins and accumulated subsystem damage.

### Top Damage

- Meaning: subsystem with the highest accumulated damage right now
- Purpose: tells you what is already most degraded

### Top Stress

- Meaning: subsystem or operating dimension with the strongest current stress contribution
- Purpose: tells you what is currently attacking durability the hardest

### Oil Temp / Coolant

- Meaning: current modeled temperatures used by the wear system
- Purpose: quick thermal context for the focus diagnosis

## Operating Margins

This block shows live stress metrics. These are not damage yet. They are the current operating reserves or loads that drive future damage.

### Thermal Reserve

- Meaning: thermal safety margin
- High is good
- Low means temperature, pressure, rpm, and duty severity are consuming cooling headroom

### Lube Reserve

- Meaning: lubrication safety margin
- High is good
- Low means oil temperature window, load, and fatigue demand are reducing lubrication robustness

### Det Reserve

- Meaning: detonation safety margin
- High is good
- Low means combustion conditions are nearing knock-prone territory

### Fatigue Load

- Meaning: current mechanical fatigue loading
- High is bad
- Built from rpm, pressure load, manifold load, and friction load
- This is closer to a structural duty indicator than a failure indicator

### Comb Stability

- Meaning: combustion stability margin
- High is good
- Low means AFR error, pressure spread, throttle transients, or lean behavior are increasing combustion variability

## Accumulated Damage

This block answers: what has already been spent from the engine life budget?

Each metric is normalized between `0%` and `100%`.

### Bottom End

- Scope: crankshaft, rods, mains, bearing support path
- Driven by:
  - fatigue load
  - lubrication degradation
  - detonation contribution

### Ring Seal

- Scope: piston rings, sealing quality, blow-by related wear
- Driven by:
  - thermal stress
  - detonation stress
  - combustion instability
  - pressure load

### Valvetrain

- Scope: cam, followers, springs, seats, valve train fatigue path
- Driven by:
  - fatigue load
  - thermal stress
  - combustion instability

### Head / Gasket

- Scope: head thermal integrity and gasket risk path
- Driven by:
  - thermal stress
  - detonation stress
  - combustion instability

### Lube Sys

- Scope: lubrication system degradation and margin erosion
- Driven by:
  - lubrication stress
  - thermal stress
  - fatigue demand

## Duty History

This block captures operating history that explains why wear is accumulating.

### Overrev

- Meaning: cumulative time spent near redline / overspeed zone
- Why it matters: increases inertial loading and fatigue demand

### Overtemp

- Meaning: cumulative time above critical oil or coolant thresholds
- Why it matters: increases thermal degradation and head/gasket risk

### Cold Load

- Meaning: cumulative time under high load while oil is still cold
- Why it matters: bad lubrication conditions during high mechanical demand

### Oil Starve

- Meaning: cumulative time with very low lubrication margin
- Why it matters: can accelerate bearing and bottom-end damage sharply

### Sev Knock

- Meaning: count of severe knock episodes
- Why it matters: repeated strong knock events can rapidly damage ring lands, head/gasket, and combustion surfaces

### Heat Cyc

- Meaning: count of thermal cycles detected by the wear model
- Why it matters: thermal cycling is relevant to long-term fatigue, sealing integrity, and structural durability

## How the current fake model works

The current fake-data model is still physically inspired. It is not random UI noise.

It uses live simulation signals such as:

- rpm and redline ratio
- throttle and throttle rate
- manifold pressure
- intake AFR
- per-cylinder pressure
- per-cylinder temperature
- chamber friction force
- dyno load bias

From those signals it builds:

- live margins
- damage rates for each subsystem
- accumulated normalized damage
- dominant failure mode
- engineering exposure counters

Model cadence:

- wear model update: `50 Hz`
- UI snapshot publication: `10 Hz`

## Reading guidance

For durability engineering, the intended reading order is:

1. `Health` and `Damage Rate`
2. `Primary Mode`
3. `Operating Margins`
4. `Accumulated Damage`
5. `Duty History`

This gives a practical workflow:

- identify current severity
- identify the dominant failure vector
- identify what is currently driving it
- identify what has already degraded
- verify the operating history that explains it

## Current limitations

- all metrics are still fake-data outputs
- `Confidence` is currently fixed to `100%`
- `RUL` is a model projection, not a validated lifetime estimator
- thresholds and weights are placeholders intended to be replaced or calibrated later

## Intended future evolution

The current metric package is already shaped for two target use cases:

### 1. Prototype durability / constructor use

- direct access to internal simulation signals
- rich subsystem breakdown
- failure-mode-oriented diagnosis

### 2. Digital twin / OBD-connected engine

- same output metrics
- lower confidence on inferred metrics
- stronger distinction between measured and estimated signals

The goal is to keep the same top-level engineering language in both modes, while changing only the data source and the confidence level behind the metrics.

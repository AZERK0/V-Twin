#ifndef ATG_ENGINE_SIM_ENGINE_NODE_H
#define ATG_ENGINE_SIM_ENGINE_NODE_H

#include "object_reference_node.h"

#include "crankshaft_node.h"
#include "cylinder_bank_node.h"
#include "ignition_module_node.h"
#include "engine_context.h"
#include "fuel_node.h"
#include "throttle_nodes.h"

#include "engine_sim.h"

#include <map>
#include <set>

namespace es_script {

    class EngineNode : public ObjectReferenceNode<EngineNode> {
    public:
        EngineNode() { /* void */ }
        virtual ~EngineNode() { /* void */ }

        void buildEngine(Engine *engine) {
            int cylinderCount = 0;
            for (const CylinderBankNode *bank : m_cylinderBanks) {
                cylinderCount += bank->getCylinderCount();
            }

            std::set<ExhaustSystemNode *> exhaustSystems;
            std::set<IntakeNode *> intakes;
            for (const CylinderBankNode *bank : m_cylinderBanks) {
                const int n = bank->getCylinderCount();
                for (int i = 0; i < n; ++i) {
                    exhaustSystems.insert(bank->getCylinder(i).exhaust);
                    intakes.insert(bank->getCylinder(i).intake);
                }
            }

            EngineContext context;
            context.setEngine(engine);

            Engine::Parameters parameters = m_parameters;
            parameters.crankshaftCount = (int)m_crankshafts.size();
            parameters.cylinderBanks = (int)m_cylinderBanks.size();
            parameters.cylinderCount = cylinderCount;
            parameters.exhaustSystemCount = (int)exhaustSystems.size();
            parameters.intakeCount = (int)intakes.size();
            parameters.throttle = m_throttle->generate();
            engine->initialize(parameters);

            {
                int i = 0;
                for (ExhaustSystemNode *exhaust : exhaustSystems) {
                    context.addExhaust(
                        exhaust, engine->getExhaustSystem(i++));
                }
            }

            {
                int i = 0;
                for (IntakeNode *intake : intakes) {
                    context.addIntake(
                        intake, engine->getIntake(i++));
                }
            }

            {
                int i = 0;
                for (const CylinderBankNode *bank : m_cylinderBanks) {
                    context.addHead(bank->getCylinderHead(), engine->getHead(i++));
                }
            }

            for (const CylinderBankNode *bank : m_cylinderBanks) {
                const int n = bank->getCylinderCount();
                for (int i = 0; i < n; ++i) {
                    exhaustSystems.insert(bank->getCylinder(i).exhaust);
                    intakes.insert(bank->getCylinder(i).intake);
                }
            }

            for (int i = 0; i < parameters.crankshaftCount; ++i) {
                m_crankshafts[i]->generate(engine->getCrankshaft(i), &context);
            }

            for (int i = 0; i < parameters.cylinderBanks; ++i) {
                m_cylinderBanks[i]->indexSlaveJournals(&context);
            }

            int cylinderIndex = 0;
            for (int i = 0; i < parameters.cylinderBanks; ++i) {
                m_cylinderBanks[i]->generate(
                    i,
                    cylinderIndex,
                    engine->getCylinderBank(i),
                    engine->getCrankshaft(0),
                    engine,
                    &context);
                cylinderIndex += m_cylinderBanks[i]->getCylinderCount();
            }

            for (int i = 0; i < parameters.cylinderBanks; ++i) {
                m_cylinderBanks[i]->connectRodAssemblies(&context);
            }

            m_ignitionModule->generate(engine, &context);
            
            Function *meanPistonSpeedToTurbulence = new Function;
            meanPistonSpeedToTurbulence->initialize(30, 1);
            for (int i = 0; i < 30; ++i) {
                const double s = (double)i;
                meanPistonSpeedToTurbulence->addSample(s, s * 0.5);
            }

            Fuel *fuel = engine->getFuel();
            m_fuel->generate(fuel, &context);

            CombustionChamber::Parameters ccParams;
            ccParams.CrankcasePressure = units::pressure(1.0, units::atm);
            ccParams.fuel = fuel;
            ccParams.StartingPressure = units::pressure(1.0, units::atm);
            ccParams.StartingTemperature = units::celcius(25.0);
            ccParams.MeanPistonSpeedToTurbulence = meanPistonSpeedToTurbulence;

            for (int i = 0; i < engine->getCylinderCount(); ++i) {
                ccParams.piston = engine->getPiston(i);
                ccParams.Head = engine->getHead(ccParams.piston->getCylinderBank()->getIndex());
                engine->getChamber(i)->initialize(ccParams);
            }
        }

        void addCrankshaft(CrankshaftNode *crankshaft) {
            m_crankshafts.push_back(crankshaft);
        }

        void addCylinderBank(CylinderBankNode *bank) {
            m_cylinderBanks.push_back(bank);
        }

        int getIgnitionModuleCount() const {
            return m_ignitionModule == nullptr
                ? 0
                : 1;
        }

        void addIgnitionModule(IgnitionModuleNode *ignitionModule) {
            m_ignitionModule = ignitionModule;
        }

    protected:
        virtual void registerInputs() {
            addInput("name", &m_parameters.name);
            addInput("starter_torque", &m_parameters.starterTorque);
            addInput("starter_speed", &m_parameters.starterSpeed);
            addInput("dyno_min_speed", &m_parameters.dynoMinSpeed);
            addInput("dyno_max_speed", &m_parameters.dynoMaxSpeed);
            addInput("dyno_hold_step", &m_parameters.dynoHoldStep);
            addInput("redline", &m_parameters.redline);
            addInput("fuel", &m_fuel, InputTarget::Type::Object);
            addInput("throttle", &m_throttle, InputTarget::Type::Object);
            addInput("simulation_frequency", &m_parameters.initialSimulationFrequency);
            addInput("hf_gain", &m_parameters.initialHighFrequencyGain);
            addInput("jitter", &m_parameters.initialJitter);
            addInput("noise", &m_parameters.initialNoise);
            registerThermalInputs();

            ObjectReferenceNode<EngineNode>::registerInputs();
        }

        void registerThermalInputs() {
            EngineThermalParameters &thermal = m_parameters.thermalParameters;
            addInput("thermal_ambient_temperature", &thermal.ambientTemperatureK);
            addInput("thermal_piston_specific_heat", &thermal.pistonSpecificHeatJPerKgK);
            addInput("thermal_cylinder_capacity", &thermal.cylinderThermalCapacityJPerK);
            addInput("thermal_oil_volume", &thermal.oilVolumeM3);
            addInput("thermal_oil_density", &thermal.oilDensityKgPerM3);
            addInput("thermal_oil_specific_heat", &thermal.oilSpecificHeatJPerKgK);
            addInput("thermal_piston_cylinder_conductance", &thermal.pistonCylinderConductanceWPerK);
            addInput("thermal_piston_oil_conductance", &thermal.pistonOilConductanceWPerK);
            addInput("thermal_cylinder_oil_conductance", &thermal.cylinderOilConductanceWPerK);
            addInput("thermal_cylinder_ambient_conductance", &thermal.cylinderAmbientConductanceWPerK);
            addInput("thermal_oil_ambient_conductance", &thermal.oilAmbientConductanceWPerK);
            addInput("thermal_piston_friction_fraction", &thermal.pistonFrictionHeatFraction);
            addInput("thermal_cylinder_friction_fraction", &thermal.cylinderFrictionHeatFraction);
            addInput("thermal_oil_friction_fraction", &thermal.oilFrictionHeatFraction);
            addInput("thermal_piston_surface_factor", &thermal.pistonGasSurfaceFactor);
            addInput("thermal_cylinder_surface_factor", &thermal.cylinderGasSurfaceFactor);
            addInput("thermal_piston_transfer_factor", &thermal.pistonGasHeatTransferFactor);
            addInput("thermal_cylinder_transfer_factor", &thermal.cylinderGasHeatTransferFactor);
            addInput("thermal_hohenberg_coefficient", &thermal.hohenbergCoefficient);
            addInput("thermal_maximum_transfer_coefficient", &thermal.maximumHeatTransferCoefficientWPerM2K);
            addInput("thermal_update_interval", &thermal.updateIntervalSeconds);
        }

        virtual void _evaluate() {
            setOutput(this);

            // Read inputs
            readAllInputs();
        }

        ThrottleNode *m_throttle = nullptr;
        IgnitionModuleNode *m_ignitionModule = nullptr;
        FuelNode *m_fuel = nullptr;

        Engine::Parameters m_parameters;
        std::vector<CrankshaftNode *> m_crankshafts;
        std::vector<CylinderBankNode *> m_cylinderBanks;
    };

} /* namespace es_script */

#endif /* ATG_ENGINE_SIM_ENGINE_NODE_H */

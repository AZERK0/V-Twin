#include "../include/engine_sim_application.h"

#include "../include/combustion_chamber_object.h"
#include "../include/connecting_rod_object.h"
#include "../include/crankshaft_object.h"
#include "../include/cylinder_bank_object.h"
#include "../include/cylinder_head_object.h"
#include "../include/exhaust_system.h"
#include "../include/piston_object.h"

#include "../scripting/include/compiler.h"

#include <fstream>

void EngineSimApplication::loadEngine(
    Engine *engine,
    Vehicle *vehicle,
    Transmission *transmission)
{
    destroyObjects();

    if (m_simulator != nullptr) {
        m_simulator->releaseSimulation();
        delete m_simulator;
    }

    if (m_vehicle != nullptr) {
        delete m_vehicle;
        m_vehicle = nullptr;
    }

    if (m_transmission != nullptr) {
        delete m_transmission;
        m_transmission = nullptr;
    }

    if (m_iceEngine != nullptr) {
        m_iceEngine->destroy();
        delete m_iceEngine;
    }

    if (engine == nullptr || vehicle == nullptr || transmission == nullptr) {
        m_iceEngine = nullptr;
        m_vehicle = nullptr;
        m_transmission = nullptr;
        m_simulator = nullptr;
        m_viewParameters.Layer1 = 0;
        return;
    }

    m_iceEngine = engine;
    m_vehicle = vehicle;
    m_transmission = transmission;
    m_simulator = engine->createSimulator(vehicle, transmission);

    createObjects(engine);

    m_viewParameters.Layer1 = engine->getMaxDepth();
    engine->calculateDisplacement();

    m_simulator->setSimulationFrequency(engine->getSimulationFrequency());

    Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
    audioParams.inputSampleNoise = static_cast<float>(engine->getInitialJitter());
    audioParams.airNoise = static_cast<float>(engine->getInitialNoise());
    audioParams.dF_F_mix = static_cast<float>(engine->getInitialHighFrequencyGain());
    m_simulator->synthesizer().setAudioParameters(audioParams);

    initializeImpulseResponses(engine);
    m_simulator->startAudioRenderingThread();
}

void EngineSimApplication::createObjects(Engine *engine) {
    for (int i = 0; i < engine->getCylinderCount(); ++i) {
        ConnectingRodObject *rodObject = new ConnectingRodObject;
        rodObject->initialize(this);
        rodObject->m_connectingRod = engine->getConnectingRod(i);
        m_objects.push_back(rodObject);

        PistonObject *pistonObject = new PistonObject;
        pistonObject->initialize(this);
        pistonObject->m_piston = engine->getPiston(i);
        m_objects.push_back(pistonObject);

        CombustionChamberObject *chamberObject = new CombustionChamberObject;
        chamberObject->initialize(this);
        chamberObject->m_chamber = engine->getChamber(i);
        m_objects.push_back(chamberObject);
    }

    for (int i = 0; i < engine->getCrankshaftCount(); ++i) {
        CrankshaftObject *crankshaftObject = new CrankshaftObject;
        crankshaftObject->initialize(this);
        crankshaftObject->m_crankshaft = engine->getCrankshaft(i);
        m_objects.push_back(crankshaftObject);
    }

    for (int i = 0; i < engine->getCylinderBankCount(); ++i) {
        CylinderBankObject *bankObject = new CylinderBankObject;
        bankObject->initialize(this);
        bankObject->m_bank = engine->getCylinderBank(i);
        bankObject->m_head = engine->getHead(i);
        m_objects.push_back(bankObject);

        CylinderHeadObject *headObject = new CylinderHeadObject;
        headObject->initialize(this);
        headObject->m_head = engine->getHead(i);
        headObject->m_engine = engine;
        m_objects.push_back(headObject);
    }
}

void EngineSimApplication::destroyObjects() {
    for (SimulationObject *object : m_objects) {
        object->destroy();
        delete object;
    }

    m_objects.clear();
}

void EngineSimApplication::loadDefaultVehicle(Vehicle **vehicle) const {
    Vehicle::Parameters parameters;
    parameters.mass = units::mass(1597, units::kg);
    parameters.diffRatio = 3.42;
    parameters.tireRadius = units::distance(10, units::inch);
    parameters.dragCoefficient = 0.25;
    parameters.crossSectionArea = units::distance(6.0, units::foot) * units::distance(6.0, units::foot);
    parameters.rollingResistance = 2000.0;

    *vehicle = new Vehicle;
    (*vehicle)->initialize(parameters);
}

void EngineSimApplication::loadDefaultTransmission(Transmission **transmission) const {
    static const double gearRatios[] = { 2.97, 2.07, 1.43, 1.00, 0.84, 0.56 };

    Transmission::Parameters parameters;
    parameters.GearCount = 6;
    parameters.GearRatios = gearRatios;
    parameters.MaxClutchTorque = units::torque(1000.0, units::ft_lb);

    *transmission = new Transmission;
    (*transmission)->initialize(parameters);
}

void EngineSimApplication::initializeImpulseResponses(Engine *engine) {
    for (int i = 0; i < engine->getExhaustSystemCount(); ++i) {
        ImpulseResponse *response = engine->getExhaustSystem(i)->getImpulseResponse();

        ysAudioWaveFile waveFile;
        waveFile.OpenFile(response->getFilename().c_str());
        waveFile.InitializeInternalBuffer(waveFile.GetSampleCount());
        waveFile.FillBuffer(0);
        waveFile.CloseFile();

        m_simulator->synthesizer().initializeImpulseResponse(
            reinterpret_cast<const int16_t *>(waveFile.GetBuffer()),
            waveFile.GetSampleCount(),
            response->getVolume(),
            i);

        waveFile.DestroyInternalBuffer();
    }
}

void EngineSimApplication::loadScript() {
    Engine *engine = nullptr;
    Vehicle *vehicle = nullptr;
    Transmission *transmission = nullptr;

#ifdef ATG_ENGINE_SIM_PIRANHA_ENABLED
    std::vector<piranha::IrPath> searchPaths;
    searchPaths.push_back(m_userData.Append("assets").ToString());
    searchPaths.push_back(m_dataRoot.Append("assets").ToString());
    searchPaths.push_back(m_dataRoot.Append("es").ToString());

    std::vector<piranha::IrPath> dataPaths;
    dataPaths.push_back(m_userData.ToString());
    dataPaths.push_back(m_dataRoot.ToString());

    piranha::IrPath scriptPath;
    bool scriptFound = false;

    if (!m_scriptPathOverride.empty()) {
        const piranha::IrPath overridePath(m_scriptPathOverride);
        if (overridePath.exists()) {
            scriptPath = overridePath;
            scriptFound = true;
        }
        else {
            for (const auto &dataPath : dataPaths) {
                const auto candidate = dataPath.append(m_scriptPathOverride);
                if (candidate.exists()) {
                    scriptPath = candidate;
                    scriptFound = true;
                    break;
                }
            }
        }
    }
    else {
        for (const auto &dataPath : dataPaths) {
            const auto candidate = dataPath.append("assets/main.mr");
            if (candidate.exists()) {
                scriptPath = candidate;
                scriptFound = true;
                break;
            }
        }
    }

    if (scriptFound) {
        const auto outputLogPath = m_outputPath.Append("error_log.log").ToString();
        std::ofstream outputLog(outputLogPath, std::ios::out);

        std::string compileScriptPath = scriptPath.toString();
        if (!m_scriptPathOverride.empty()) {
            std::string importPath = compileScriptPath;
            const std::string assetsPrefix = m_dataRoot.Append("assets").ToString() + "/";

            if (importPath.rfind(assetsPrefix, 0) == 0) {
                importPath = importPath.substr(assetsPrefix.size());
            }
            else if (importPath.rfind("assets/", 0) == 0) {
                importPath = importPath.substr(7);
            }

            compileScriptPath = m_outputPath.Append("script_override_main.mr").ToString();
            std::ofstream overrideScript(compileScriptPath, std::ios::out);
            overrideScript
                << "import \"engine_sim.mr\"\n"
                << "import \"themes/default.mr\"\n"
                << "import \"" << importPath << "\"\n\n"
                << "use_default_theme()\n"
                << "main()\n";
        }

        es_script::Compiler compiler;
        compiler.initialize(searchPaths);
        const bool compiled = compiler.compile(compileScriptPath, outputLog);
        if (compiled) {
            const es_script::Compiler::Output output = compiler.execute();
            configure(output.applicationSettings);
            engine = output.engine;
            vehicle = output.vehicle;
            transmission = output.transmission;
        }

        compiler.destroy();
    }
#endif

    if (engine == nullptr) {
        if (vehicle != nullptr) delete vehicle;
        if (transmission != nullptr) delete transmission;
        return;
    }

    if (vehicle == nullptr) {
        loadDefaultVehicle(&vehicle);
    }

    if (transmission == nullptr) {
        loadDefaultTransmission(&transmission);
    }

    loadEngine(engine, vehicle, transmission);
    refreshUserInterface();
}

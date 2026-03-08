add_library(engine-sim STATIC
    ${ENGINE_SIM_CORE_SOURCES}
    ${ENGINE_SIM_CORE_HEADERS}
)

target_include_directories(engine-sim
    PUBLIC dependencies/submodules
)

target_link_libraries(engine-sim
    simple-2d-constraint-solver
    csv-io
    delta-basic
)

if (PIRANHA_ENABLED)
    add_library(engine-sim-script-interpreter STATIC
        ${ENGINE_SIM_SCRIPT_SOURCES}
        ${ENGINE_SIM_SCRIPT_HEADERS}
    )

    target_include_directories(engine-sim-script-interpreter
        PUBLIC dependencies/submodules
    )

    target_link_libraries(engine-sim-script-interpreter
        csv-io
        piranha
    )
endif()

add_executable(engine-sim-app WIN32
    ${ENGINE_SIM_APP_SOURCES}
    ${ENGINE_SIM_APP_HEADERS}
)

target_include_directories(engine-sim-app
    PUBLIC dependencies/submodules
)

target_link_libraries(engine-sim-app
    engine-sim
)

if (DTV)
    target_link_libraries(engine-sim-app
        direct-to-video
    )
endif()

if (PIRANHA_ENABLED)
    target_link_libraries(engine-sim-app
        engine-sim-script-interpreter
    )
endif()

if (DISCORD_ENABLED)
    target_sources(engine-sim-app PUBLIC
        src/discord.cpp
        include/discord.h
    )

    target_include_directories(engine-sim-app PUBLIC
        dependencies/discord/include
    )

    target_link_libraries(engine-sim-app
        discord-rpc
    )
endif()

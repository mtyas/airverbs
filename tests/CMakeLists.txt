add_executable (AirverbsTests
    TestMain.cpp
    ../source/dsp/EngineSlot.cpp
    ../source/dsp/Constellation.cpp)
target_link_libraries (AirverbsTests PRIVATE AirverbsEngines)
target_compile_features (AirverbsTests PRIVATE cxx_std_20)
if (NOT MSVC)
    target_compile_options (AirverbsTests PRIVATE -O2)
endif()

# Processor-level checks (presets, state round trip, MIDI learn, undo) against the real plugin code.
add_executable (AirverbsPluginTests PluginTests.cpp)
target_include_directories (AirverbsPluginTests PRIVATE $<TARGET_PROPERTY:Airverbs,INCLUDE_DIRECTORIES>)
target_compile_definitions (AirverbsPluginTests PRIVATE $<TARGET_PROPERTY:Airverbs,COMPILE_DEFINITIONS>)
target_compile_features (AirverbsPluginTests PRIVATE cxx_std_20)
target_link_libraries (AirverbsPluginTests PRIVATE Airverbs)

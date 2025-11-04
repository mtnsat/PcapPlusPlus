add_executable(PfRingBridge main.cpp)

target_link_libraries(PfRingBridge PUBLIC PcapPlusPlus::Pcap++)

set_target_properties(PfRingBridge PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PCAPPP_BINARY_EXAMPLES_DIR}")

if(PCAPPP_INSTALL)
  install(
    TARGETS PfRingBridge
    EXPORT PcapPlusPlusTargets
    RUNTIME DESTINATION ${PCAPPP_INSTALL_BINDIR})
endif()

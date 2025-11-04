add_executable(PcapLiveBridge main.cpp)

target_link_libraries(PcapLiveBridge PUBLIC PcapPlusPlus::Pcap++)

set_target_properties(PcapLiveBridge PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PCAPPP_BINARY_EXAMPLES_DIR}")

if(PCAPPP_INSTALL)
  install(
    TARGETS PcapLiveBridge
    EXPORT PcapPlusPlusTargets
    RUNTIME DESTINATION ${PCAPPP_INSTALL_BINDIR})
endif()

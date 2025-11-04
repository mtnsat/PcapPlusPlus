add_executable(XdpBridge main.cpp)

target_link_libraries(XdpBridge PUBLIC PcapPlusPlus::Pcap++)

set_target_properties(XdpBridge PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PCAPPP_BINARY_EXAMPLES_DIR}")

if(PCAPPP_INSTALL)
  install(
    TARGETS XdpBridge
    EXPORT PcapPlusPlusTargets
    RUNTIME DESTINATION ${PCAPPP_INSTALL_BINDIR})
endif()

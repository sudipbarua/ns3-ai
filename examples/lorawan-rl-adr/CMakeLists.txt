# Python module for lorawan rl adr
pybind11_add_module(ns3ai_lorawan_rl_adr_py ai_lorawan_rl_adr_py.cc)
set_target_properties(ns3ai_lorawan_rl_adr_py PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ns3ai_lorawan_rl_adr_py PRIVATE     
        ${libai}
        ${libcore}
        ${libpoint-to-point}
        ${libnetwork}
        ${libapplications}
        ${libmobility}
        ${libinternet}
        ${libwifi}
        ${libflow-monitor})

# Build Python interface along with C++ lib
add_dependencies(ns3ai_lorawan_rl_adr ns3ai_lorawan_rl_adr_py)

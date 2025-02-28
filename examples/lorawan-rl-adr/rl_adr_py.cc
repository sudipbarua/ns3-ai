#include "lorawan_rl_adr.h"
#include <ns3/ai-module.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

PYBIND11_MAKE_OPAQUE(std::array<ns3::AiAdrStatesStruct, 64>);  // Opaque type binding refers to the fact that the type is not fully defined in the binding code
// The array size 64 is likely chosen based on the maximum number of possible rate adaptation states that the Thompson Sampling-based Wi-Fi manager needs to track.
// We will try to treat the AiAdrStatesStruct as an array for python. But gwList, last received packet and packetList are not directly convertible to python. So we will need to extract relevant info from them or use them as default (not an array)

PYBIND11_MODULE(ns3ai_lorawan_adr_rl_py, m)
{
    py::class<ns3::AiAdrStatesStruct>(m, "AiAdrStatesStruct")
        .def(py::init<>())
        .def_readwrite("managerId", &ns3::AiAdrStatesStruct::managerId)
        .def_readwrite("type", &ns3::AiAdrStatesStruct::type)
        .def_readwrite("spreadingFactor", &ns3::AiAdrStatesStruct::spreadingFactor)
        .def_readwrite("txPower", &ns3::AiAdrStatesStruct::txPower)
        .def_readwrite("snr", &ns3::AiAdrStatesStruct::snr)
        .def_readwrite("rssi", &ns3::AiAdrStatesStruct::rssi)
        .def_readwrite("gwList", &ns3::AiAdrStatesStruct::gwList)
        .def_readwrite("packetList", &ns3::AiAdrStatesStruct::packetList)
        .def_readwrite("lastReceivedPacket", &ns3::AiAdrStatesStruct::lastReceivedPacket)
        .def_readwrite("batteryLevel", &ns3::AiAdrStatesStruct::batteryLevel)
        .def("__copy__", [](const ns3::AiAdrStatesStruct& self) {       // Creating copy of the object in python. [] means that the function is overloaded
            return ns3::AiAdrStatesStruct(self);
        });
    
    py::class_<std::array<ns3::AiAdrStatesStruct, 64>>(m, "AiAdrStatesStructArray")
        .def(py::init<>())
        .def("size", &std::array<ns3::AiAdrStatesStruct, 64>::size)
        .def("__len__",
             [](const std::array<ns3::AiAdrStatesStruct, 64>& arr) { return arr.size(); })
        .def("__getitem__",
             [](const std::array<ns3::AiAdrStatesStruct, 64>& arr, uint32_t i) {
                 if (i >= arr.size())
                 {
                     std::cerr << "Invalid index " << i << " for std::array, whose size is "
                               << arr.size() << std::endl;
                     exit(1);
                 }
                 return arr.at(i);
             });

    py::class<ns3::AiAdrActionStruct>(m, "AiAdrActionStruct")
        .def(py::init<>())
        .def_readwrite("managerId", &ns3::AiAdrActionStruct::managerId)
        .def_readwrite("spreadingFactor", &ns3::AiAdrActionStruct::spreadingFactor)
        .def_readwrite("txPower", &ns3::AiAdrActionStruct::txPower)
        .def("__copy__", [](const ns3::AiAdrActionStruct& self) {       // Creating copy of the object in python. [] means that the function is overloaded
            return ns3::AiAdrActionStruct(self);
        });


    // Handling message exchange between Python and C++
    py::class_<ns3::Ns3AiMsgInterfaceImpl<ns3::AiAdrStatesStruct, ns3::AiAdrActionStruct>>(m, "Ns3AiMsgInterfaceImpl")
        .def(py::init<bool,
                      bool,
                      bool,
                      uint32_t,
                      const char*,
                      const char*,
                      const char*,
                      const char*>())
        .def("PyRecvBegin",
             &ns3::Ns3AiMsgInterfaceImpl<ns3::AiAdrStatesStruct,
                                         ns3::AiAdrActionStruct>::PyRecvBegin)
        .def("PyRecvEnd",
             &ns3::Ns3AiMsgInterfaceImpl<ns3::AiAdrStatesStruct,
                                         ns3::AiAdrActionStruct>::PyRecvEnd)
        .def("PySendBegin",
             &ns3::Ns3AiMsgInterfaceImpl<ns3::AiAdrStatesStruct,
                                         ns3::AiAdrActionStruct>::PySendBegin)
        .def("PySendEnd",
             &ns3::Ns3AiMsgInterfaceImpl<ns3::AiAdrStatesStruct,
                                         ns3::AiAdrActionStruct>::PySendEnd)
        .def("PyGetFinished",
             &ns3::Ns3AiMsgInterfaceImpl<ns3::AiAdrStatesStruct,
                                         ns3::AiAdrActionStruct>::PyGetFinished)
        .def("GetCpp2PyStruct",
             &ns3::Ns3AiMsgInterfaceImpl<ns3::AiAdrStatesStruct,
                                         ns3::AiAdrActionStruct>::GetCpp2PyStruct,
             py::return_value_policy::reference)
        .def("GetPy2CppStruct",
             &ns3::Ns3AiMsgInterfaceImpl<ns3::AiAdrStatesStruct,
                                         ns3::AiAdrActionStruct>::GetPy2CppStruct,
             py::return_value_policy::reference);
}

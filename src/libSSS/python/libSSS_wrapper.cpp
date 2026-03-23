
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "../osn/OSNReceiver.h"
#include "../osn/OSNSender.h"

namespace py = pybind11;

PYBIND11_MODULE(SSS, m)
{
    m.doc() = "Python bindings for OSNReceiver and OSNSender classes";

    // Wrap the OSNReceiver class
    py::class_<OSNReceiver>(m, "OSNReceiver")
        .def(py::init<size_t>(), py::arg("ios_threads") = 4)
        .def("run", &OSNReceiver::run_osn,
             py::arg("size"), py::arg("p"),
             py::arg("ot_type") = 1,
             py::arg("Sip") = "127.0.0.1:12222",
             py::arg("sessionHint") = "",
             py::arg("num_threads") = 1)
        .def("getTotalDataSent", &OSNReceiver::getTotalDataSent)
        .def("getTotalDataRecv", &OSNReceiver::getTotalDataRecv)
        .def("setTimer", &OSNReceiver::setTimer);

    // Wrap the OSNSender class
    py::class_<OSNSender>(m, "OSNSender")
        .def(py::init<size_t, std::vector<int>&, std::vector<uint64_t>&, size_t>(),
             py::arg("size"), py::arg("dest"), py::arg("p"), py::arg("ios_threads") = 4)
        .def("run", &OSNSender::run_osn,
             py::arg("size"),
             py::arg("p"),
             py::arg("ot_type") = 1,
             py::arg("Sip") = "127.0.0.1:12222",
             py::arg("sessionHint") = "",
             py::arg("num_threads") = 1)
        .def("getTotalDataSent", &OSNSender::getTotalDataSent)
        .def("getTotalDataRecv", &OSNSender::getTotalDataRecv)
        .def("setTimer", &OSNSender::setTimer);
}

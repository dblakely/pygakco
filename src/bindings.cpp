#include <pybind11/pybind11.h>
#include "svm.hpp"

namespace py = pybind11;

PYBIND11_MODULE(igakco, m) {
    py::class_<SVM>(m, "SVM")
        .def(py::init<int, int, double, double, double, std::string>(), 
            py::arg("g"), py::arg("m"), py::arg("C")=1.0, 
            py::arg("nu")=0.5, py::arg("eps")=0.001, py::arg("kernel")="linear")
        .def("toString", &SVM::toString)
        .def("fit", &SVM::fit, py::arg("train_file"), py::arg("test_file"), 
            py::arg("dict")="", py::arg("quiet")=false, 
            py::arg("kernel_file")="")
        .def("predict", &SVM::predict, py::arg("predictions_file"));

#ifdef VERSION_INFO
    m.attr("__version__") = VERSION_INFO;
#else
    m.attr("__version__") = "dev";
#endif
}

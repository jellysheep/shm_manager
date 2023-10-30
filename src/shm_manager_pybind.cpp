#include <pybind11/pybind11.h>
#include "shm_manager.hpp"

namespace py = pybind11;

PYBIND11_MODULE(shm_manager, m)
{
  py::class_<shm_manager::ShmManager>(m, "ShmManager").def(py::init()).def("run", [](shm_manager::ShmManager& manager) {
    manager.run();
  });
  py::class_<shm_manager::ShmClient>(m, "ShmClient")
      .def_static(
          "create",
          [](const std::string& name, size_t size) { return shm_manager::ShmClient::create(name, size); },
          py::arg("name"),
          py::arg("size"))
      .def_static(
          "get", [](const std::string& name) { return shm_manager::ShmClient::get(name); }, py::arg("name"))
      .def_static(
          "remove", [](const std::string& name) { return shm_manager::ShmClient::remove(name); }, py::arg("name"))
      .def_static("send_quit", shm_manager::ShmClient::send_quit)
      .def_property_readonly("fd", [](const shm_manager::ShmClient& client) { return client.get_fd(); })
      .def_property_readonly("addr", [](const shm_manager::ShmClient& client) { return uintptr_t(client.get_addr()); })
      .def(
          "map_fd",
          [](shm_manager::ShmClient& client, uintptr_t target_addr) { client.map_fd((void*)target_addr); },
          py::arg("target_addr") = 0);
}

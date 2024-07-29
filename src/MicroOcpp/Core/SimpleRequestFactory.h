// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_SIMPLEREQUESTFACTORY_H
#define MO_SIMPLEREQUESTFACTORY_H

#include <memory>

#include <MicroOcpp/Core/Request.h>

namespace MicroOcpp {

class Operation;

std::unique_ptr<Request> makeRequest(std::unique_ptr<Operation> op);
std::unique_ptr<Request> makeRequest(Operation *op); //takes ownership of op

} //end namespace MicroOcpp
#endif

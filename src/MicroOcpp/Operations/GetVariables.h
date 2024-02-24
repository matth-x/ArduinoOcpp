// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_GETVARIABLES_H
#define MO_GETVARIABLES_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <string>
#include <vector>

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/Variables/Variable.h>

namespace MicroOcpp {

class VariableService;

namespace Ocpp201 {

// GetVariableDataType (2.25) and
// GetVariableResultType (2.26)
struct GetVariableData {
    // GetVariableDataType
    Variable::AttributeType attributeType = Variable::AttributeType::Actual;
    std::string componentName;
    int componentEvseId = -1;
    int componentEvseConnectorId = -1;
    std::string variableName;

    // GetVariableResultType
    GetVariableStatus attributeStatus;
    Variable *variable = nullptr;
};

class GetVariables : public Operation {
private:
    VariableService& variableService;
    std::vector<GetVariableData> queries;

    const char *errorCode = nullptr;
public:
    GetVariables(VariableService& variableService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

    const char *getErrorCode() override {return errorCode;}

};

} //namespace Ocpp201
} //namespace MicroOcpp

#endif //MO_ENABLE_V201

#endif

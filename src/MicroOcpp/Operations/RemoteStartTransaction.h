// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_REMOTESTARTTRANSACTION_H
#define MO_REMOTESTARTTRANSACTION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Operations/CiStrings.h>

namespace MicroOcpp {

class Model;
class ChargingProfile;

namespace Ocpp16 {

class RemoteStartTransaction : public Operation, public MemoryManaged {
private:
    Model& model;

    bool accepted = false;
    
    const char *errorCode {nullptr};
    const char *errorDescription = "";
public:
    RemoteStartTransaction(Model& model);

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
    const char *getErrorDescription() override {return errorDescription;}
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif

// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_SENDLOCALLIST_H
#define MO_SENDLOCALLIST_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_LOCAL_AUTH

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

class AuthorizationService;

namespace Ocpp16 {

class SendLocalList : public Operation {
private:
    AuthorizationService& authService;
    const char *errorCode = nullptr;
    bool updateFailure = true;
    bool versionMismatch = false;
public:
    SendLocalList(AuthorizationService& authService);

    ~SendLocalList();

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace MicroOcpp

#endif //MO_ENABLE_LOCAL_AUTH
#endif

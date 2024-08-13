// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CLEARCHARGINGPROFILE_H
#define MO_CLEARCHARGINGPROFILE_H

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

class SmartChargingService;

namespace Ocpp16 {

class ClearChargingProfile : public Operation, public MemoryManaged {
private:
    SmartChargingService& scService;
    bool matchingProfilesFound = false;
public:
    ClearChargingProfile(SmartChargingService& scService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;
    
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif

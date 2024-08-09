// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/GetDiagnostics.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::GetDiagnostics;
using MicroOcpp::MemJsonDoc;

GetDiagnostics::GetDiagnostics(DiagnosticsService& diagService) : AllocOverrider("v16.Operation.", getOperationType()), diagService(diagService), fileName(makeMemString(nullptr, getMemoryTag())) {

}

void GetDiagnostics::processReq(JsonObject payload) {

    const char *location = payload["location"] | "";
    //check location URL. Maybe introduce Same-Origin-Policy?
    if (!*location) {
        errorCode = "FormationViolation";
        return;
    }
    
    int retries = payload["retries"] | 1;
    int retryInterval = payload["retryInterval"] | 180;
    if (retries < 0 || retryInterval < 0) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    Timestamp startTime;
    if (payload.containsKey("startTime")) {
        if (!startTime.setTime(payload["startTime"] | "Invalid")) {
            errorCode = "PropertyConstraintViolation";
            MO_DBG_WARN("bad time format");
            return;
        }
    }

    Timestamp stopTime;
    if (payload.containsKey("stopTime")) {
        if (!stopTime.setTime(payload["stopTime"] | "Invalid")) {
            errorCode = "PropertyConstraintViolation";
            MO_DBG_WARN("bad time format");
            return;
        }
    }

    fileName = diagService.requestDiagnosticsUpload(location, (unsigned int) retries, (unsigned int) retryInterval, startTime, stopTime);
}

std::unique_ptr<MemJsonDoc> GetDiagnostics::createConf(){
    if (fileName.empty()) {
        return createEmptyDocument();
    } else {
        auto doc = makeMemJsonDoc(JSON_OBJECT_SIZE(1), getMemoryTag());
        JsonObject payload = doc->to<JsonObject>();
        payload["fileName"] = fileName.c_str();
        return doc;
    }
}

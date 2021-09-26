// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef OCPPENGINE_H
#define OCPPENGINE_H

#include <ArduinoJson.h>

#include <ArduinoOcpp/Core/OcppOperation.h>
#include <ArduinoOcpp/Core/OcppSocket.h>
#include <ArduinoOcpp/Core/OcppTime.h>
#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingService.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Tasks/Metering/MeteringService.h>
#include <ArduinoOcpp/Tasks/FirmwareManagement/FirmwareService.h>

namespace ArduinoOcpp {

void ocppEngine_initialize(OcppSocket *ocppSocket);

#if 0

bool processWebSocketEvent(const char* payload, size_t length);

//WebSocket fragments are not supported. This function sends a meaningful error response
boolean processWebSocketUnsupportedEvent(const char* payload, size_t length);

void handleConfMessage(JsonDocument *json);

void handleReqMessage(JsonDocument *json);

void handleReqMessage(JsonDocument *json, OcppOperation *op);

void handleErrMessage(JsonDocument *json);

#endif

void initiateOcppOperation(OcppOperation *o);

void ocppEngine_startHeartbeat(int interval);

void ocppEngine_loop();

void setSmartChargingService(SmartChargingService *scs);

SmartChargingService* getSmartChargingService();

void setChargePointStatusService(ChargePointStatusService *cpss);

ChargePointStatusService *getChargePointStatusService();

ConnectorStatus *getConnectorStatus(int connectorId);

void setMeteringSerivce(MeteringService *meteringService);

MeteringService* getMeteringService();

void setFirmwareService(FirmwareService *firmwareService);

FirmwareService *getFirmwareService();

void ocppEngine_setOcppTime(OcppTime *ocppTime);

OcppTime *getOcppTime();

} //end namespace ArduinoOcpp

#endif

// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_MODEL_H
#define MO_MODEL_H

#include <memory>

#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>
#include <MicroOcpp/Model/ConnectorBase/Connector.h>

namespace MicroOcpp {

class TransactionStore;
class SmartChargingService;
class ConnectorsCommon;
class MeteringService;
class FirmwareService;
class DiagnosticsService;
class HeartbeatService;
class BootService;
class ResetService;

#if MO_ENABLE_LOCAL_AUTH
class AuthorizationService;
#endif //MO_ENABLE_LOCAL_AUTH

#if MO_ENABLE_RESERVATION
class ReservationService;
#endif //MO_ENABLE_RESERVATION

#if MO_ENABLE_CERT_MGMT
class CertificateService;
#endif //MO_ENABLE_CERT_MGMT

#if MO_ENABLE_V201
class AvailabilityService;
class VariableService;
class TransactionService;
class RemoteControlService;

namespace Ocpp201 {
class ResetService;
class MeteringService;
}
#endif //MO_ENABLE_V201

class Model : public MemoryManaged {
private:
    Vector<std::unique_ptr<Connector>> connectors;
    std::unique_ptr<TransactionStore> transactionStore;
    std::unique_ptr<SmartChargingService> smartChargingService;
    std::unique_ptr<ConnectorsCommon> chargeControlCommon;
    std::unique_ptr<MeteringService> meteringService;
    std::unique_ptr<FirmwareService> firmwareService;
    std::unique_ptr<DiagnosticsService> diagnosticsService;
    std::unique_ptr<HeartbeatService> heartbeatService;
    std::unique_ptr<BootService> bootService;
    std::unique_ptr<ResetService> resetService;

#if MO_ENABLE_LOCAL_AUTH
    std::unique_ptr<AuthorizationService> authorizationService;
#endif //MO_ENABLE_LOCAL_AUTH

#if MO_ENABLE_RESERVATION
    std::unique_ptr<ReservationService> reservationService;
#endif //MO_ENABLE_RESERVATION

#if MO_ENABLE_CERT_MGMT
    std::unique_ptr<CertificateService> certService;
#endif //MO_ENABLE_CERT_MGMT

#if MO_ENABLE_V201
    std::unique_ptr<AvailabilityService> availabilityService;
    std::unique_ptr<VariableService> variableService;
    std::unique_ptr<TransactionService> transactionService;
    std::unique_ptr<Ocpp201::ResetService> resetServiceV201;
    std::unique_ptr<Ocpp201::MeteringService> meteringServiceV201;
    std::unique_ptr<RemoteControlService> remoteControlService;
#endif

    Clock clock;

    ProtocolVersion version;

    bool capabilitiesUpdated = true;
    void updateSupportedStandardProfiles();

    bool runTasks = false;

    const uint16_t bootNr = 0; //each boot of this lib has a unique number

public:
    Model(ProtocolVersion version = ProtocolVersion(1,6), uint16_t bootNr = 0);
    Model(const Model& rhs) = delete;
    ~Model();

    void loop();

    void activateTasks() {runTasks = true;}

    void setTransactionStore(std::unique_ptr<TransactionStore> transactionStore);
    TransactionStore *getTransactionStore();

    void setSmartChargingService(std::unique_ptr<SmartChargingService> scs);
    SmartChargingService* getSmartChargingService() const;

    void setConnectorsCommon(std::unique_ptr<ConnectorsCommon> ccs);
    ConnectorsCommon *getConnectorsCommon();

    void setConnectors(Vector<std::unique_ptr<Connector>>&& connectors);
    unsigned int getNumConnectors() const;
    Connector *getConnector(unsigned int connectorId);

    void setMeteringSerivce(std::unique_ptr<MeteringService> meteringService);
    MeteringService* getMeteringService() const;

    void setFirmwareService(std::unique_ptr<FirmwareService> firmwareService);
    FirmwareService *getFirmwareService() const;

    void setDiagnosticsService(std::unique_ptr<DiagnosticsService> diagnosticsService);
    DiagnosticsService *getDiagnosticsService() const;

    void setHeartbeatService(std::unique_ptr<HeartbeatService> heartbeatService);

#if MO_ENABLE_LOCAL_AUTH
    void setAuthorizationService(std::unique_ptr<AuthorizationService> authorizationService);
    AuthorizationService *getAuthorizationService();
#endif //MO_ENABLE_LOCAL_AUTH

#if MO_ENABLE_RESERVATION
    void setReservationService(std::unique_ptr<ReservationService> reservationService);
    ReservationService *getReservationService();
#endif //MO_ENABLE_RESERVATION

    void setBootService(std::unique_ptr<BootService> bs);
    BootService *getBootService() const;

    void setResetService(std::unique_ptr<ResetService> rs);
    ResetService *getResetService() const;

#if MO_ENABLE_CERT_MGMT
    void setCertificateService(std::unique_ptr<CertificateService> cs);
    CertificateService *getCertificateService() const;
#endif //MO_ENABLE_CERT_MGMT

#if MO_ENABLE_V201
    void setAvailabilityService(std::unique_ptr<AvailabilityService> as);
    AvailabilityService *getAvailabilityService() const;

    void setVariableService(std::unique_ptr<VariableService> vs);
    VariableService *getVariableService() const;

    void setTransactionService(std::unique_ptr<TransactionService> ts);
    TransactionService *getTransactionService() const;

    void setResetServiceV201(std::unique_ptr<Ocpp201::ResetService> rs);
    Ocpp201::ResetService *getResetServiceV201() const;

    void setMeteringServiceV201(std::unique_ptr<Ocpp201::MeteringService> ms);
    Ocpp201::MeteringService *getMeteringServiceV201() const;

    void setRemoteControlService(std::unique_ptr<RemoteControlService> rs);
    RemoteControlService *getRemoteControlService() const;
#endif

    Clock &getClock();

    const ProtocolVersion& getVersion() const;

    uint16_t getBootNr();
};

} //end namespace MicroOcpp

#endif

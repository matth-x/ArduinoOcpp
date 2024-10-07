// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Availability/AvailabilityService.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Model/Reservation/ReservationService.h>
#include <MicroOcpp/Model/Transactions/TransactionService.h>
#include <MicroOcpp/Operations/ChangeAvailability.h>
#include <MicroOcpp/Operations/StatusNotification.h>

using namespace MicroOcpp;

AvailabilityServiceEvse::AvailabilityServiceEvse(Context& context, AvailabilityService& availabilityService, unsigned int evseId) : MemoryManaged("v201.Availability.AvailabilityServiceEvse"), context(context), availabilityService(availabilityService), evseId(evseId) {

}

void AvailabilityServiceEvse::loop() {

    if (evseId >= 1) {
        auto status = getStatus();

        if (status != reportedStatus &&
                context.getModel().getClock().now() >= MIN_TIME) {

            auto statusNotification = makeRequest(new Ocpp201::StatusNotification(evseId, status, context.getModel().getClock().now()));
            statusNotification->setTimeout(0);
            context.initiateRequest(std::move(statusNotification));
            reportedStatus = status;
            return;
        }
    }
}

void AvailabilityServiceEvse::setConnectorPluggedInput(std::function<bool()> connectorPluggedInput) {
    this->connectorPluggedInput = connectorPluggedInput;
}

void AvailabilityServiceEvse::setOccupiedInput(std::function<bool()> occupiedInput) {
    this->occupiedInput = occupiedInput;
}

ChargePointStatus AvailabilityServiceEvse::getStatus() {
    ChargePointStatus res = ChargePointStatus_UNDEFINED;

    if (isFaulted()) {
        res = ChargePointStatus_Faulted;
    } else if (!isAvailable()) {
        res = ChargePointStatus_Unavailable;
    }
    #if MO_ENABLE_RESERVATION
    else if (context.getModel().getReservationService() && context.getModel().getReservationService()->getReservation(evseId)) {
        res = ChargePointStatus_Reserved;
    }
    #endif 
    else if ((!connectorPluggedInput || !connectorPluggedInput()) &&   //no vehicle plugged
               (!occupiedInput || !occupiedInput())) {                       //occupied override clear
        res = ChargePointStatus_Available;
    } else {
        res = ChargePointStatus_Occupied;
    }

    return res;
}

void AvailabilityServiceEvse::setUnavailable(void *requesterId) {
    for (size_t i = 0; i < MO_INOPERATIVE_REQUESTERS_MAX; i++) {
        if (!unavailableRequesters[i]) {
            unavailableRequesters[i] = requesterId;
            return;
        }
    }
    MO_DBG_ERR("exceeded max. unavailable requesters");
}

void AvailabilityServiceEvse::setAvailable(void *requesterId) {
    for (size_t i = 0; i < MO_INOPERATIVE_REQUESTERS_MAX; i++) {
        if (unavailableRequesters[i] == requesterId) {
            unavailableRequesters[i] = nullptr;
            return;
        }
    }
    MO_DBG_ERR("could not find unavailable requester");
}

ChangeAvailabilityStatus AvailabilityServiceEvse::changeAvailability(bool operative) {
    if (operative) {
        setAvailable(this);
    } else {
        setUnavailable(this);
    }

    if (!operative) {
        if (isAvailable()) {
            return ChangeAvailabilityStatus::Scheduled;
        }

        if (evseId == 0) {
            for (unsigned int id = 1; id < MO_NUM_EVSEID; id++) {
                if (availabilityService.getEvse(id) && availabilityService.getEvse(id)->isAvailable()) {
                    return ChangeAvailabilityStatus::Scheduled;
                }
            }
        }
    }

    return ChangeAvailabilityStatus::Accepted;
}

void AvailabilityServiceEvse::setFaulted(void *requesterId) {
    for (size_t i = 0; i < MO_FAULTED_REQUESTERS_MAX; i++) {
        if (!faultedRequesters[i]) {
            faultedRequesters[i] = requesterId;
            return;
        }
    }
    MO_DBG_ERR("exceeded max. faulted requesters");
}

void AvailabilityServiceEvse::resetFaulted(void *requesterId) {
    for (size_t i = 0; i < MO_FAULTED_REQUESTERS_MAX; i++) {
        if (faultedRequesters[i] == requesterId) {
            faultedRequesters[i] = nullptr;
            return;
        }
    }
    MO_DBG_ERR("could not find faulted requester");
}

bool AvailabilityServiceEvse::isAvailable() {

    auto txService = context.getModel().getTransactionService();
    auto txEvse = txService ? txService->getEvse(evseId) : nullptr;
    if (txEvse) {
        if (txEvse->getTransaction() &&
                txEvse->getTransaction()->started &&
                !txEvse->getTransaction()->stopped) {
            return true;
        }
    }

    if (evseId > 0) {
        if (availabilityService.getEvse(0) && !availabilityService.getEvse(0)->isAvailable()) {
            return false;
        }
    }

    for (size_t i = 0; i < MO_INOPERATIVE_REQUESTERS_MAX; i++) {
        if (unavailableRequesters[i]) {
            return false;
        }
    }
    return true;
}

bool AvailabilityServiceEvse::isFaulted() {
    for (size_t i = 0; i < MO_FAULTED_REQUESTERS_MAX; i++) {
        if (faultedRequesters[i]) {
            return true;
        }
    }
    return false;
}

AvailabilityService::AvailabilityService(Context& context, size_t numEvses) : MemoryManaged("v201.Availability.AvailabilityService"), context(context) {

    for (size_t i = 0; i < numEvses && i < MO_NUM_EVSEID; i++) {
        evses[i] = new AvailabilityServiceEvse(context, *this, (unsigned int)i);
    }

    context.getOperationRegistry().registerOperation("StatusNotification", [&context] () {
        return new Ocpp16::StatusNotification(-1, ChargePointStatus_UNDEFINED, Timestamp());});
    context.getOperationRegistry().registerOperation("ChangeAvailability", [this] () {
        return new Ocpp201::ChangeAvailability(*this);});
}

AvailabilityService::~AvailabilityService() {
    for (size_t i = 0; i < MO_NUM_EVSEID && evses[i]; i++) {
        delete evses[i];
    }
}

void AvailabilityService::loop() {
    for (size_t i = 0; i < MO_NUM_EVSEID && evses[i]; i++) {
        evses[i]->loop();
    }
}

AvailabilityServiceEvse *AvailabilityService::getEvse(unsigned int evseId) {
    if (evseId >= MO_NUM_EVSEID) {
        MO_DBG_ERR("invalid arg");
        return nullptr;
    }
    return evses[evseId];
}

#endif // MO_ENABLE_V201

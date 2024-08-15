// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <limits>

#include <MicroOcpp/Model/Boot/BootService.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Operations/BootNotification.h>
#include <MicroOcpp/Platform.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

unsigned int PreBootQueue::getFrontRequestOpNr() {
    if (!activatedPostBootCommunication) {
        return 0;
    }

    return VolatileRequestQueue::getFrontRequestOpNr();
}

void PreBootQueue::activatePostBootCommunication() {
    activatedPostBootCommunication = true;
}

RegistrationStatus MicroOcpp::deserializeRegistrationStatus(const char *serialized) {
    if (!strcmp(serialized, "Accepted")) {
        return RegistrationStatus::Accepted;
    } else if (!strcmp(serialized, "Pending")) {
        return RegistrationStatus::Pending;
    } else if (!strcmp(serialized, "Rejected")) {
        return RegistrationStatus::Rejected;
    } else {
        MO_DBG_ERR("deserialization error");
        return RegistrationStatus::UNDEFINED;
    }
}

BootService::BootService(Context& context, std::shared_ptr<FilesystemAdapter> filesystem) : MemoryManaged("v16.Boot.BootService"), context(context), filesystem(filesystem), cpCredentials{makeString(getMemoryTag())} {
    
    context.getRequestQueue().setPreBootSendQueue(&preBootQueue); //register PreBootQueue in RequestQueue module
    
    //if transactions can start before the BootNotification succeeds
    preBootTransactionsBool = declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "PreBootTransactions", false);
    
    if (!preBootTransactionsBool) {
        MO_DBG_ERR("initialization error");
    }

    //Register message handler for TriggerMessage operation
    context.getOperationRegistry().registerOperation("BootNotification", [this] () {
        return new Ocpp16::BootNotification(this->context.getModel(), getChargePointCredentials());});
}

void BootService::loop() {

    if (!executedFirstTime) {
        executedFirstTime = true;
        firstExecutionTimestamp = mocpp_tick_ms();
    }

    if (!executedLongTime && mocpp_tick_ms() - firstExecutionTimestamp >= MO_BOOTSTATS_LONGTIME_MS) {
        executedLongTime = true;
        MO_DBG_DEBUG("boot success timer reached");

        configuration_clean_unused();

        BootStats bootstats;
        loadBootStats(filesystem, bootstats);
        bootstats.lastBootSuccess = bootstats.bootNr;
        storeBootStats(filesystem, bootstats);
    }

    preBootQueue.loop();

    if (!activatedPostBootCommunication && status == RegistrationStatus::Accepted) {
        preBootQueue.activatePostBootCommunication();
        activatedPostBootCommunication = true;
    }

    if (!activatedModel && (status == RegistrationStatus::Accepted || preBootTransactionsBool->getBool())) {
        context.getModel().activateTasks();
        activatedModel = true;
    }

    if (status == RegistrationStatus::Accepted) {
        return;
    }
    
    if (mocpp_tick_ms() - lastBootNotification < (interval_s * 1000UL)) {
        return;
    }

    /*
     * Create BootNotification. The BootNotifaction object will fetch its paremeters from
     * this class and notify this class about the response
     */
    auto bootNotification = makeRequest(new Ocpp16::BootNotification(context.getModel(), getChargePointCredentials()));
    bootNotification->setTimeout(interval_s * 1000UL);
    context.getRequestQueue().sendRequestPreBoot(std::move(bootNotification));

    lastBootNotification = mocpp_tick_ms();
}

void BootService::setChargePointCredentials(JsonObject credentials) {
    auto written = serializeJson(credentials, cpCredentials);
    if (written < 2) {
        MO_DBG_ERR("serialization error");
        cpCredentials = "{}";
    }
}

void BootService::setChargePointCredentials(const char *credentials) {
    cpCredentials = credentials;
    if (cpCredentials.size() < 2) {
        cpCredentials = "{}";
    }
}

std::unique_ptr<JsonDoc> BootService::getChargePointCredentials() {
    if (cpCredentials.size() <= 2) {
        return createEmptyDocument();
    }

    std::unique_ptr<JsonDoc> doc;
    size_t capacity = JSON_OBJECT_SIZE(9) + cpCredentials.size();
    DeserializationError err = DeserializationError::NoMemory;
    while (err == DeserializationError::NoMemory && capacity <= MO_MAX_JSON_CAPACITY) {
        doc = makeJsonDoc(getMemoryTag(), capacity);
        err = deserializeJson(*doc, cpCredentials);

        capacity *= 2;
    }

    if (!err) {
        return doc;
    } else {
        MO_DBG_ERR("could not parse stored credentials: %s", err.c_str());
        return nullptr;
    }
}

void BootService::notifyRegistrationStatus(RegistrationStatus status) {
    this->status = status;
    lastBootNotification = mocpp_tick_ms();
}

void BootService::setRetryInterval(unsigned long interval_s) {
    if (interval_s == 0) {
        this->interval_s = MO_BOOT_INTERVAL_DEFAULT;
    } else {
        this->interval_s = interval_s;
    }
    lastBootNotification = mocpp_tick_ms();
}

bool BootService::loadBootStats(std::shared_ptr<FilesystemAdapter> filesystem, BootStats& bstats) {
    if (!filesystem) {
        return false;
    }

    size_t msize = 0;
    if (filesystem->stat(MO_FILENAME_PREFIX "bootstats.jsn", &msize) == 0) {
        
        bool success = true;

        auto json = FilesystemUtils::loadJson(filesystem, MO_FILENAME_PREFIX "bootstats.jsn", "v16.Boot.BootService");
        if (json) {
            int bootNrIn = (*json)["bootNr"] | -1;
            if (bootNrIn >= 0 && bootNrIn <= std::numeric_limits<uint16_t>::max()) {
                bstats.bootNr = (uint16_t) bootNrIn;
            } else {
                success = false;
            }

            int lastSuccessIn = (*json)["lastSuccess"] | -1;
            if (lastSuccessIn >= 0 && lastSuccessIn <= std::numeric_limits<uint16_t>::max()) {
                bstats.lastBootSuccess = (uint16_t) lastSuccessIn;
            } else {
                success = false;
            }

            const char *microOcppVersionIn = (*json)["MicroOcppVersion"] | (const char*)nullptr;
            if (microOcppVersionIn) {
                auto ret = snprintf(bstats.microOcppVersion, sizeof(bstats.microOcppVersion), "%s", microOcppVersionIn);
                if (ret < 0 || (size_t)ret >= sizeof(bstats.microOcppVersion)) {
                    success = false;
                }
            } //else: version specifier can be missing after upgrade from pre 1.2.0 version
        } else {
            success = false;
        }

        if (!success) {
            MO_DBG_ERR("bootstats corrupted");
            filesystem->remove(MO_FILENAME_PREFIX "bootstats.jsn");
            bstats = BootStats();
        }

        return success;
    } else {
        return false;
    }
}

bool BootService::storeBootStats(std::shared_ptr<FilesystemAdapter> filesystem, BootStats& bstats) {
    if (!filesystem) {
        return false;
    }

    auto json = initJsonDoc("v16.Boot.BootService", JSON_OBJECT_SIZE(3));

    json["bootNr"] = bstats.bootNr;
    json["lastSuccess"] = bstats.lastBootSuccess;
    json["MicroOcppVersion"] = (const char*)bstats.microOcppVersion;

    return FilesystemUtils::storeJson(filesystem, MO_FILENAME_PREFIX "bootstats.jsn", json);
}

bool BootService::recover(std::shared_ptr<FilesystemAdapter> filesystem, BootStats& bstats) {
    if (!filesystem) {
        return false;
    }

    bool success = FilesystemUtils::remove_if(filesystem, [] (const char *fname) -> bool {
        return !strncmp(fname, "sd", strlen("sd")) ||
                !strncmp(fname, "tx", strlen("tx")) ||
                !strncmp(fname, "sc-", strlen("sc-")) ||
                !strncmp(fname, "reservation", strlen("reservation")) ||
                !strncmp(fname, "client-state", strlen("client-state"));
    });
    MO_DBG_ERR("clear local state files (recovery): %s", success ? "success" : "not completed");

    return success;
}

bool BootService::migrate(std::shared_ptr<FilesystemAdapter> filesystem, BootStats& bstats) {
    if (!filesystem) {
        return false;
    }

    bool success = true;

    if (strcmp(bstats.microOcppVersion, MO_VERSION)) {
        MO_DBG_INFO("migrate persistent storage to MO v" MO_VERSION);
        success = FilesystemUtils::remove_if(filesystem, [] (const char *fname) -> bool {
            return !strncmp(fname, "sd", strlen("sd")) ||
                    !strncmp(fname, "tx", strlen("tx")) ||
                    !strncmp(fname, "op", strlen("op")) ||
                    !strncmp(fname, "sc-", strlen("sc-")) ||
                    !strcmp(fname, "client-state.cnf") ||
                    !strcmp(fname, "arduino-ocpp.cnf") ||
                    !strcmp(fname, "ocpp-creds.jsn");
        });

        snprintf(bstats.microOcppVersion, sizeof(bstats.microOcppVersion), "%s", MO_VERSION);
        MO_DBG_DEBUG("clear local state files (migration): %s", success ? "success" : "not completed");
    }
    return success;
}

// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Certificates/Certificate_c.h>

#if MO_ENABLE_CERT_MGMT

#include <MicroOcpp/Debug.h>

#include <string.h>

namespace MicroOcpp {

/*
 * C++ wrapper for the C-style certificate interface
 */
class CertificateStoreC : public CertificateStore {
private:
    ocpp_cert_store *certstore = nullptr;
public:
    CertificateStoreC(ocpp_cert_store *certstore) : certstore(certstore) {

    }

    ~CertificateStoreC() = default;

    GetInstalledCertificateStatus getCertificateIds(const std::vector<GetCertificateIdType>& certificateType, std::vector<CertificateChainHash>& out) override {
        out.clear();

        ocpp_cert_chain_hash *cch;

        auto ret = certstore->getCertificateIds(certstore->user_data, &certificateType[0], certificateType.size(), &cch);
        if (ret == GetInstalledCertificateStatus_NotFound || !cch) {
            return GetInstalledCertificateStatus_NotFound;
        }

        bool err = false;
        
        for (ocpp_cert_chain_hash *it = cch; it && !err; it = it->next) {
            out.emplace_back();
            auto &chd_el = out.back();
            chd_el.certificateType = it->certType;
            memcpy(&chd_el.certificateHashData, &it->certHashData, sizeof(ocpp_cert_hash));
        }

        while (cch) {
            ocpp_cert_chain_hash *el = cch;
            cch = cch->next;
            el->invalidate(el);
        }

        if (err) {
            out.clear();
        }

        return out.empty() ?
                GetInstalledCertificateStatus_NotFound :
                GetInstalledCertificateStatus_Accepted;
    }

    DeleteCertificateStatus deleteCertificate(const CertificateHash& hash) override {
        return certstore->deleteCertificate(certstore->user_data, &hash);
    }

    InstallCertificateStatus installCertificate(InstallCertificateType certificateType, const char *certificate) override {
        return certstore->installCertificate(certstore->user_data, certificateType, certificate);
    }
};

std::unique_ptr<CertificateStore> makeCertificateStoreCwrapper(ocpp_cert_store *certstore) {
    return std::unique_ptr<CertificateStore>(new CertificateStoreC(certstore));
}

} //namespace MicroOcpp
#endif //MO_ENABLE_CERT_MGMT

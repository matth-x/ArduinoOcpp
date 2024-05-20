// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Certificates/CertificateMbedTLS.h>

#if MO_ENABLE_CERT_MGMT && MO_ENABLE_CERT_STORE_MBEDTLS

#include <string.h>

#include <mbedtls/version.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/md.h>
#include <mbedtls/error.h>

#include <MicroOcpp/Debug.h>

bool ocpp_get_cert_hash(mbedtls_x509_crt& cacert, HashAlgorithmType hashAlg, ocpp_cert_hash *out) {

    if (cacert.next) {
        MO_DBG_ERR("only sole root certs supported");
        return false;
    }

    out->hashAlgorithm = hashAlg;

    mbedtls_md_type_t hash_alg_mbed;

    switch (hashAlg) {
        case HashAlgorithmType_SHA256:
            hash_alg_mbed = MBEDTLS_MD_SHA256;
            break;
        case HashAlgorithmType_SHA384:
            hash_alg_mbed = MBEDTLS_MD_SHA384;
            break;
        case HashAlgorithmType_SHA512:
            hash_alg_mbed = MBEDTLS_MD_SHA512;
            break;
        default:
            MO_DBG_ERR("internal error");
            return false;
    }

    const mbedtls_md_info_t *md_info;

    md_info = mbedtls_md_info_from_type(hash_alg_mbed);
    if (!md_info) {
        MO_DBG_ERR("hash algorithmus not supported");
        return false;
    }

    size_t hash_size = mbedtls_md_get_size(md_info);
    if (hash_size > sizeof(out->issuerNameHash)) {
        MO_DBG_ERR("internal error");
        return false;
    }

    if (!cacert.issuer_raw.p) {
        MO_DBG_ERR("missing issuer name");
        return false;
    }

    int ret;

    if ((ret = mbedtls_md(md_info, cacert.issuer_raw.p, cacert.issuer_raw.len, out->issuerNameHash))) {
        MO_DBG_ERR("mbedtls_md: %i", ret);
        return false;
    }

    // copy public key into pk_buf to create issuerKeyHash
    size_t pk_size = cacert.pk_raw.len;
    unsigned char *pk_buf = new unsigned char[pk_size];
    if (!pk_buf) {
        MO_DBG_ERR("OOM (alloc size %zu)", pk_size);
        return false;
    }
    int pk_len = 0;
    unsigned char *pk_p = pk_buf + pk_size;

    bool pk_err = false;

    if ((pk_len = mbedtls_pk_write_pubkey(&pk_p, pk_buf, &cacert.pk)) <= 0) {
        pk_err = true;
        char err [100];
        mbedtls_strerror(ret, err, 100);
        MO_DBG_ERR("mbedtls_pk_write_pubkey_pem: %i -- %s", pk_len, err);
        // return after pk_buf has been freed
    }

    if (!pk_err) {
        if ((ret = mbedtls_md(md_info, pk_p, pk_len, out->issuerKeyHash))) {
            pk_err = true;
            MO_DBG_ERR("mbedtls_md: %i", ret);
        }
    }

    delete[] pk_buf;
    if (pk_err) {
        return false;
    }

    size_t serial_begin = 0; //trunicate leftmost 0x00 bytes
    for (; serial_begin < cacert.serial.len - 1; serial_begin++) { //keep at least 1 byte, even if 0x00
        if (cacert.serial.p[serial_begin] != 0) {
            break;
        }
    }

    out->serialNumberLen = std::min(cacert.serial.len - serial_begin, sizeof(out->serialNumber));
    memcpy(out->serialNumber, cacert.serial.p + serial_begin, out->serialNumberLen);

    return true;
}

bool ocpp_get_cert_hash(const unsigned char *buf, size_t len, HashAlgorithmType hashAlg, ocpp_cert_hash *out) {

    mbedtls_x509_crt cacert;
    mbedtls_x509_crt_init(&cacert);

    bool success = false;
    int ret;

    if((ret = mbedtls_x509_crt_parse(&cacert, buf, len + 1)) >= 0) {
        success = ocpp_get_cert_hash(cacert, hashAlg, out);
    } else {
        char err [100];
        mbedtls_strerror(ret, err, 100);
        MO_DBG_ERR("mbedtls_x509_crt_parse: %i -- %s", ret, err);
    }

    mbedtls_x509_crt_free(&cacert);
    return success;
}

namespace MicroOcpp {

class CertificateStoreMbedTLS : public CertificateStore {
private:
    std::shared_ptr<FilesystemAdapter> filesystem;

    bool getCertHash(const char *fn, HashAlgorithmType hashAlg, CertificateHash& out) {
        size_t fsize;
        if (filesystem->stat(fn, &fsize) != 0) {
            MO_DBG_ERR("certificate does not exist: %s", fn);
            return false;
        }

        if (fsize >= MO_MAX_CERT_SIZE) {
            MO_DBG_ERR("cert file exceeds limit: %s,  %zuB", fn, fsize);
            return false;
        }

        auto file = filesystem->open(fn, "r");
        if (!file) {
            MO_DBG_ERR("could not open file: %s", fn);
            return false;
        }

        unsigned char *buf = new unsigned char[fsize + 1];
        if (!buf) {
            MO_DBG_ERR("OOM");
            return false;
        }

        bool success = true;

        size_t ret;
        if ((ret = file->read((char*) buf, fsize)) != fsize) {
            MO_DBG_ERR("read error: %zu (expect %zu)", ret, fsize);
            success = false;
        }

        buf[fsize] = '\0';

        if (success) {
            success &= ocpp_get_cert_hash(buf, fsize, hashAlg, &out);
        }

        if (!success) {
            MO_DBG_ERR("could not read cert: %s", fn);
        }

        delete[] buf;
        return success;
    }
public:
    CertificateStoreMbedTLS(std::shared_ptr<FilesystemAdapter> filesystem)
            : filesystem(filesystem) {

    }

    GetInstalledCertificateStatus getCertificateIds(const std::vector<GetCertificateIdType>& certificateType, std::vector<CertificateChainHash>& out) override {
        out.clear();

        for (auto certType : certificateType) {
            const char *certTypeFnStr = nullptr;
            switch (certType) {
                case GetCertificateIdType_CSMSRootCertificate:
                    certTypeFnStr = MO_CERT_FN_CSMS_ROOT;
                    break;
                case GetCertificateIdType_ManufacturerRootCertificate:
                    certTypeFnStr = MO_CERT_FN_MANUFACTURER_ROOT;
                    break;
                default:
                    MO_DBG_ERR("only CSMS / Manufacturer root supported");
                    break;
            }

            if (!certTypeFnStr) {
                continue;
            }

            for (size_t i = 0; i < MO_CERT_STORE_SIZE; i++) {
                char fn [MO_MAX_PATH_SIZE];
                if (!printCertFn(certTypeFnStr, i, fn, MO_MAX_PATH_SIZE)) {
                    MO_DBG_ERR("internal error");
                    out.clear();
                    break;
                }

                size_t msize;
                if (filesystem->stat(fn, &msize) != 0) {
                    continue; //no cert installed at this slot
                }

                out.emplace_back();
                CertificateChainHash& rootCert = out.back();

                rootCert.certificateType = certType;

                if (!getCertHash(fn, HashAlgorithmType_SHA256, rootCert.certificateHashData)) {
                    MO_DBG_ERR("could not create hash: %s", fn);
                    out.pop_back();
                    continue;
                }
            }
        }

        return out.empty() ?
                GetInstalledCertificateStatus_NotFound :
                GetInstalledCertificateStatus_Accepted;
    }

    DeleteCertificateStatus deleteCertificate(const CertificateHash& hash) override {
        bool err = false;

        //enumerate all certs possibly installed by this CertStore implementation
        for (const char *certTypeFnStr : {MO_CERT_FN_CSMS_ROOT, MO_CERT_FN_MANUFACTURER_ROOT}) {
            for (size_t i = 0; i < MO_CERT_STORE_SIZE; i++) {

                char fn [MO_MAX_PATH_SIZE] = {'\0'}; //cert fn on flash storage

                if (!printCertFn(certTypeFnStr, i, fn, MO_MAX_PATH_SIZE)) {
                    MO_DBG_ERR("internal error");
                    return DeleteCertificateStatus_Failed;
                }

                size_t msize;
                if (filesystem->stat(fn, &msize) != 0) {
                    continue; //no cert installed at this slot
                }

                CertificateHash probe;
                if (!getCertHash(fn, hash.hashAlgorithm, probe)) {
                    MO_DBG_ERR("could not create hash: %s", fn);
                    err = true;
                    continue;
                }

                if (ocpp_cert_equals(&probe, &hash)) {
                    //found, delete

                    bool success = filesystem->remove(fn);
                    return success ?
                        DeleteCertificateStatus_Accepted :
                        DeleteCertificateStatus_Failed;
                }
            }
        }

        return err ?
            DeleteCertificateStatus_Failed :
            DeleteCertificateStatus_NotFound;
    }

    InstallCertificateStatus installCertificate(InstallCertificateType certificateType, const char *certificate) override {
        const char *certTypeFnStr;
        GetCertificateIdType certTypeGetType;
        switch (certificateType) {
            case InstallCertificateType_CSMSRootCertificate:
                certTypeFnStr = MO_CERT_FN_CSMS_ROOT;
                certTypeGetType = GetCertificateIdType_CSMSRootCertificate;
                break;
            case InstallCertificateType_ManufacturerRootCertificate:
                certTypeFnStr = MO_CERT_FN_MANUFACTURER_ROOT;
                certTypeGetType = GetCertificateIdType_ManufacturerRootCertificate;
                break;
            default:
                MO_DBG_ERR("only CSMS / Manufacturer root supported");
                return InstallCertificateStatus_Failed;
        }

        //check if this implementation is able to parse incoming cert
        CertificateHash certId;
        if (!ocpp_get_cert_hash((const unsigned char*)certificate, strlen(certificate), HashAlgorithmType_SHA256, &certId)) {
            MO_DBG_ERR("unable to parse cert");
            return InstallCertificateStatus_Rejected;
        }

#if MO_DBG_LEVEL >= MO_DL_DEBUG
        {
            MO_DBG_DEBUG("Cert ID:");
            MO_DBG_DEBUG("hashAlgorithm: %s", HashAlgorithmLabel(certId.hashAlgorithm));
            char buf [MO_CERT_HASH_ISSUER_NAME_KEY_SIZE];

            ocpp_cert_print_issuerNameHash(&certId, buf, sizeof(buf));
            MO_DBG_DEBUG("issuerNameHash: %s", buf);

            ocpp_cert_print_issuerKeyHash(&certId, buf, sizeof(buf));
            MO_DBG_DEBUG("issuerKeyHash: %s", buf);

            ocpp_cert_print_serialNumber(&certId, buf, sizeof(buf));
            MO_DBG_DEBUG("serialNumber: %s", buf);
        }
#endif // MO_DBG_LEVEL >= MO_DL_DEBUG

        //check if cert is already stored on flash
        std::vector<CertificateChainHash> installedCerts;
        auto ret = getCertificateIds({certTypeGetType}, installedCerts);
        if (ret == GetInstalledCertificateStatus_Accepted) {
            for (auto &installedCert : installedCerts) {
                if (ocpp_cert_equals(&installedCert.certificateHashData, &certId)) {
                    MO_DBG_INFO("certificate already installed");
                    return InstallCertificateStatus_Accepted;
                }
                for (auto& installedChild : installedCert.childCertificateHashData) {
                    if (ocpp_cert_equals(&installedChild, &certId)) {
                        MO_DBG_INFO("certificate already installed");
                        return InstallCertificateStatus_Accepted;
                    }
                }
            }
        }

        char fn [MO_MAX_PATH_SIZE] = {'\0'}; //cert fn on flash storage

        //check for free cert slot
        for (size_t i = 0; i < MO_CERT_STORE_SIZE; i++) {
            if (!printCertFn(certTypeFnStr, i, fn, MO_MAX_PATH_SIZE)) {
                MO_DBG_ERR("invalid cert fn");
                return InstallCertificateStatus_Failed;
            }

            size_t msize;
            if (filesystem->stat(fn, &msize) != 0) {
                //found free slot; fn contains result
                break;
            } else {
                //this slot is already occupied; invalidate fn and try next
                fn[0] = '\0';
            }
        }

        if (fn[0] == '\0') {
            MO_DBG_ERR("exceed maximum number of certs; must delete before");
            return InstallCertificateStatus_Rejected;
        }

        auto file = filesystem->open(fn, "w");
        if (!file) {
            MO_DBG_ERR("could not open file");
            return InstallCertificateStatus_Failed;
        }

        size_t cert_len = strlen(certificate);
        auto written = file->write(certificate, cert_len);
        if (written < cert_len) {
            MO_DBG_ERR("file write error");
            file.reset();
            filesystem->remove(fn);
            return InstallCertificateStatus_Failed;
        }

        MO_DBG_INFO("installed certificate: %s", fn);
        return InstallCertificateStatus_Accepted;
    }
};

std::unique_ptr<CertificateStore> makeCertificateStoreMbedTLS(std::shared_ptr<FilesystemAdapter> filesystem) {
    if (!filesystem) {
        MO_DBG_WARN("default Certificate Store requires FS");
        return nullptr;
    }
    return std::unique_ptr<CertificateStore>(new CertificateStoreMbedTLS(filesystem));
}

bool printCertFn(const char *certType, size_t index, char *buf, size_t bufsize) {
    if (!certType || !*certType || index >= MO_CERT_STORE_SIZE || !buf) {
        MO_DBG_ERR("invalid args");
        return false;
    }

    auto ret = snprintf(buf, bufsize, MO_FILENAME_PREFIX MO_CERT_FN_PREFIX "%s" "-%zu" MO_CERT_FN_SUFFIX,
            certType, index);
    if (ret < 0 || ret >= (int)bufsize) {
        MO_DBG_ERR("fn error: %i", ret);
        return false;
    }
    return true;
}

} //namespace MicroOcpp

#endif //MO_ENABLE_CERT_MGMT && MO_ENABLE_CERT_STORE_MBEDTLS

// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Certificates/CertificateMbedTLS.h>

#if MO_ENABLE_MBEDTLS

#include <string.h>

#include <mbedtls/version.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/md.h>
#include <mbedtls/oid.h>
#include <mbedtls/error.h>

#include <MicroOcpp/Debug.h>

#define MO_X509_OID_COMMON_NAME "2.5.4.3" //object-identifier of x509 common-name

bool ocpp_get_cert_hash(const unsigned char *buf, size_t len, HashAlgorithmEnumType_c hashAlg, char *issuerNameHash, char *issuerKeyHash, char *serialNumber) {

    mbedtls_x509_crt cacert;
    mbedtls_x509_crt_init(&cacert);

    int ret;

    if((ret = mbedtls_x509_crt_parse(&cacert, buf, len + 1)) < 0) {
        char err [100];
        mbedtls_strerror(ret, err, 100);
        MO_DBG_ERR("mbedtls_x509_crt_parse: %i -- %s", ret, err);
        return false;
    }

    if (cacert.next) {
        MO_DBG_ERR("only sole root certs supported");
        return false;
    }

    const unsigned char *subject_cn_p = nullptr;
    size_t subject_cn_len = 0;
    for (mbedtls_x509_name *it = &cacert.subject; it; it = it->next) {
        char oid_cstr [50];
        if ((ret = mbedtls_oid_get_numeric_string(oid_cstr, 50, &it->oid)) < 0) {
            MO_DBG_ERR("internal error: %i", ret);
            continue; //there is an oid which exceeds the bufsize, but the target oid does fit so continue
        }

        if (!strcmp(oid_cstr, MO_X509_OID_COMMON_NAME)) {
            subject_cn_p = it->val.p;
            subject_cn_len = it->val.len;
            break;
        }
    }

    if (!subject_cn_p || !subject_cn_len) {
        MO_DBG_ERR("could not find subject common name");
        return false;
    }

    const unsigned char *issuer_cn_p = nullptr;
    size_t issuer_cn_len = 0;
    for (mbedtls_x509_name *it = &cacert.issuer; it; it = it->next) {
        char oid_cstr [50];
        if ((ret = mbedtls_oid_get_numeric_string(oid_cstr, 50, &it->oid)) < 0) {
            MO_DBG_ERR("internal error: %i", ret);
            continue; //there is an oid which exceeds the bufsize, but the target oid does fit so continue
        }

        if (!strcmp(oid_cstr, MO_X509_OID_COMMON_NAME)) {
            issuer_cn_p = it->val.p;
            issuer_cn_len = it->val.len;
            break;
        }
    }

    if (!issuer_cn_p || !issuer_cn_len) {
        MO_DBG_ERR("could not find issuer common name");
        return false;
    }

    if (subject_cn_len != issuer_cn_len || strncmp((const char*) subject_cn_p, (const char*) issuer_cn_p, subject_cn_len)) {
        MO_DBG_ERR("only support self-signed root certs");
        return false;
    }
    
    mbedtls_md_type_t hash_alg_mbed;

    switch (hashAlg) {
        case HashAlgorithmEnumType_c::ENUM_HA_SHA256:
            hash_alg_mbed = MBEDTLS_MD_SHA256;
            break;
        case HashAlgorithmEnumType_c::ENUM_HA_SHA384:
            hash_alg_mbed = MBEDTLS_MD_SHA384;
            break;
        case HashAlgorithmEnumType_c::ENUM_HA_SHA512:
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

    unsigned char hash_buf [64]; //at most 512 Bits (SHA512), equalling 64 Bytes

    size_t hash_size = mbedtls_md_get_size(md_info);
    if (hash_size > sizeof(hash_buf)) {
        MO_DBG_ERR("internal error");
        return false;
    }

    if ((ret = mbedtls_md(md_info, issuer_cn_p, issuer_cn_len, hash_buf))) {
        MO_DBG_ERR("mbedtls_md: %i", ret);
        return false;
    }

    for (size_t i = 0; i < hash_size; i ++) {
        sprintf(issuerNameHash + 2 * i, "%02X", hash_buf[i]);
    }

    unsigned char *pk_p;
    size_t pk_len;

#if MBEDTLS_VERSION_MAJOR == 2 && MBEDTLS_VERSION_MINOR <= 16
    unsigned char pk_buf [256];
    if ((ret = mbedtls_pk_write_pubkey_pem(&cacert.pk, pk_buf, 256)) < 0) {
        MO_DBG_ERR("mbedtls_md: %i", ret);
        return false;
    }
    pk_p = pk_buf;
    pk_len = strnlen((const char*)pk_buf, 256);
#else //tested on MbedTLS 2.28.1
    pk_p = cacert.pk_raw.p;
    pk_len = cacert.pk_raw.len;
#endif // MbedTLS version

    if ((ret = mbedtls_md(md_info, pk_p, pk_len, hash_buf))) {
        MO_DBG_ERR("mbedtls_md: %i", ret);
        return false;
    }

    for (size_t i = 0; i < hash_size; i ++) {
        sprintf(issuerKeyHash + 2*i, "%02X", hash_buf[i]);
    }

    size_t serial_begin = 0; //trunicate leftmost 0x00 bytes
    for (; serial_begin < cacert.serial.len - 1; serial_begin++) { //keep at least 1 byte, even if 0x00
        if (cacert.serial.p[serial_begin] != 0) {
            break;
        }
    }

    for (size_t i = 0; i < std::min(cacert.serial.len - serial_begin, (size_t) ((MO_CERT_HASH_SERIAL_NUMBER_SIZE - 1)/2)); i++) {
        sprintf(serialNumber + 2*i, "%02X", cacert.serial.p[i + serial_begin]);
    }

    return true;
}

namespace MicroOcpp {

class CertificateStoreMbedTLS : public CertificateStore {
private:
    std::shared_ptr<FilesystemAdapter> filesystem;

    bool getCertHash(const char *fn, HashAlgorithmEnumType hashAlg, CertificateHash& out) {
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
            success &= getCertHash(buf, fsize, hashAlg, out);
        }

        if (!success) {
            MO_DBG_ERR("could not read cert: %s", fn);
            (void)0;
        }

        delete[] buf;
        return success;
    }

    bool getCertHash(const unsigned char *buf, size_t len, HashAlgorithmEnumType hashAlg, CertificateHash& out) {
        
        HashAlgorithmEnumType_c ha;

        switch (hashAlg) {
            case HashAlgorithmEnumType::SHA256:
                ha = ENUM_HA_SHA256;
                break;
            case HashAlgorithmEnumType::SHA384:
                ha = ENUM_HA_SHA384;
                break;
            case HashAlgorithmEnumType::SHA512:
                ha = ENUM_HA_SHA512;
                break;
            default:
                MO_DBG_ERR("internal error");
                return false;
        }

        out.hashAlgorithm = hashAlg;
        return ocpp_get_cert_hash(buf, len, ha, out.issuerNameHash, out.issuerKeyHash, out.serialNumber);
    }
public:
    CertificateStoreMbedTLS(std::shared_ptr<FilesystemAdapter> filesystem)
            : filesystem(filesystem) {

    }

    GetInstalledCertificateStatus getCertificateIds(GetCertificateIdType certificateType, std::vector<CertificateChainHash>& out) override {
        const char *certTypeFnStr = nullptr;
        switch (certificateType) {
            case GetCertificateIdType::CSMSRootCertificate:
                certTypeFnStr = MO_CERT_FN_CSMS_ROOT;
                break;
            case GetCertificateIdType::ManufacturerRootCertificate:
                certTypeFnStr = MO_CERT_FN_MANUFACTURER_ROOT;
                break;
            default:
                MO_DBG_ERR("only CSMS / Manufacturer root supported");
                break;
        }

        if (!certTypeFnStr) {
            return GetInstalledCertificateStatus::NotFound;
        }

        out.clear();

        for (size_t i = 0; i < MO_CERT_STORE_SIZE; i++) {
            char fn [MO_MAX_PATH_SIZE];
            if (!printCertFn(certTypeFnStr, i, fn, MO_MAX_PATH_SIZE)) {
                MO_DBG_ERR("internal error");
                return GetInstalledCertificateStatus::NotFound;
            }

            size_t msize;
            if (filesystem->stat(fn, &msize) != 0) {
                continue; //no cert installed at this slot
            }

            out.emplace_back();
            CertificateChainHash& rootCert = out.back();

            rootCert.certificateType = certificateType;

            if (!getCertHash(fn, HashAlgorithmEnumType::SHA256, rootCert.certificateHashData)) {
                MO_DBG_ERR("could not create hash: %s", fn);
                out.pop_back();
                continue;
            }
        }

        return out.empty() ?
                GetInstalledCertificateStatus::NotFound :
                GetInstalledCertificateStatus::Accepted;
    }

    DeleteCertificateStatus deleteCertificate(const CertificateHash& hash) override {
        bool err = false;

        //enumerate all certs possibly installed by this CertStore implementation
        for (const char *certTypeFnStr : {MO_CERT_FN_CSMS_ROOT, MO_CERT_FN_MANUFACTURER_ROOT}) {
            for (size_t i = 0; i < MO_CERT_STORE_SIZE; i++) {

                char fn [MO_MAX_PATH_SIZE] = {'\0'}; //cert fn on flash storage

                if (!printCertFn(certTypeFnStr, i, fn, MO_MAX_PATH_SIZE)) {
                    MO_DBG_ERR("internal error");
                    return DeleteCertificateStatus::Failed;
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

                if (probe.equals(hash)) {
                    //found, delete

                    bool success = filesystem->remove(fn);
                    return success ?
                        DeleteCertificateStatus::Accepted :
                        DeleteCertificateStatus::Failed;
                }
            }
        }

        return err ?
            DeleteCertificateStatus::Failed :
            DeleteCertificateStatus::NotFound;
    }

    InstallCertificateStatus installCertificate(InstallCertificateType certificateType, const char *certificate) override {
        const char *certTypeFnStr = nullptr;
        switch (certificateType) {
            case InstallCertificateType::CSMSRootCertificate:
                certTypeFnStr = MO_CERT_FN_CSMS_ROOT;
                break;
            case InstallCertificateType::ManufacturerRootCertificate:
                certTypeFnStr = MO_CERT_FN_MANUFACTURER_ROOT;
                break;
            default:
                MO_DBG_ERR("only CSMS / Manufacturer root supported");
                break;
        }

        if (!certTypeFnStr) {
            return InstallCertificateStatus::Failed;
        }

        //check if this implementation is able to parse incoming cert
        {
            CertificateHash certId;
            if (!getCertHash((const unsigned char*)certificate, strlen(certificate), HashAlgorithmEnumType::SHA256, certId)) {
                MO_DBG_ERR("unable to parse cert");
                return InstallCertificateStatus::Rejected;
            }
            MO_DBG_DEBUG("Cert ID:");
            MO_DBG_DEBUG("hashAlgorithm: %s", certId.getHashAlgorithmCStr());
            MO_DBG_DEBUG("issuerNameHash: %s", certId.issuerNameHash);
            MO_DBG_DEBUG("issuerKeyHash: %s", certId.issuerKeyHash);
            MO_DBG_DEBUG("serialNumber: %s", certId.serialNumber);
        }

        char fn [MO_MAX_PATH_SIZE] = {'\0'}; //cert fn on flash storage

        //check for free cert slot
        for (size_t i = 0; i < MO_CERT_STORE_SIZE; i++) {
            if (!printCertFn(certTypeFnStr, i, fn, MO_MAX_PATH_SIZE)) {
                MO_DBG_ERR("invalid cert fn");
                return InstallCertificateStatus::Failed;
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
            return InstallCertificateStatus::Rejected;
        }

        auto file = filesystem->open(fn, "w");
        if (!file) {
            MO_DBG_ERR("could not open file");
            return InstallCertificateStatus::Failed;
        }

        size_t cert_len = strlen(certificate);
        auto written = file->write(certificate, cert_len);
        if (written < cert_len) {
            MO_DBG_ERR("file write error");
            file.reset();
            filesystem->remove(fn);
            return InstallCertificateStatus::Failed;
        }

        MO_DBG_INFO("installed certificate: %s", fn);
        return InstallCertificateStatus::Accepted;
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

#endif //MO_ENABLE_MBEDTLS

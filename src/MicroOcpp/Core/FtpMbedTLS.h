// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_FTP_MBEDTLS_H
#define MO_FTP_MBEDTLS_H

/*
 * Built-in FTP client (depends on MbedTLS)
 *
 * Moved from https://github.com/matth-x/MicroFtp
 * 
 * Currently, the compatibility with the following FTP servers has been tested:
 * 
 * | Server                                                                | FTP | FTPS |
 * | --------------------------------------------------------------------- | --- | ---- |
 * | [vsftp](https://security.appspot.com/vsftpd.html)                     |     |  x   |
 * | [Rebex](https://www.rebex.net/)                                       |  x  |  x   |
 * | [Windows Server 2022](https://www.microsoft.com/en-us/windows-server) |  x  |  x   |
 * | [SFTPGo](https://github.com/drakkan/sftpgo)                           |  x  |      |
 * 
 */

#include <MicroOcpp/Platform.h>

#if MO_ENABLE_MBEDTLS

#include <memory>

#include <MicroOcpp/Core/Ftp.h>

namespace MicroOcpp {

std::unique_ptr<FtpClient> makeFtpClientMbedTLS(bool tls_only = false, const char *client_cert = nullptr, const char *client_key = nullptr);

} //namespace MicroOcpp

#endif //MO_ENABLE_MBEDTLS

#endif

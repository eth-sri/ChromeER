// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/x509_certificate_model.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "net/base/test_data_directory.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_NSS)
#include "net/cert/nss_cert_database.h"
#endif

TEST(X509CertificateModelTest, GetCertNameOrNicknameAndGetTitle) {
  scoped_refptr<net::X509Certificate> cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(),
                              "root_ca_cert.pem"));
  ASSERT_TRUE(cert.get());
  EXPECT_EQ(
      "Test Root CA",
      x509_certificate_model::GetCertNameOrNickname(cert->os_cert_handle()));

  scoped_refptr<net::X509Certificate> punycode_cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(),
                              "punycodetest.der"));
  ASSERT_TRUE(punycode_cert.get());
  EXPECT_EQ("xn--wgv71a119e.com (日本語.com)",
            x509_certificate_model::GetCertNameOrNickname(
                punycode_cert->os_cert_handle()));

  scoped_refptr<net::X509Certificate> no_cn_cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(),
                              "no_subject_common_name_cert.pem"));
  ASSERT_TRUE(no_cn_cert.get());
#if defined(USE_OPENSSL)
  EXPECT_EQ("emailAddress=wtc@google.com",
            x509_certificate_model::GetCertNameOrNickname(
                no_cn_cert->os_cert_handle()));
#else
  // Temp cert has no nickname.
  EXPECT_EQ("",
            x509_certificate_model::GetCertNameOrNickname(
                no_cn_cert->os_cert_handle()));
#endif

  EXPECT_EQ("xn--wgv71a119e.com",
            x509_certificate_model::GetTitle(
                punycode_cert->os_cert_handle()));

#if defined(USE_OPENSSL)
  EXPECT_EQ("emailAddress=wtc@google.com",
            x509_certificate_model::GetTitle(
                no_cn_cert->os_cert_handle()));
#else
  EXPECT_EQ("E=wtc@google.com",
            x509_certificate_model::GetTitle(
                no_cn_cert->os_cert_handle()));
#endif

  scoped_refptr<net::X509Certificate> no_cn_cert2(net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "ct-test-embedded-cert.pem"));
  ASSERT_TRUE(no_cn_cert2.get());
  EXPECT_EQ("L=Erw Wen,ST=Wales,O=Certificate Transparency,C=GB",
            x509_certificate_model::GetTitle(no_cn_cert2->os_cert_handle()));
}

TEST(X509CertificateModelTest, GetExtensions) {
  {
    scoped_refptr<net::X509Certificate> cert(net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "root_ca_cert.pem"));
    ASSERT_TRUE(cert.get());

    x509_certificate_model::Extensions extensions;
    x509_certificate_model::GetExtensions(
        "critical", "notcrit", cert->os_cert_handle(), &extensions);
    ASSERT_EQ(3U, extensions.size());

    EXPECT_EQ("Certificate Basic Constraints", extensions[0].name);
    EXPECT_EQ(
        "critical\nIs a Certification Authority\n"
        "Maximum number of intermediate CAs: unlimited",
        extensions[0].value);

    EXPECT_EQ("Certificate Subject Key ID", extensions[1].name);
    EXPECT_EQ(
        "notcrit\nKey ID: 2B 88 93 E1 D2 54 50 F4 B8 A4 20 BD B1 79 E6 0B\nAA "
        "EB EC 1A",
        extensions[1].value);

    EXPECT_EQ("Certificate Key Usage", extensions[2].name);
    EXPECT_EQ("critical\nCertificate Signer\nCRL Signer", extensions[2].value);
  }

  {
    scoped_refptr<net::X509Certificate> cert(net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "subjectAltName_sanity_check.pem"));
    x509_certificate_model::Extensions extensions;
    x509_certificate_model::GetExtensions(
        "critical", "notcrit", cert->os_cert_handle(), &extensions);
    ASSERT_EQ(2U, extensions.size());
    EXPECT_EQ("Certificate Subject Alternative Name", extensions[1].name);
    EXPECT_EQ(
        "notcrit\nIP Address: 127.0.0.2\nIP Address: fe80::1\nDNS Name: "
        "test.example\nEmail Address: test@test.example\nOID.1.2.3.4: 0C 09 69 "
        "67 6E 6F 72 65 20 6D 65\nX.500 Name: CN = 127.0.0.3\n\n",
        extensions[1].value);
  }

  {
    scoped_refptr<net::X509Certificate> cert(net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "foaf.me.chromium-test-cert.der"));
    x509_certificate_model::Extensions extensions;
    x509_certificate_model::GetExtensions(
        "critical", "notcrit", cert->os_cert_handle(), &extensions);
    ASSERT_EQ(5U, extensions.size());
    EXPECT_EQ("Netscape Certificate Comment", extensions[1].name);
    EXPECT_EQ("notcrit\nOpenSSL Generated Certificate", extensions[1].value);
  }

  {
    scoped_refptr<net::X509Certificate> cert(net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "2029_globalsign_com_cert.pem"));
    x509_certificate_model::Extensions extensions;
    x509_certificate_model::GetExtensions(
        "critical", "notcrit", cert->os_cert_handle(), &extensions);
    ASSERT_EQ(9U, extensions.size());

    EXPECT_EQ("Certificate Subject Key ID", extensions[0].name);
    EXPECT_EQ(
        "notcrit\nKey ID: 59 BC D9 69 F7 B0 65 BB C8 34 C5 D2 C2 EF 17 78\nA6 "
        "47 1E 8B",
        extensions[0].value);

    EXPECT_EQ("Certification Authority Key ID", extensions[1].name);
    EXPECT_EQ(
        "notcrit\nKey ID: 8A FC 14 1B 3D A3 59 67 A5 3B E1 73 92 A6 62 91\n7F "
        "E4 78 30\n",
        extensions[1].value);

    EXPECT_EQ("Authority Information Access", extensions[2].name);
    EXPECT_EQ(
        "notcrit\nCA Issuers: "
        "URI: http://secure.globalsign.net/cacert/SHA256extendval1.crt\n",
        extensions[2].value);

    EXPECT_EQ("CRL Distribution Points", extensions[3].name);
    EXPECT_EQ("notcrit\nURI: http://crl.globalsign.net/SHA256ExtendVal1.crl\n",
              extensions[3].value);

    EXPECT_EQ("Certificate Basic Constraints", extensions[4].name);
    EXPECT_EQ("notcrit\nIs not a Certification Authority\n",
              extensions[4].value);

    EXPECT_EQ("Certificate Key Usage", extensions[5].name);
    EXPECT_EQ(
        "critical\nSigning\nNon-repudiation\nKey Encipherment\n"
        "Data Encipherment",
        extensions[5].value);

    EXPECT_EQ("Extended Key Usage", extensions[6].name);
    EXPECT_EQ(
        "notcrit\nTLS WWW Server Authentication (OID.1.3.6.1.5.5.7.3.1)\n"
        "TLS WWW Client Authentication (OID.1.3.6.1.5.5.7.3.2)\n",
        extensions[6].value);

    EXPECT_EQ("Certificate Policies", extensions[7].name);
    EXPECT_EQ(
        "notcrit\nOID.1.3.6.1.4.1.4146.1.1:\n"
        "  Certification Practice Statement Pointer:"
        "    http://www.globalsign.net/repository/\n",
        extensions[7].value);

    EXPECT_EQ("Netscape Certificate Type", extensions[8].name);
    EXPECT_EQ("notcrit\nSSL Client Certificate\nSSL Server Certificate",
              extensions[8].value);
  }

  {
    scoped_refptr<net::X509Certificate> cert(net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "diginotar_public_ca_2025.pem"));
    x509_certificate_model::Extensions extensions;
    x509_certificate_model::GetExtensions(
        "critical", "notcrit", cert->os_cert_handle(), &extensions);
    ASSERT_EQ(7U, extensions.size());

    EXPECT_EQ("Authority Information Access", extensions[0].name);
    EXPECT_EQ(
        "notcrit\nOCSP Responder: "
        "URI: http://validation.diginotar.nl\n",
        extensions[0].value);

    EXPECT_EQ("Certificate Basic Constraints", extensions[2].name);
    EXPECT_EQ(
        "critical\nIs a Certification Authority\n"
        "Maximum number of intermediate CAs: 0",
        extensions[2].value);
    EXPECT_EQ("Certificate Policies", extensions[3].name);
    EXPECT_EQ(
        "notcrit\nOID.2.16.528.1.1001.1.1.1.1.5.2.6.4:\n"
        "  Certification Practice Statement Pointer:"
        "    http://www.diginotar.nl/cps\n"
        "  User Notice:\n"
        "    Conditions, as mentioned on our website (www.diginotar.nl), are "
        "applicable to all our products and services.\n",
        extensions[3].value);
  }
}

TEST(X509CertificateModelTest, GetTypeCA) {
  scoped_refptr<net::X509Certificate> cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(),
                              "root_ca_cert.pem"));
  ASSERT_TRUE(cert.get());

#if defined(USE_OPENSSL)
  // Remove this when OpenSSL build implements the necessary functions.
  EXPECT_EQ(net::OTHER_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));
#else
  EXPECT_EQ(net::CA_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));

  // Test that explicitly distrusted CA certs are still returned as CA_CERT
  // type. See http://crbug.com/96654.
  EXPECT_TRUE(net::NSSCertDatabase::GetInstance()->SetCertTrust(
      cert.get(), net::CA_CERT, net::NSSCertDatabase::DISTRUSTED_SSL));

  EXPECT_EQ(net::CA_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));
#endif
}

TEST(X509CertificateModelTest, GetTypeServer) {
  scoped_refptr<net::X509Certificate> cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(),
                              "google.single.der"));
  ASSERT_TRUE(cert.get());

#if defined(USE_OPENSSL)
  // Remove this when OpenSSL build implements the necessary functions.
  EXPECT_EQ(net::OTHER_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));
#else
  // Test mozilla_security_manager::GetCertType with server certs and default
  // trust.  Currently this doesn't work.
  // TODO(mattm): make mozilla_security_manager::GetCertType smarter so we can
  // tell server certs even if they have no trust bits set.
  EXPECT_EQ(net::OTHER_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));

  net::NSSCertDatabase* cert_db = net::NSSCertDatabase::GetInstance();
  // Test GetCertType with server certs and explicit trust.
  EXPECT_TRUE(cert_db->SetCertTrust(
      cert.get(), net::SERVER_CERT, net::NSSCertDatabase::TRUSTED_SSL));

  EXPECT_EQ(net::SERVER_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));

  // Test GetCertType with server certs and explicit distrust.
  EXPECT_TRUE(cert_db->SetCertTrust(
      cert.get(), net::SERVER_CERT, net::NSSCertDatabase::DISTRUSTED_SSL));

  EXPECT_EQ(net::SERVER_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));
#endif
}

// An X.509 v1 certificate with the version field omitted should get
// the default value v1.
TEST(X509CertificateModelTest, GetVersionOmitted) {
  scoped_refptr<net::X509Certificate> cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(),
                              "ndn.ca.crt"));
  ASSERT_TRUE(cert.get());

  EXPECT_EQ("1", x509_certificate_model::GetVersion(cert->os_cert_handle()));
}

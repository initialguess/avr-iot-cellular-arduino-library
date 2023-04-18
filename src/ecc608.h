#ifndef ECC608_H
#define ECC608_H

#include <Arduino.h>

#include <stddef.h>
#include <stdint.h>

#include "cryptoauthlib/lib/atcacert/atcacert.h"
#include "cryptoauthlib/lib/cryptoauthlib.h"

enum EccDataType {
    EMPTY                 = 0,
    AWS_THINGNAME         = 1,
    AWS_ENDPOINT          = 2,
    AZURE_ID_SCOPE        = 3,
    AZURE_IOT_HUB_NAME    = 4,
    AZURE_DEVICE_ID       = 5,
    GOOGLE_PROJECT_ID     = 6,
    GOOGLE_PROJECT_REGION = 7,
    GOOGLE_REGISTRY_ID    = 8,
    GOOGLE_DEVICE_ID      = 9,
    NUM_TYPES // Placeholder, keep this last
};

class ECC608Class {

  private:
    /**
     * @brief Set when #begin() is called.
     */
    bool initialized = false;

    /**
     * @brief Hide constructor in order to enforce a single instance of the
     * class.
     */
    ECC608Class(){};

  public:
    /**
     * @brief Singleton instance.
     */
    static ECC608Class& instance(void) {
        static ECC608Class instance;
        return instance;
    }

    /**
     * @brief Initializes the ECC.
     *
     * @return The enumerations of #ATCA_STATUS. #ATCA_SUCCESS on success.
     */
    ATCA_STATUS begin();

    /**
     * @brief Extract item with given type from ECC slot.
     *
     * This code traverses the list until matching type is found,
     * assuming there is only one item of each type in the slot.
     * Notice: A \0 terminator is added to the returned item for
     *         easier string processing. This is not included in returned
     *         length, but there must be space for it in the buffer.
     *
     * @param type [in] Type of requested item.
     * @param buffer [out] Buffer to store item in.
     * @param length [in, out] Pointer to length of buffer. Set to actual item
     * length on return..
     *
     * @return ATCA_STATUS error code. If the item is not found ATCA_INVALID_ID
     * will be returned.
     */
    ATCA_STATUS
    readProvisionItem(const enum EccDataType type,
                      uint8_t* buffer,
                      size_t* size);

    /**
     * @brief Get the size of the root certificate (not base64 encoded. The size
     * is in raw bytes).
     *
     * @param size [out] The size will be placed in this pointer.
     *
     * @return The #ATCACERT_x defines. @see atcacert.h.
     */
    int getRootCertificateSize(size_t* size);

    /**
     * @brief Retrieve the root certificate. Note that the certificate format is
     * not base64 encoded.
     *
     * @param certificate [out] Buffer where the certificate will be placed.
     * @param size [in, out] Size of buffer as input. Will be overwritten with
     * the size of the certificate.
     *
     * @return The #ATCACERT_x defines. @see atcacert.h.
     */
    int getRootCertificate(uint8_t* certificate, size_t* size);

    /**
     * @brief Get the size of the signer certificate (not base64 encoded. The
     * size is in raw bytes).
     *
     * @param size [out] The size will be placed in this pointer.
     *
     * @return The #ATCACERT_x defines. @see atcacert.h.
     */
    int getSignerCertificateSize(size_t* size);

    /**
     * @brief Retrieve the signer certificate. Note that the certificate format
     * is not base64 encoded.
     *
     * @param certificate [out] Buffer where the certificate will be placed.
     * @param size [in, out] Size of buffer as input. Will be overwritten with
     * the size of the certificate.
     *
     * @return The #ATCACERT_x defines. @see atcacert.h.
     */
    int getSignerCertificate(uint8_t* certificate, size_t* size);

    /**
     * @brief Get the size of the device certificate (not base64 encoded. The
     * size is in raw bytes).
     *
     * @param size [out] The size will be placed in this pointer.
     *
     * @return The #ATCACERT_x defines. @see atcacert.h.
     */
    int getDeviceCertificateSize(size_t* size);

    /**
     * @brief Retrieve the device certificate. Note that the certificate format
     * is not base64 encoded.
     *
     * @param certificate [out] Buffer where the certificate will be placed.
     * @param size [in, out] Size of buffer as input. Will be overwritten with
     * the size of the certificate.
     *
     * @return The #ATCACERT_x defines. @see atcacert.h.
     */
    int getDeviceCertificate(uint8_t* certificate, size_t* size);

    /**
     * @brief Calculates the buffer size needed for encoding a certificate in
     * raw bytes to base64.
     *
     * @param certificate_size [in] The certificate size in raw bytes.
     *
     * @return The size needed for base64 encoding.
     */
    size_t calculateBase64EncodedCertificateSize(const size_t certificate_size);

    /**
     * @brief Base64 encodes a certificate in raw byte format.
     *
     * @param certificate [in] The certificate in raw bytes.
     * @param certificate_size [in] Size of the certificate in raw bytes.
     * @param base64_encoded_certificate [out] The base64 encoded certificate
     * will be placed in this buffer.
     * @param base64_encoded_certificate_size [in, out] Size of @p
     * base64_encoded_certificate, will be overwritten with the final size of
     * the encoded certificate.
     *
     * @return The enumerations of cryptoauthlib's #ATCA_STATUS. #ATCA_SUCCESS
     * on success.
     */
    ATCA_STATUS
    base64EncodeCertificate(const uint8_t* certificate,
                            const size_t certificate_size,
                            char* base64_encoded_certificate,
                            size_t* base64_encoded_certificate_size);
};

extern ECC608Class ECC608;

#endif

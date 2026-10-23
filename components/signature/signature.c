#include "signature.h"
#include "esp_err.h"
#include "soc/soc_caps.h"

#if SOC_ECDSA_SUPPORTED

#include <inttypes.h>
#include "esp_efuse.h"
#include "esp_log.h"
#include "psa/crypto.h"
#include "psa_crypto_driver_esp_ecdsa.h"
#include "psa_crypto_driver_esp_ecdsa_contexts.h"

static char const TAG[] = "signature";

#define SIGNATURE_KEY_BITS        256
#define SIGNATURE_KEY_BYTES       (SIGNATURE_KEY_BITS / 8)
#define SIGNATURE_LENGTH          (2 * SIGNATURE_KEY_BYTES)
#define SIGNATURE_PUBLIC_KEY_SIZE (1 + 2 * SIGNATURE_KEY_BYTES)
#define SIGNATURE_ALGORITHM       PSA_ALG_ECDSA(PSA_ALG_SHA_256)

// Import a reference to the hardware ECDSA key stored in eFuse block KEY0 as an opaque PSA key
static esp_err_t import_key(psa_key_id_t* key_id) {
    esp_ecdsa_opaque_key_t opaque_key = {
        .curve       = ESP_ECDSA_CURVE_SECP256R1,
        .efuse_block = EFUSE_BLK_KEY0,
    };

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attributes, SIGNATURE_KEY_BITS);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_HASH);
    psa_set_key_algorithm(&attributes, SIGNATURE_ALGORITHM);
    psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_ESP_ECDSA_VOLATILE);

    psa_status_t status = psa_import_key(&attributes, (uint8_t*)&opaque_key, sizeof(opaque_key), key_id);
    psa_reset_key_attributes(&attributes);

    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "Error importing key: %" PRId32, (int32_t)status);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t signature_sign(uint8_t* data, size_t data_length, uint8_t* signature) {
    // Signature buffer must be 64 bytes long
    if (data == NULL || signature == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t hash[PSA_HASH_LENGTH(PSA_ALG_SHA_256)];
    size_t  hash_length = 0;

    psa_status_t status = psa_hash_compute(PSA_ALG_SHA_256, data, data_length, hash, sizeof(hash), &hash_length);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "Error hashing data: %" PRId32, (int32_t)status);
        return ESP_FAIL;
    }

    psa_key_id_t key_id = 0;
    esp_err_t    res    = import_key(&key_id);
    if (res != ESP_OK) {
        return res;
    }

    size_t signature_length = 0;
    status =
        psa_sign_hash(key_id, SIGNATURE_ALGORITHM, hash, hash_length, signature, SIGNATURE_LENGTH, &signature_length);
    psa_destroy_key(key_id);

    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "Error signing: %" PRId32, (int32_t)status);
        return ESP_FAIL;
    }

    if (signature_length != SIGNATURE_LENGTH) {
        ESP_LOGE(TAG, "Invalid signature length");
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

esp_err_t signature_read_public_key(uint8_t* public_key) {
    // Public key buffer must be 65 bytes long
    if (public_key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    psa_key_id_t key_id = 0;
    esp_err_t    res    = import_key(&key_id);
    if (res != ESP_OK) {
        return res;
    }

    size_t       public_key_length = 0;
    psa_status_t status = psa_export_public_key(key_id, public_key, SIGNATURE_PUBLIC_KEY_SIZE, &public_key_length);
    psa_destroy_key(key_id);

    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "Error exporting public key: %" PRId32, (int32_t)status);
        return ESP_FAIL;
    }

    if (public_key_length != SIGNATURE_PUBLIC_KEY_SIZE) {
        ESP_LOGE(TAG, "Invalid public key length");
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

#else

esp_err_t signature_sign(uint8_t* data, size_t data_length, uint8_t* signature) {
    (void)data;
    (void)data_length;
    (void)signature;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t signature_read_public_key(uint8_t* public_key) {
    (void)public_key;
    return ESP_ERR_NOT_SUPPORTED;
}

#endif  // SOC_ECDSA_SUPPORTED

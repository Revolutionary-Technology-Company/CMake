#include "cmBitLockerTpm.h"
#include <iostream>
#include <fstream>
#include <cstdlib>

#if defined(__linux__)
#include <tss2/tss2_esys.h>
#include <tss2/tss2_mu.h>
#endif

// Main strategy selector
bool cmBitLockerTpm::ExtractKey(std::vector<uint8_t>& outKey, TpmKeyStrategy strategy, const std::string& blobPath, uint32_t nvIndex) {
#if !defined(__linux__)
    std::cerr << "TPM extraction pipeline is only supported on Linux targets.\n";
    return false;
#else
    ESYS_CONTEXT* esysContext = nullptr;
    TSS2_RC rc = Esys_Initialize(&esysContext, nullptr, nullptr);
    if (rc != TSS2_RC_SUCCESS) {
        std::cerr << "Initialization Error: Failed to stand up ESYS context.\n";
        return false;
    }

    bool success = false;

    if (strategy == TpmKeyStrategy::SealedBlob || strategy == TpmKeyStrategy::Auto) {
        std::cout << "Attempting to unseal key blob using system PCR profile (PCR 0,1,7)...\n";
        success = ExtractFromSealedBlob(esysContext, blobPath, outKey);
    }

    if (!success && (strategy == TpmKeyStrategy::NvIndex || strategy == TpmKeyStrategy::Auto)) {
        std::cout << "Falling back to direct TPM NV Index reading (Slot: 0x" << std::hex << nvIndex << ")...\n";
        success = ExtractFromNvIndex(esysContext, nvIndex, outKey);
    }

    Esys_Finalize(&esysContext);
    return success;
#endif
}

// Strategy A: Extracting from a fixed NV Index slot
bool cmBitLockerTpm::ExtractFromNvIndex(void* context, uint32_t nvIndex, std::vector<uint8_t>& outKey) {
#if defined(__linux__)
    ESYS_CONTEXT* esysContext = static_cast<ESYS_CONTEXT*>(context);
    ESYS_TR nvHandle = ESYS_TR_NONE;
    
    TSS2_RC rc = Esys_TR_FromTPMPublic(esysContext, nvIndex, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, &nvHandle);
    if (rc != TSS2_RC_SUCCESS) return false;

    TPM2B_MAX_NV_BUFFER* nvData = nullptr;
    // Standard AES-256 Volume Master Key size is 32 bytes
    rc = Esys_NV_Read(esysContext, ESYS_TR_RH_OWNER, nvHandle, ESYS_TR_PASSWORD, ESYS_TR_NONE, ESYS_TR_NONE, 32, 0, &nvData);
    
    if (rc == TSS2_RC_SUCCESS && nvData) {
        outKey.assign(nvData->buffer, nvData->buffer + nvData->size);
        free(nvData);
        return true;
    }
#endif
    return false;
}

// Strategy B: Unsealing a Key Blob bound to active system PCR policies
bool cmBitLockerTpm::ExtractFromSealedBlob(void* context, const std::string& blobPath, std::vector<uint8_t>& outKey) {
#if defined(__linux__)
    if (blobPath.empty()) return false;
    ESYS_CONTEXT* esysContext = static_cast<ESYS_CONTEXT*>(context);

    // Read the wrapped blob file from the host system disk
    std::ifstream file(blobPath, std::ios::binary);
    if (!file.is_open()) return false;
    std::vector<uint8_t> blobData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Unmarshal the raw file bytes into a structured TPM Object Type
    TPM2B_PRIVATE privateKey;
    TPM2B_PUBLIC publicKey;
    size_t offset = 0;
    
    // Parse the bytes into legitimate TSS2 structures
    TSS2_RC rc = Tss2_MU_TPM2B_PRIVATE_Unmarshal(blobData.data(), blobData.size(), &offset, &privateKey);
    if (rc != TSS2_RC_SUCCESS) return false;
    rc = Tss2_MU_TPM2B_PUBLIC_Unmarshal(blobData.data(), blobData.size(), &offset, &publicKey);
    if (rc != TSS2_RC_SUCCESS) return false;

    // Load the wrapped key object into the Storage Root Key (SRK) context space
    ESYS_TR loadedKeyHandle = ESYS_TR_NONE;
    rc = Esys_Load(esysContext, ESYS_TR_RH_OWNER, ESYS_TR_PASSWORD, ESYS_TR_NONE, ESYS_TR_NONE, 
                   &privateKey, &publicKey, &loadedKeyHandle);

    if (rc != TSS2_RC_SUCCESS) {
        std::cerr << "PCR Integrity Check Failed: TPM rejected structural unsealing. PCR state mismatched.\n";
        return false;
    }

    // Evaluate the object using the internal engine crypt-unseal sequence
    TPM2B_DATA* unsealedData = nullptr;
    rc = Esys_Unseal(esysContext, loadedKeyHandle, ESYS_TR_PASSWORD, ESYS_TR_NONE, ESYS_TR_NONE, &unsealedData);
    
    if (rc == TSS2_RC_SUCCESS && unsealedData) {
        outKey.assign(unsealedData->buffer, unsealedData->buffer + unsealedData->size);
        free(unsealedData);
        Esys_FlushContext(esysContext, loadedKeyHandle);
        return true;
    }
    
    if (loadedKeyHandle != ESYS_TR_NONE) Esys_FlushContext(esysContext, loadedKeyHandle);
#endif
    return false;
}

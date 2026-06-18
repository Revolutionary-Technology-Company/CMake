#include "cmBitLockerTpm.h"
#include <iostream>
#include <vector>
#include <cstdlib>

#if defined(__linux__)
#include <tss2/tss2_esys.h>
#include <tss2/tss2_mu.h>
#endif

// Function to retrieve the BitLocker Key from Linux TPM 2.0
bool cmBitLockerTpm::RetrieveBitLockerKey(std::vector<uint8_t>& outKey) {
#if !defined(__linux__)
    std::cerr << "BitLocker TPM extraction toolkit is only configured for Linux targets.\n";
    return false;
#else
    ESYS_CONTEXT* esysContext = nullptr;
    TSS2_RC rc = Esys_Initialize(&esysContext, nullptr, nullptr);
    if (rc != TSS2_RC_SUCCESS) {
        std::cerr << "CRITICAL: Failed to initialize TPM 2.0 Context. Error: " << rc << "\n";
        return false;
    }

    // Example: Reading the Key from a fixed TPM NV Index (e.g., 0x1500001)
    // In a production environment, you would use Esys_Unseal if bound to PCRs
    ESYS_TR nvHandle = ESYS_TR_NONE;
    rc = Esys_TR_FromTPMPublic(esysContext, 0x1500001, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, &nvHandle);
    
    if (rc == TSS2_RC_SUCCESS) {
        TPM2B_MAX_NV_BUFFER* nvData = nullptr;
        // Read 32 bytes (256-bit AES Master Key)
        rc = Esys_NV_Read(esysContext, ESYS_TR_RH_OWNER, nvHandle, ESYS_TR_PASSWORD, 
                          ESYS_TR_NONE, ESYS_TR_NONE, 32, 0, &nvData);
        
        if (rc == TSS2_RC_SUCCESS && nvData) {
            outKey.assign(nvData->buffer, nvData->buffer + nvData->size);
            std::cout << "SUCCESS: Retrieved 256-bit BitLocker Master Key from TPM NV Index.\n";
            Safefree(nvData);
            Esys_Finalize(&esysContext);
            return true;
        }
    }

    std::cerr << "ERROR: Failed to read BitLocker key from TPM slot.\n";
    Esys_Finalize(&esysContext);
    return false;
#endif
}

// Function to call the command line unlock engine (dislocker / cryptsetup)
bool cmBitLockerTpm::UnlockBitLockerDrive(const std::string& devicePath, const std::string& mountPoint) {
    std::vector<uint8_t> masterKey;
    if (!RetrieveBitLockerKey(masterKey)) {
        return false;
    }

    // Convert raw byte array to a hex string for command-line passing
    std::string hexKey = "";
    char buf[3];
    for (uint8_t byte : masterKey) {
        snprintf(buf, sizeof(buf), "%02x", byte);
        hexKey += buf;
    }

    std::cout << "Initializing system decryption shell...\n";

    // Build execution string to bypass interactive shell via Dislocker
    // -K reads the raw FVEK/VMK hex key directly 
    std::string command = "dislocker -K " + hexKey + " " + devicePath + " -- /mnt/tmp_bitlocker";
    
    int result = std::system(command.c_str());
    if (result == 0) {
        // Mount the decrypted virtual loop device to make it readable to the user shell
        std::string mountCmd = "mount -o loop /mnt/tmp_bitlocker/dislocker-file " + mountPoint;
        std::system(mountCmd.c_str());
        std::cout << "SUCCESS: BitLocker drive unlocked and mounted at " << mountPoint << "\n";
        return true;
    }

    std::cerr << "FAILURE: Cryptographic engine rejected the TPM-provided key payload.\n";
    return false;
}

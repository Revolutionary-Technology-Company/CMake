#ifndef cmBitLockerTpm_h
#define cmBitLockerTpm_h

#include <string>
#include <vector>
#include <cstdint>

enum class TpmKeyStrategy {
    Auto,       // Try Sealed Blob first, fallback to NV Index
    SealedBlob, // Read a file containing a TPM-wrapped blob bound to PCRs
    NvIndex     // Read directly from a raw TPM NV memory slot
};

class cmBitLockerTpm {
public:
    static bool ExtractKey(std::vector<uint8_t>& outKey, TpmKeyStrategy strategy, const std::string& blobPath = "", uint32_t nvIndex = 0x1500001);
    static bool UnlockDrive(const std::string& devicePath, const std::string& mountPoint, TpmKeyStrategy strategy, const std::string& blobPath = "", uint32_t nvIndex = 0x1500001);

private:
    static bool ExtractFromNvIndex(void* esysContext, uint32_t nvIndex, std::vector<uint8_t>& outKey);
    static bool ExtractFromSealedBlob(void* esysContext, const std::string& blobPath, std::vector<uint8_t>& outKey);
};

#endif

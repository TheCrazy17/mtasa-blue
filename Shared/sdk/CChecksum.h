/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        sdk/CChecksum.h
 *  PURPOSE:     Checksum class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/
// Note: Cannot use #pragma once here, due to a duplicate existing in publicsdk
#ifndef __CChecksum_H
#define __CChecksum_H

#include <variant>
#include <thread>
#include <chrono>
#include <algorithm>
#include <memory>
#include "SharedUtil.Hash.h"
#include "SharedUtil.File.h"
#include "SString.h"
#include <bochs_internal/bochs_crc32.h>

#if defined(_WIN32) && defined(MTA_CLIENT)
    #include <unordered_map>
    #include <mutex>
#endif

// Depends on CMD5Hasher and CRCGenerator
class CChecksum
{
public:
    CChecksum() : ulCRC(0), md5{} {}

    // Comparison operators
    bool operator==(const CChecksum& other) const { return ulCRC == other.ulCRC && memcmp(md5.data, other.md5.data, sizeof(md5.data)) == 0; }

    bool operator!=(const CChecksum& other) const { return !operator==(other); }

    // Incremental CRC32 and MD5 over data fed one chunk at a time
    class CHasher
    {
    public:
        CHasher() { m_md5Hasher.Init(); }

        void Update(const void* pData, std::size_t size)
        {
            m_ulCRC = CRCGenerator::GetCRCFromBuffer(static_cast<const char*>(pData), size, m_ulCRC);
            m_md5Hasher.Update(static_cast<const unsigned char*>(pData), static_cast<unsigned int>(size));
        }

        CChecksum Finalize()
        {
            m_md5Hasher.Finalize();
            CChecksum result;
            result.ulCRC = m_ulCRC;
            memcpy(result.md5.data, m_md5Hasher.GetResult(), sizeof(result.md5.data));
            return result;
        }

    private:
        CMD5Hasher    m_md5Hasher;
        unsigned long m_ulCRC = 0;
    };

#if defined(_WIN32) && defined(MTA_CLIENT)
private:
    struct CacheEntry
    {
        std::uint64_t size, mtime;
        unsigned long crc;
        MD5           md5;
    };
    static std::unordered_map<std::string, CacheEntry>& Cache()
    {
        static std::unordered_map<std::string, CacheEntry> c;
        return c;
    }
    static std::mutex& CacheMtx()
    {
        static std::mutex m;
        return m;
    }

    // Read budget scaled to the file size at a conservative 20MB/s, so a huge .img is not held to a script's deadline
    static DWORD ScaledReadTimeoutMs(std::uint64_t sizeBytes)
    {
        constexpr std::uint64_t bytesPerMs = 20 * 1024;
        return static_cast<DWORD>(std::clamp<std::uint64_t>(sizeBytes / bytesPerMs, 2000, 60000));
    }

public:
    static void ClearChecksumCache()
    {
        std::lock_guard<std::mutex> l(CacheMtx());
        Cache().clear();
    }

    static std::variant<CChecksum, std::string> GenerateChecksumFromFile(const SString& strFilename)
    {
        std::string key = strFilename;
        for (char& c : key)
        {
            if (c >= 'A' && c <= 'Z')
                c += 32;
            if (c == '\\')
                c = '/';
        }

        WIN32_FILE_ATTRIBUTE_DATA attr;
        WString                   wide;
        try
        {
            wide = SharedUtil::FromUTF8(strFilename);
        }
        catch (...)
        {
        }
        bool          hasMeta = !wide.empty() && SharedUtil::GetFileAttributesExWithTimeout(wide.c_str(), attr, 500);
        std::uint64_t sz = 0, mt = 0;
        if (hasMeta)
        {
            sz = (std::uint64_t(attr.nFileSizeHigh) << 32) | attr.nFileSizeLow;
            mt = (std::uint64_t(attr.ftLastWriteTime.dwHighDateTime) << 32) | attr.ftLastWriteTime.dwLowDateTime;
            std::lock_guard<std::mutex> l(CacheMtx());
            auto                        it = Cache().find(key);
            if (it != Cache().end() && it->second.size == sz && it->second.mtime == mt)
            {
                CChecksum cached;
                cached.ulCRC = it->second.crc;
                cached.md5 = it->second.md5;
                return cached;
            }
        }

        // Hash chunk by chunk so the file is never one whole allocation; the hasher is shared with the
        // reader's worker thread, which can outlive this call after a timeout
        auto                      hasher = std::make_shared<CHasher>();
        const auto                onChunk = [hasher](const char* pData, std::size_t size) { hasher->Update(pData, size); };
        SharedUtil::SFileIdentity identity;

        if (!SharedUtil::FileReadChunkedWithTimeout(strFilename, ScaledReadTimeoutMs(sz), onChunk, identity))
        {
            if (!hasMeta)
                return SString("File not found or inaccessible: %s", strFilename.c_str());
            return SString("Could not read: %s", strFilename.c_str());
        }

        CChecksum r = hasher->Finalize();

        // identity came from the handle that was hashed, so it and the checksum describe the same moment
        {
            std::lock_guard<std::mutex> l(CacheMtx());
            Cache()[key] = {identity.size, identity.mtime, r.ulCRC, r.md5};
        }
        return r;
    }
#else
    static void ClearChecksumCache() {}

    // Server and non-Windows builds use the original implementation
    static std::variant<CChecksum, std::string> GenerateChecksumFromFile(const SString& strFilename)
    {
        constexpr int maxRetries = 3;
        constexpr int retryDelayMs = 50;
        int           lastErrno = 0;
        int           attemptsMade = 0;

        for (int attempt = 0; attempt < maxRetries; ++attempt)
        {
            ++attemptsMade;

            if (attempt > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs * attempt));

            errno = 0;

            CChecksum result;
            result.ulCRC = CRCGenerator::GetCRCFromFile(strFilename);

            if (errno)
            {
                lastErrno = errno;
                if (errno == ENOENT)
                    break;
                continue;
            }

            errno = 0;
            bool success = CMD5Hasher().Calculate(strFilename, result.md5);

            if (!success)
            {
                lastErrno = errno ? errno : EIO;
                if (errno == ENOENT)
                    break;
                continue;
            }

            return result;
        }

        if (lastErrno == ENOENT)
            return SString("File not found: %s", strFilename.c_str());

        return SString("Could not checksum '%s' after %d attempt%s: %s", strFilename.c_str(), attemptsMade, attemptsMade == 1 ? "" : "s",
                       lastErrno ? std::strerror(lastErrno) : "Unknown error");
    }
#endif

    // GenerateChecksumFromFileUnsafe should never ever be used unless you are a bad person. Or unless you really know what you're doing.
    // If it's the latter, please leave a code comment somewhere explaining why. Otherwise we'll think it's just code that hasn't been migrated yet.
    static CChecksum GenerateChecksumFromFileUnsafe(const SString& strFilename)
    {
        auto result = GenerateChecksumFromFile(strFilename);

        // If it holds an error message, just return a default CChecksum
        if (std::holds_alternative<std::string>(result))
            return CChecksum();

        return std::get<CChecksum>(result);
    }

    static CChecksum GenerateChecksumFromBuffer(const char* cpBuffer, unsigned long ulLength)
    {
        CChecksum result;
        result.ulCRC = CRCGenerator::GetCRCFromBuffer(cpBuffer, ulLength);
        CMD5Hasher().Calculate(cpBuffer, ulLength, result.md5);
        return result;
    }

    unsigned long ulCRC;
    MD5           md5;
};

#endif

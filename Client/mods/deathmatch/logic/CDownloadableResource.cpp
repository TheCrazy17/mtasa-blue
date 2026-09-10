/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/deathmatch/logic/CDownloadableResource.cpp
 *  PURPOSE:     Downloadable resource interface
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include <StdInc.h>

static bool    s_bChecksumBatchActive = false;
static int64_t s_checksumBatchAccumMs = 0;

void CDownloadableResource::BeginChecksumBatch()
{
    if (!s_bChecksumBatchActive)
    {
        s_bChecksumBatchActive = true;
        s_checksumBatchAccumMs = 0;
    }
}

void CDownloadableResource::EndChecksumBatch()
{
    s_bChecksumBatchActive = false;
    s_checksumBatchAccumMs = 0;
}

CDownloadableResource::CDownloadableResource(CResource* pResource, eResourceType resourceType, const char* szName, const char* szNameShort, uint uiDownloadSize,
                                             CChecksum serverChecksum, bool bAutoDownload)
{
    m_pResource = pResource;
    m_resourceType = resourceType;
    m_strName = szName;
    m_strNameShort = szNameShort;
    m_ServerChecksum = serverChecksum;
    m_bAutoDownload = bAutoDownload;
    m_bInDownloadQueue = false;
    m_bDownloaded = false;
    m_uiDownloadSize = uiDownloadSize;
    m_uiHttpServerIndex = 0;
    m_bModifedByScript = false;
    m_bClientChecksumVerified = false;

    GenerateClientChecksum();
    g_pClientGame->GetResourceManager()->OnAddResourceFile(this);
}

CDownloadableResource::~CDownloadableResource()
{
    g_pClientGame->GetResourceFileDownloadManager()->OnRemoveResourceFile(this);
    g_pClientGame->GetResourceManager()->OnRemoveResourceFile(this);
}

bool CDownloadableResource::DoesClientAndServerChecksumMatch()
{
    return !m_bClientChecksumVerified || (m_LastClientChecksum == m_ServerChecksum);
}

CChecksum CDownloadableResource::GenerateClientChecksum()
{
    constexpr int64_t BATCH_BUDGET_MS = 5000;

    if (s_bChecksumBatchActive && s_checksumBatchAccumMs >= BATCH_BUDGET_MS)
    {
        m_LastClientChecksum = CChecksum();
        m_bClientChecksumVerified = false;
        return m_LastClientChecksum;
    }

    long long startMs = GetTickCount64_();

    // A failed read must not become a "verified" zero checksum that reads as a mismatch, and a read
    // that landed mid rewrite of this file is retried rather than trusted
    auto checksumOrError = CChecksum::GenerateChecksumFromFileWithRetry(m_strName, m_uiDownloadSize);
    if (std::holds_alternative<CChecksum>(checksumOrError))
    {
        m_LastClientChecksum = std::get<CChecksum>(checksumOrError);
        m_bClientChecksumVerified = true;
    }
    else
    {
        m_LastClientChecksum = CChecksum();
        m_bClientChecksumVerified = false;
    }

    if (s_bChecksumBatchActive)
        s_checksumBatchAccumMs += GetTickCount64_() - startMs;

    return m_LastClientChecksum;
}

CChecksum CDownloadableResource::GenerateClientChecksum(CBuffer& outFileData)
{
    if (outFileData.LoadFromFile(m_strName))
        m_LastClientChecksum = CChecksum::GenerateChecksumFromBuffer(outFileData.GetData(), outFileData.GetSize());
    else
        m_LastClientChecksum = CChecksum();

    m_bClientChecksumVerified = true;

    return m_LastClientChecksum;
}

CChecksum CDownloadableResource::GetLastClientChecksum()
{
    return m_LastClientChecksum;
}

CChecksum CDownloadableResource::GetServerChecksum()
{
    return m_ServerChecksum;
}

int CDownloadableResource::GetDownloadPriorityGroup()
{
    return m_pResource->GetDownloadPriorityGroup();
}

/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CIplStore.h
 *  PURPOSE:     IPL store class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include "CIplSA.h"
#include "CPoolsSA.h"
#include <game/CIplStore.h>
#include <functional>
#include <unordered_map>

class CIplStoreSA : public CIplStore
{
public:
    CIplStoreSA();
    ~CIplStoreSA() = default;

    void SetDynamicIplStreamingEnabled(bool state);
    void SetDynamicIplStreamingEnabled(bool state, std::function<bool(CIplSAInterface* ipl)> filter);

private:
    void UnloadAndDisableStreaming(int iplId);
    void EnableStreaming(int iplId);

private:
    CPoolSAInterface<CIplSAInterface>** m_ppIplPoolInterface;

    bool m_isStreamingEnabled;

    // Remembers which IPLs actually had streaming enabled right before we forced everything
    // off, so re-enabling can bring back only those and leave IPLs that are normally always
    // disabled (like cut content such as carter.ipl and crack.ipl) exactly as they were.
    std::unordered_map<int, bool> m_streamingDisabledBackup;
};

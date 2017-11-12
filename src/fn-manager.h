// Copyright (c) 2014-2015 The Bitsend developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef FUNDAMENTALNODEMAN_H
#define FUNDAMENTALNODEMAN_H

#include "bignum.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "core.h"
#include "util.h"
#include "script.h"
#include "base58.h"
#include "main.h"
#include "fundamentalnode.h"

#define FUNDAMENTALNODES_DUMP_SECONDS               (15*60)
#define FUNDAMENTALNODES_DSEG_SECONDS               (3*60*60)

using namespace std;

class CFundamentalnodeMan;

extern CFundamentalnodeMan fnmanager;
void DumpFundamentalnodes();

/** Access to the FN database
 */
class CFundamentalnodeDB
{
private:
    boost::filesystem::path pathFN;
    std::string strMagicMessage;
public:
    enum ReadResult {
        Ok,
        FileError,
        HashReadError,
        IncorrectHash,
        IncorrectMagicMessage,
        IncorrectMagicNumber,
        IncorrectFormat
    };

    CFundamentalnodeDB();
    bool Write(const CFundamentalnodeMan &fnmanagerToSave);
    ReadResult Read(CFundamentalnodeMan& fnmanagerToLoad);
};

class CFundamentalnodeMan
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    // map to hold all MNs
    std::vector<CFundamentalnode> vFundamentalnodes;
    // who's asked for the Fundamentalnode list and the last time
    std::map<CNetAddr, int64_t> mAskedUsForFundamentalnodeList;
    // who we asked for the Fundamentalnode list and the last time
    std::map<CNetAddr, int64_t> mWeAskedForFundamentalnodeList;
    // which Fundamentalnodes we've asked for
    std::map<COutPoint, int64_t> mWeAskedForFundamentalnodeListEntry;

public:
    // keep track of dsq count to prevent fundamentalnodes from gaming darksend queue
    int64_t nDsqCount;

    IMPLEMENT_SERIALIZE
    (
        // serialized format:
        // * version byte (currently 0)
        // * fundamentalnodes vector
        {
                LOCK(cs);
                unsigned char nVersion = 0;
                READWRITE(nVersion);
                READWRITE(vFundamentalnodes);
                READWRITE(mAskedUsForFundamentalnodeList);
                READWRITE(mWeAskedForFundamentalnodeList);
                READWRITE(mWeAskedForFundamentalnodeListEntry);
                READWRITE(nDsqCount);
        }
    )

    CFundamentalnodeMan();
    CFundamentalnodeMan(CFundamentalnodeMan& other);

    /// Add an entry
    bool Add(CFundamentalnode &fn);

    /// Check all Fundamentalnodes
    void Check();

    /// Check all Fundamentalnodes and remove inactive
    void CheckAndRemove();

    /// Clear Fundamentalnode vector
    void Clear();

    int CountEnabled();

    int CountFundamentalnodesAboveProtocol(int protocolVersion);

    void FnlUpdate(CNode* pnode);

    /// Find an entry
    CFundamentalnode* Find(const CTxIn& vin);
    CFundamentalnode* Find(const CPubKey& pubKeyFundamentalnode);

    /// Find an entry thta do not match every entry provided vector
    CFundamentalnode* FindOldestNotInVec(const std::vector<CTxIn> &vVins, int nMinimumAge, int nMinimumActiveSeconds);

    /// Find a random entry
    CFundamentalnode* FindRandom();

    /// Get the current winner for this block
    CFundamentalnode* GetCurrentFundamentalNode(int mod=1, int64_t nBlockHeight=0, int minProtocol=0);

    std::vector<CFundamentalnode> GetFullFundamentalnodeVector() { Check(); return vFundamentalnodes; }

    std::vector<pair<int, CFundamentalnode> > GetFundamentalnodeRanks(int64_t nBlockHeight, int minProtocol=0);
    int GetFundamentalnodeRank(const CTxIn &vin, int64_t nBlockHeight, int minProtocol=0, bool fOnlyActive=true);
    CFundamentalnode* GetFundamentalnodeByRank(int nRank, int64_t nBlockHeight, int minProtocol=0, bool fOnlyActive=true);

    void ProcessFundamentalnodeConnections();
    
    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    /// Return the number of (unique) Fundamentalnodes
    int size() { return vFundamentalnodes.size(); }

    std::string ToString() const;

    //
    // Relay Fundamentalnode Messages
    //

    void RelayFundamentalnodeEntry(const CTxIn vin, const CService addr, const std::vector<unsigned char> vchSig, const int64_t nNow, const CPubKey pubkey, const CPubKey pubkey2, const int count, const int current, const int64_t lastUpdated, const int protocolVersion, CScript donationAddress, int donationPercentage);
    void RelayFundamentalnodeEntryPing(const CTxIn vin, const std::vector<unsigned char> vchSig, const int64_t nNow, const bool stop);

    void Remove(CTxIn vin);
};

#endif


// Copyright (c) 2014-2015 The Bitsend developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef FUNDAMENTALNODE_H
#define FUNDAMENTALNODE_H

#include "bignum.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "core.h"
#include "util.h"
#include "script.h"
#include "base58.h"
#include "main.h"
#include "fn-service.h"

#define FUNDAMENTALNODE_NOT_PROCESSED               0 // initial state
#define FUNDAMENTALNODE_IS_CAPABLE                  1
#define FUNDAMENTALNODE_NOT_CAPABLE                 2
#define FUNDAMENTALNODE_STOPPED                     3
#define FUNDAMENTALNODE_INPUT_TOO_NEW               4
#define FUNDAMENTALNODE_PORT_NOT_OPEN               6
#define FUNDAMENTALNODE_PORT_OPEN                   7
#define FUNDAMENTALNODE_SYNC_IN_PROCESS             8
#define FUNDAMENTALNODE_REMOTELY_ENABLED            9

#define FUNDAMENTALNODE_MIN_CONFIRMATIONS           15
#define FUNDAMENTALNODE_MIN_FNEP_SECONDS           (30*60)
#define FUNDAMENTALNODE_MIN_FNE_SECONDS            (10*60)  // bitsenddev 12-05 Old 5*60
#define FUNDAMENTALNODE_PING_SECONDS                (5*60)   // bitsenddev 12-05 OLD 1*60
#define FUNDAMENTALNODE_EXPIRATION_SECONDS          (65*60)
#define FUNDAMENTALNODE_REMOVAL_SECONDS             (70*60)

using namespace std;

class CFundamentalnode;
class CFundamentalnodePayments;
class CFundamentalnodePaymentWinner;

extern CFundamentalnodePayments fundamentalnodePayments;
extern map<uint256, CFundamentalnodePaymentWinner> mapSeenFundamentalnodeVotes;
extern map<int64_t, uint256> mapCacheBlockHashes;

void ProcessMessageFundamentalnodePayments(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
bool GetBlockHash(uint256& hash, int nBlockHeight);

//
// The Fundamentalnode Class. For managing the Darksend process. It contains the input of the 1000DRK, signature to prove
// it's the one who own that ip address and code for calculating the payment election.
//
class CFundamentalnode
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

public:
    enum state {
        FUNDAMENTALNODE_ENABLED = 1,
        FUNDAMENTALNODE_EXPIRED = 2,
        FUNDAMENTALNODE_VIN_SPENT = 3,
        FUNDAMENTALNODE_REMOVE = 4,
        FUNDAMENTALNODE_POS_ERROR = 5
    };

    CTxIn vin;
    CService addr;
    CPubKey pubkey;
    CPubKey pubkey2;
    std::vector<unsigned char> sig;
    int activeState;
    int64_t sigTime;
    int64_t lastFnep;
    int64_t lastTimeSeen;
    int cacheInputAge;
    int cacheInputAgeBlock;
    bool unitTest;
    bool allowFreeTx;
    int protocolVersion;
    int64_t nLastFnq; //
    CScript donationAddress;
    int donationPercentage;
    int nVote;
    int64_t lastVote;
    int nScanningErrorCount;
    int nLastScanningErrorBlockHeight;

    CFundamentalnode();
    CFundamentalnode(const CFundamentalnode& other);
    CFundamentalnode(CService newAddr, CTxIn newVin, CPubKey newPubkey, std::vector<unsigned char> newSig, int64_t newSigTime, CPubKey newPubkey2, int protocolVersionIn, CScript donationAddress, int donationPercentage);

    void swap(CFundamentalnode& first, CFundamentalnode& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.vin, second.vin);
        swap(first.addr, second.addr);
        swap(first.pubkey, second.pubkey);
        swap(first.pubkey2, second.pubkey2);
        swap(first.sig, second.sig);
        swap(first.activeState, second.activeState);
        swap(first.sigTime, second.sigTime);
        swap(first.lastFnep, second.lastFnep);
        swap(first.lastTimeSeen, second.lastTimeSeen);
        swap(first.cacheInputAge, second.cacheInputAge);
        swap(first.cacheInputAgeBlock, second.cacheInputAgeBlock);
        swap(first.unitTest, second.unitTest);
        swap(first.allowFreeTx, second.allowFreeTx);
        swap(first.protocolVersion, second.protocolVersion);
        swap(first.nLastFnq, second.nLastFnq);
        swap(first.donationAddress, second.donationAddress);
        swap(first.donationPercentage, second.donationPercentage);
        swap(first.nVote, second.nVote);
        swap(first.lastVote, second.lastVote);
        swap(first.nScanningErrorCount, second.nScanningErrorCount);
        swap(first.nLastScanningErrorBlockHeight, second.nLastScanningErrorBlockHeight);
    }

    CFundamentalnode& operator=(CFundamentalnode from)
    {
        swap(*this, from);
        return *this;
    }
    friend bool operator==(const CFundamentalnode& a, const CFundamentalnode& b)
    {
        return a.vin == b.vin;
    }
    friend bool operator!=(const CFundamentalnode& a, const CFundamentalnode& b)
    {
        return !(a.vin == b.vin);
    }

    uint256 CalculateScore(int mod=1, int64_t nBlockHeight=0);

    IMPLEMENT_SERIALIZE
    (
        // serialized format:
        // * version byte (currently 0)
        // * all fields (?)
        {
                LOCK(cs);
                unsigned char nVersion = 0;
                READWRITE(nVersion);
                READWRITE(vin);
                READWRITE(addr);
                READWRITE(pubkey);
                READWRITE(pubkey2);
                READWRITE(sig);
                READWRITE(activeState);
                READWRITE(sigTime);
                READWRITE(lastFnep);
                READWRITE(lastTimeSeen);
                READWRITE(cacheInputAge);
                READWRITE(cacheInputAgeBlock);
                READWRITE(unitTest);
                READWRITE(allowFreeTx);
                READWRITE(protocolVersion);
                READWRITE(nLastFnq);
                READWRITE(donationAddress);
                READWRITE(donationPercentage);
                READWRITE(nVote);
                READWRITE(lastVote);
                READWRITE(nScanningErrorCount);
                READWRITE(nLastScanningErrorBlockHeight);
        }
    )


    void UpdateLastSeen(int64_t override=0)
    {
        if(override == 0){
            lastTimeSeen = GetAdjustedTime();
        } else {
            lastTimeSeen = override;
        }
    }

    inline uint64_t SliceHash(uint256& hash, int slice)
    {
        uint64_t n = 0;
        memcpy(&n, &hash+slice*64, 64);
        return n;
    }

    void Check();

    bool UpdatedWithin(int seconds)
    {
        // LogPrintf("UpdatedWithin %d, %d --  %d \n", GetAdjustedTime() , lastTimeSeen, (GetAdjustedTime() - lastTimeSeen) < seconds);

        return (GetAdjustedTime() - lastTimeSeen) < seconds;
    }

    void Disable()
    {
        lastTimeSeen = 0;
    }

    bool IsEnabled()
    {
        return activeState == FUNDAMENTALNODE_ENABLED;
    }

    int GetFundamentalnodeInputAge()
    {
        if(pindexBest == NULL) return 0;

        if(cacheInputAge == 0){
            cacheInputAge = GetInputAge(vin);
            cacheInputAgeBlock = pindexBest->nHeight;
        }

        return cacheInputAge+(pindexBest->nHeight-cacheInputAgeBlock);
    }

    void ApplyScanningError(CFundamentalnodeScanningError& mnse)
    {
        if(!mnse.IsValid()) return;

        if(mnse.nBlockHeight == nLastScanningErrorBlockHeight) return;
        nLastScanningErrorBlockHeight = mnse.nBlockHeight;

        if(mnse.nErrorType == SCANNING_SUCCESS){
            nScanningErrorCount--;
            if(nScanningErrorCount < 0) nScanningErrorCount = 0;
        } else { //all other codes are equally as bad
                    nScanningErrorCount++;
           /* Bitsenddev 04/08/2015 
                        if(nScanningErrorCount >= 4)
                        {
                        nScanningErrorCount = 0;
                        LogPrintf("S-Reset Bad Fundamentalnodescore \n"); //	Bitsenddev Set this for Debug
                        }
                        */
            if(nScanningErrorCount > FUNDAMENTALNODE_SCANNING_ERROR_THESHOLD*2) nScanningErrorCount = FUNDAMENTALNODE_SCANNING_ERROR_THESHOLD*2;
        }
    }

    std::string Status() {
        std::string strStatus = "ACTIVE";

        if(activeState == CFundamentalnode::FUNDAMENTALNODE_ENABLED) strStatus   = "ENABLED";
        if(activeState == CFundamentalnode::FUNDAMENTALNODE_EXPIRED) strStatus   = "EXPIRED";
        if(activeState == CFundamentalnode::FUNDAMENTALNODE_VIN_SPENT) strStatus = "VIN_SPENT";
        if(activeState == CFundamentalnode::FUNDAMENTALNODE_REMOVE) strStatus    = "REMOVE";
        if(activeState == CFundamentalnode::FUNDAMENTALNODE_POS_ERROR) strStatus = "POS_ERROR";

        return strStatus;
    }

};

// for storing the winning payments



class CFundamentalnodePaymentWinner
{
public:
    int nBlockHeight;
    CTxIn vin;
    CScript payee;
    std::vector<unsigned char> vchSig;
    uint64_t score;
	
		
    CFundamentalnodePaymentWinner() {
        nBlockHeight = 0;
        score = 0;
        vin = CTxIn();
        payee = CScript();
    }
	


    uint256 GetHash()
	{ 
	uint256 n2, n3; 
		n2 = Hash(BEGIN(nBlockHeight), END(nBlockHeight));
		n3 = vin.prevout.hash > n2 ? (vin.prevout.hash - n2) : (n2 - vin.prevout.hash);
		return n3;
    
    }

    IMPLEMENT_SERIALIZE(
        READWRITE(nBlockHeight);
        READWRITE(payee);
        READWRITE(vin);
        READWRITE(score);
        READWRITE(vchSig);
     )
};

//
// Fundamentalnode Payments Class
// Keeps track of who should get paid for which blocks
//

class CFundamentalnodePayments
{
private:
    std::vector<CFundamentalnodePaymentWinner> vWinning;
    int nSyncedFromPeer;
    std::string strMasterPrivKey;
    std::string strTestPubKey;
    std::string strMainPubKey;
    bool enabled;
    int nLastBlockHeight;

public:

    CFundamentalnodePayments() {
        
        // 100: G=0 101: MK just test
        strMainPubKey = "04351636759f760e78bdee87ab1c966b6a22e42601c21da396a7e6a5fc33787fd6bbbcf70f1bb5b1853352decc719cf9a37b55c9c1c4c48d4c9ff6998b2416137b";
        strTestPubKey = "04CBC82D432A42A05F9474A5554413A6166767C928DE669C40144DC585FB85F15E28035EADE398A6B8E38C24A001EAB50023124C4D8328C99EC2FDE47ED54B17BF";  // bitsenddev do not use 04-2015
        enabled = false;
    }

    bool SetPrivKey(std::string strPrivKey);
    bool CheckSignature(CFundamentalnodePaymentWinner& winner);
    bool Sign(CFundamentalnodePaymentWinner& winner);

    // Deterministically calculate a given "score" for a fundamentalnode depending on how close it's hash is
    // to the blockHeight. The further away they are the better, the furthest will win the election
    // and get paid this block
    //

    uint64_t CalculateScore(uint256 blockHash, CTxIn& vin);
    bool GetWinningFundamentalnode(int nBlockHeight, CTxIn& vinOut);
    bool AddWinningFundamentalnode(CFundamentalnodePaymentWinner& winner);
    bool ProcessBlock(int nBlockHeight);
    void Relay(CFundamentalnodePaymentWinner& winner);
    void Sync(CNode* node);
    void CleanPaymentList();
    int LastPayment(CFundamentalnode& mn);

    //slow
    bool GetBlockPayee(int nBlockHeight, CScript& payee);
};


#endif

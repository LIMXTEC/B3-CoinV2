


#include "bignum.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "util.h"
#include "script.h"
#include "base58.h"
#include "protocol.h"
#include "fn-activity.h"
#include "fn-manager.h"
#include "spork.h"
#include <boost/lexical_cast.hpp>
#include "fn-manager.h"

using namespace std;
using namespace boost;

std::map<uint256, CFundamentalnodeScanningError> mapFundamentalnodeScanningErrors;
CFundamentalnodeScanning fnscan;
CActiveFundamentalnode activeFundamentalnode;

/* 
    Fundamentalnode -  Service

    Here we follow Dash strictly.

    -- What it checks

    1.) Making sure Fundamentalnodes have their ports open
    2.) Are responding to requests made by the network


*/

void ProcessMessageFundamentalnodePOS(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if(fProMode) return;
    if(!IsSporkActive(SPORK_7_FUNDAMENTALNODE_SCANNING)) return;
    if(IsInitialBlockDownload()) return;

    if (strCommand == "fnse") //Fundamentalnode Scanning Error
    {

        CDataStream vMsg(vRecv);
        CFundamentalnodeScanningError fnse;
        vRecv >> fnse;

        CInv inv(MSG_FUNDAMENTALNODE_SCANNING_ERROR, fnse.GetHash());
        pfrom->AddInventoryKnown(inv);

        if(mapFundamentalnodeScanningErrors.count(fnse.GetHash())){
            return;
        }
        mapFundamentalnodeScanningErrors.insert(make_pair(fnse.GetHash(), fnse));

        if(!fnse.IsValid())
        {
            LogPrintf("FundamentalnodePOS::fnse - Invalid object\n");
            return;
        }

        CFundamentalnode* pfnA = fnmanager.Find(fnse.vinFundamentalnodeA);
        if(pfnA == NULL) return;
        if(pfnA->protocolVersion < MIN_FUNDAMENTALNODE_POS_PROTO_VERSION) return;

        int nBlockHeight = pindexBest->nHeight;
        if(nBlockHeight - fnse.nBlockHeight > 10){
            LogPrintf("FundamentalnodePOS::fnse - Too old\n");
            return;   
        }

        // Lowest fundamentalnodes in rank check the highest each block
        int a = fnmanager.GetFundamentalnodeRank(fnse.vinFundamentalnodeA, fnse.nBlockHeight, MIN_FUNDAMENTALNODE_POS_PROTO_VERSION);
        if(a == -1 || a > GetCountScanningPerBlock())
        {
            if(a != -1) LogPrintf("FundamentalnodePOS::fnse - FundamentalnodeA ranking is too high\n");
            return;
        }

        int b = fnmanager.GetFundamentalnodeRank(fnse.vinFundamentalnodeB, fnse.nBlockHeight, MIN_FUNDAMENTALNODE_POS_PROTO_VERSION, false);
        if(b == -1 || b < fnmanager.CountFundamentalnodesAboveProtocol(MIN_FUNDAMENTALNODE_POS_PROTO_VERSION)-GetCountScanningPerBlock())
        {
            if(b != -1) LogPrintf("FundamentalnodePOS::fnse - FundamentalnodeB ranking is too low\n");
            return;
        }

        if(!fnse.SignatureValid()){
            LogPrintf("FundamentalnodePOS::fnse - Bad fundamentalnode message\n");
            return;
        }

        CFundamentalnode* pfnB = fnmanager.Find(fnse.vinFundamentalnodeB);
        if(pfnB == NULL) return;

        if(fDebug) LogPrintf("ProcessMessageFundamentalnodePOS::fnse - nHeight %d FundamentalnodeA %s FundamentalnodeB %s\n", fnse.nBlockHeight, pfnA->addr.ToString().c_str(), pfnB->addr.ToString().c_str());

        pfnB->ApplyScanningError(fnse);
        fnse.Relay();
    }
}

// Returns how many fundamentalnodes are allowed to scan each block
int GetCountScanningPerBlock()
{
    return std::max(1, fnmanager.CountFundamentalnodesAboveProtocol(MIN_FUNDAMENTALNODE_POS_PROTO_VERSION)/100);
}


void CFundamentalnodeScanning::CleanFundamentalnodeScanningErrors()
{
    if(pindexBest == NULL) return;

    std::map<uint256, CFundamentalnodeScanningError>::iterator it = mapFundamentalnodeScanningErrors.begin();

    while(it != mapFundamentalnodeScanningErrors.end()) {
        if(GetTime() > it->second.nExpiration){ //keep them for an hour
            LogPrintf("Removing old fundamentalnode scanning error %s\n", it->second.GetHash().ToString().c_str());

            mapFundamentalnodeScanningErrors.erase(it++);
        } else {
            it++;
        }
    }

}

// Check other fundamentalnodes to make sure they're running correctly
void CFundamentalnodeScanning::DoFundamentalnodePOSChecks()
{
    if(!fFundamentalNode) return;
    if(fProMode) return;
    if(!IsSporkActive(SPORK_7_FUNDAMENTALNODE_SCANNING)) return;
    if(IsInitialBlockDownload()) return;

    int nBlockHeight = pindexBest->nHeight-5;

    int a = fnmanager.GetFundamentalnodeRank(activeFundamentalnode.vin, nBlockHeight, MIN_FUNDAMENTALNODE_POS_PROTO_VERSION);
    if(a == -1 || a > GetCountScanningPerBlock()){
        // we don't need to do anything this block
        return;
    }

    // The lowest ranking nodes (Fundamentalnode A) check the highest ranking nodes (Fundamentalnode B)
    CFundamentalnode* pfn = fnmanager.GetFundamentalnodeByRank(fnmanager.CountFundamentalnodesAboveProtocol(MIN_FUNDAMENTALNODE_POS_PROTO_VERSION)-a, nBlockHeight, MIN_FUNDAMENTALNODE_POS_PROTO_VERSION, false);
    if(pfn == NULL) return;

    // -- first check : Port is open

    if(!ConnectNode((CAddress)pfn->addr, NULL/* , true */)){
        // we couldn't connect to the node, let's send a scanning error
        CFundamentalnodeScanningError fnse(activeFundamentalnode.vin, pfn->vin, SCANNING_ERROR_NO_RESPONSE, nBlockHeight);
        fnse.Sign();
        mapFundamentalnodeScanningErrors.insert(make_pair(fnse.GetHash(), fnse));
        fnse.Relay();
    }

    // success
    CFundamentalnodeScanningError fnse(activeFundamentalnode.vin, pfn->vin, SCANNING_SUCCESS, nBlockHeight);
    fnse.Sign();
    mapFundamentalnodeScanningErrors.insert(make_pair(fnse.GetHash(), fnse));
    fnse.Relay();
}

bool CFundamentalnodeScanningError::SignatureValid()
{
    std::string errorMessage;
    std::string strMessage = vinFundamentalnodeA.ToString() + vinFundamentalnodeB.ToString() + 
        boost::lexical_cast<std::string>(nBlockHeight) + boost::lexical_cast<std::string>(nErrorType);

    CFundamentalnode* pfn = fnmanager.Find(vinFundamentalnodeA);

    if(pfn == NULL)
    {
        LogPrintf("CFundamentalnodeScanningError::SignatureValid() - Unknown Fundamentalnode\n");
        return false;
    }

    CScript pubkey;
    pubkey.SetDestination(pfn->pubkey2.GetID());
    CTxDestination address1;
    ExtractDestination(pubkey, address1);
    CBitcoinAddress address2(address1);

    if(!fnSigner.VerifyMessage(pfn->pubkey2, vchFundamentalNodeSignature, strMessage, errorMessage)) {
        LogPrintf("CFundamentalnodeScanningError::SignatureValid() - Verify message failed\n");
        return false;
    }

    return true;
}

bool CFundamentalnodeScanningError::Sign()
{
    std::string errorMessage;

    CKey key2;
    CPubKey pubkey2;
    std::string strMessage = vinFundamentalnodeA.ToString() + vinFundamentalnodeB.ToString() + 
        boost::lexical_cast<std::string>(nBlockHeight) + boost::lexical_cast<std::string>(nErrorType);

    if(!fnSigner.SetKey(strFundamentalNodePrivKey, errorMessage, key2, pubkey2))
    {
        LogPrintf("CFundamentalnodeScanningError::Sign() - ERROR: Invalid fundamentalnodeprivkey: '%s'\n", errorMessage.c_str());
        return false;
    }

    CScript pubkey;
    pubkey.SetDestination(pubkey2.GetID());
    CTxDestination address1;
    ExtractDestination(pubkey, address1);
    CBitcoinAddress address2(address1);
    //LogPrintf("signing pubkey2 %s \n", address2.ToString().c_str());

    if(!fnSigner.SignMessage(strMessage, errorMessage, vchFundamentalNodeSignature, key2)) {
        LogPrintf("CFundamentalnodeScanningError::Sign() - Sign message failed");
        return false;
    }

    if(!fnSigner.VerifyMessage(pubkey2, vchFundamentalNodeSignature, strMessage, errorMessage)) {
        LogPrintf("CFundamentalnodeScanningError::Sign() - Verify message failed");
        return false;
    }

    return true;
}

void CFundamentalnodeScanningError::Relay()
{
    CInv inv(MSG_FUNDAMENTALNODE_SCANNING_ERROR, GetHash());

    vector<CInv> vInv;
    vInv.push_back(inv);
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes){
        pnode->PushMessage("inv", vInv);
    }
}

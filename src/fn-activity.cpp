
#include "core.h"
#include "protocol.h"
#include "fn-activity.h"
#include "fn-manager.h"
#include "main.h"
#include <boost/lexical_cast.hpp>

//
// Bootup the Fundamentalnode, look for a 5000 BSD input and register on the network
//
void CActiveFundamentalnode::ManageStatus()
{
    std::string errorMessage;

    if(!fFundamentalNode) return;

    if (fDebug) LogPrintf("CActiveFundamentalnode::ManageStatus() - Begin\n");

    //need correct adjusted time to send ping
    bool fIsInitialDownload = IsInitialBlockDownload();
    if(fIsInitialDownload) {
        status = FUNDAMENTALNODE_SYNC_IN_PROCESS;
        LogPrintf("CActiveFundamentalnode::ManageStatus() - Sync in progress. Must wait until sync is complete to start Fundamentalnode.\n");
        return;
    }

    if(status == FUNDAMENTALNODE_INPUT_TOO_NEW || status == FUNDAMENTALNODE_NOT_CAPABLE || status == FUNDAMENTALNODE_SYNC_IN_PROCESS){
        status = FUNDAMENTALNODE_NOT_PROCESSED;
    }

    if(status == FUNDAMENTALNODE_NOT_PROCESSED) {
        if(strFundamentalNodeAddr.empty()) {
            if(!GetLocal(service)) {
                notCapableReason = "Can't detect external address. Please use the Fundamentalnodeaddr configuration option.";
                status = FUNDAMENTALNODE_NOT_CAPABLE;
                LogPrintf("CActiveFundamentalnode::ManageStatus() - not capable: %s\n", notCapableReason.c_str());
                return;
            }
        } else {
            service = CService(strFundamentalNodeAddr);
        }

        LogPrintf("CActiveFundamentalnode::ManageStatus() - Checking inbound connection to '%s'\n", service.ToString().c_str());

        if(Params().NetworkID() == CChainParams::MAIN){
            if(service.GetPort() != 5647) {
                notCapableReason = "Invalid port: " + boost::lexical_cast<string>(service.GetPort()) + " - only 8886 is supported on mainnet.";
                status = FUNDAMENTALNODE_NOT_CAPABLE;
                LogPrintf("CActiveFundamentalnode::ManageStatus() - not capable: %s\n", notCapableReason.c_str());
                return;
            }
        } else if(service.GetPort() == 30420) {
            notCapableReason = "Invalid port: " + boost::lexical_cast<string>(service.GetPort()) + " - 8886 is only supported on mainnet.";
            status = FUNDAMENTALNODE_NOT_CAPABLE;
            LogPrintf("CActiveFundamentalnode::ManageStatus() - not capable: %s\n", notCapableReason.c_str());
            return;
        }

        if(!ConnectNode((CAddress)service, service.ToString().c_str())){
            notCapableReason = "Could not connect to " + service.ToString();
            status = FUNDAMENTALNODE_NOT_CAPABLE;
            LogPrintf("CActiveFundamentalnode::ManageStatus() - not capable: %s\n", notCapableReason.c_str());
            return;
        }

        if(pwalletMain->IsLocked()){
            notCapableReason = "Wallet is locked.";
            status = FUNDAMENTALNODE_NOT_CAPABLE;
            LogPrintf("CActiveFundamentalnode::ManageStatus() - not capable: %s\n", notCapableReason.c_str());
            return;
        }

        // Set defaults
        status = FUNDAMENTALNODE_NOT_CAPABLE;
        notCapableReason = "Unknown. Check debug.log for more information.";

        // Choose coins to use
        CPubKey pubKeyCollateralAddress;
        CKey keyCollateralAddress;

        if(GetFundamentalNodeVin(vin, pubKeyCollateralAddress, keyCollateralAddress)) {

            if(GetInputAge(vin) < FUNDAMENTALNODE_MIN_CONFIRMATIONS){
                notCapableReason = "Input must have least " + boost::lexical_cast<string>(FUNDAMENTALNODE_MIN_CONFIRMATIONS) +
                        " confirmations - " + boost::lexical_cast<string>(GetInputAge(vin)) + " confirmations";
                LogPrintf("CActiveFundamentalnode::ManageStatus() - %s\n", notCapableReason.c_str());
                status = FUNDAMENTALNODE_INPUT_TOO_NEW;
                return;
            }

            LogPrintf("CActiveFundamentalnode::ManageStatus() - Is capable master node!\n");

            status = FUNDAMENTALNODE_IS_CAPABLE;
            notCapableReason = "";

            pwalletMain->LockCoin(vin.prevout);

            // send to all nodes
            CPubKey pubKeyFundamentalnode;
            CKey keyFundamentalnode;

            if(!fnSigner.SetKey(strFundamentalNodePrivKey, errorMessage, keyFundamentalnode, pubKeyFundamentalnode))
            {
                LogPrintf("Register::ManageStatus() - Error upon calling SetKey: %s\n", errorMessage.c_str());
                return;
            }

            /* donations are not supported in bitsend.conf */
            CScript donationAddress = CScript();
            int donationPercentage = 0;

            if(!Register(vin, service, keyCollateralAddress, pubKeyCollateralAddress, keyFundamentalnode, pubKeyFundamentalnode, donationAddress, donationPercentage, errorMessage)) {
                LogPrintf("CActiveFundamentalnode::ManageStatus() - Error on Register: %s\n", errorMessage.c_str());
            }

            return;
        } else {
            notCapableReason = "Could not find suitable coins!";
            LogPrintf("CActiveFundamentalnode::ManageStatus() - %s\n", notCapableReason.c_str());
        }
    }

    //send to all peers
    if(!Fnep(errorMessage)) {
        LogPrintf("CActiveFundamentalnode::ManageStatus() - Error on Ping: %s\n", errorMessage.c_str());
    }
}

// Send stop fnep to network for remote Fundamentalnode
bool CActiveFundamentalnode::StopFundamentalNode(std::string strService, std::string strKeyFundamentalnode, std::string& errorMessage) {
    CTxIn vin;
    CKey keyFundamentalnode;
    CPubKey pubKeyFundamentalnode;

    if(!fnSigner.SetKey(strKeyFundamentalnode, errorMessage, keyFundamentalnode, pubKeyFundamentalnode)) {
        LogPrintf("CActiveFundamentalnode::StopFundamentalNode() - Error: %s\n", errorMessage.c_str());
        return false;
    }

    return StopFundamentalNode(vin, CService(strService), keyFundamentalnode, pubKeyFundamentalnode, errorMessage);
}

// Send stop fnep to network for main Fundamentalnode
bool CActiveFundamentalnode::StopFundamentalNode(std::string& errorMessage) {
    if(status != FUNDAMENTALNODE_IS_CAPABLE && status != FUNDAMENTALNODE_REMOTELY_ENABLED) {
        errorMessage = "Fundamentalnode is not in a running status";
        LogPrintf("CActiveFundamentalnode::StopFundamentalNode() - Error: %s\n", errorMessage.c_str());
        return false;
    }

    status = FUNDAMENTALNODE_STOPPED;

    CPubKey pubKeyFundamentalnode;
    CKey keyFundamentalnode;

    if(!fnSigner.SetKey(strFundamentalNodePrivKey, errorMessage, keyFundamentalnode, pubKeyFundamentalnode))
    {
        LogPrintf("Register::ManageStatus() - Error upon calling SetKey: %s\n", errorMessage.c_str());
        return false;
    }

    return StopFundamentalNode(vin, service, keyFundamentalnode, pubKeyFundamentalnode, errorMessage);
}

// Send stop dseep to network for any Fundamentalnode
bool CActiveFundamentalnode::StopFundamentalNode(CTxIn vin, CService service, CKey keyFundamentalnode, CPubKey pubKeyFundamentalnode, std::string& errorMessage) {
    pwalletMain->UnlockCoin(vin.prevout);
    return Fnep(vin, service, keyFundamentalnode, pubKeyFundamentalnode, errorMessage, true);
}

bool CActiveFundamentalnode::Fnep(std::string& errorMessage) {
    if(status != FUNDAMENTALNODE_IS_CAPABLE && status != FUNDAMENTALNODE_REMOTELY_ENABLED) {
        errorMessage = "Fundamentalnode is not in a running status";
        LogPrintf("CActiveFundamentalnode::Fnep() - Error: %s\n", errorMessage.c_str());
        return false;
    }

    CPubKey pubKeyFundamentalnode;
    CKey keyFundamentalnode;

    if(!fnSigner.SetKey(strFundamentalNodePrivKey, errorMessage, keyFundamentalnode, pubKeyFundamentalnode))
    {
        LogPrintf("CActiveFundamentalnode::Fnep() - Error upon calling SetKey: %s\n", errorMessage.c_str());
        return false;
    }

    return Fnep(vin, service, keyFundamentalnode, pubKeyFundamentalnode, errorMessage, false);
}

bool CActiveFundamentalnode::Fnep(CTxIn vin, CService service, CKey keyFundamentalnode, CPubKey pubKeyFundamentalnode, std::string &retErrorMessage, bool stop) {
    std::string errorMessage;
    std::vector<unsigned char> vchFundamentalNodeSignature;
    std::string strFundamentalNodeSignMessage;
    int64_t fundamentalNodeSignatureTime = GetAdjustedTime();

    std::string strMessage = service.ToString() + boost::lexical_cast<std::string>(fundamentalNodeSignatureTime) + boost::lexical_cast<std::string>(stop);

    if(!fnSigner.SignMessage(strMessage, errorMessage, vchFundamentalNodeSignature, keyFundamentalnode)) {
        retErrorMessage = "sign message failed: " + errorMessage;
        LogPrintf("CActiveFundamentalnode::Fnep() - Error: %s\n", retErrorMessage.c_str());
        return false;
    }

    if(!fnSigner.VerifyMessage(pubKeyFundamentalnode, vchFundamentalNodeSignature, strMessage, errorMessage)) {
        retErrorMessage = "Verify message failed: " + errorMessage;
        LogPrintf("CActiveFundamentalnode::Fnep() - Error: %s\n", retErrorMessage.c_str());
        return false;
    }

    // Update Last Seen timestamp in Fundamentalnode list
    CFundamentalnode* pfn = fnmanager.Find(vin);
    if(pfn != NULL)
    {
        if(stop)
        fnmanager.Remove(pfn->vin);
        else
        pfn->UpdateLastSeen();
    }
    else
    {
        // Seems like we are trying to send a ping while the Fundamentalnode is not registered in the network
        retErrorMessage = "Fundamentalnode List doesn't include our Fundamentalnode, Shutting down Fundamentalnode pinging service! " + vin.ToString();
        LogPrintf("CActiveFundamentalnode::Fnep() - Error: %s\n", retErrorMessage.c_str());
        status = FUNDAMENTALNODE_NOT_CAPABLE;
        notCapableReason = retErrorMessage;
        return false;
    }

    //send to all peers
    LogPrintf("CActiveFundamentalnode::Fnep() - RelayFundamentalnodeEntryPing vin = %s\n", vin.ToString().c_str());
    fnmanager.RelayFundamentalnodeEntryPing(vin, vchFundamentalNodeSignature, fundamentalNodeSignatureTime, stop);

    return true;
}

bool CActiveFundamentalnode::Register(std::string strService, std::string strKeyFundamentalnode, std::string txHash, std::string strOutputIndex, std::string strDonationAddress, std::string strDonationPercentage, std::string& errorMessage) {
    CTxIn vin;
    CPubKey pubKeyCollateralAddress;
    CKey keyCollateralAddress;
    CPubKey pubKeyFundamentalnode;
    CKey keyFundamentalnode;
    CScript donationAddress = CScript();
    int donationPercentage = 0;

    if(!fnSigner.SetKey(strKeyFundamentalnode, errorMessage, keyFundamentalnode, pubKeyFundamentalnode))
    {
        LogPrintf("CActiveFundamentalnode::Register() - Error upon calling SetKey: %s\n", errorMessage.c_str());
        return false;
    }

    if(!GetFundamentalNodeVin(vin, pubKeyCollateralAddress, keyCollateralAddress, txHash, strOutputIndex)) {
        errorMessage = "could not allocate vin";
        LogPrintf("CActiveFundamentalnode::Register() - Error: %s\n", errorMessage.c_str());
        return false;
    }

    CBitcoinAddress address;
    if (strDonationAddress != "")
    {
        if(!address.SetString(strDonationAddress))
        {
            LogPrintf("CActiveFundamentalnode::Register - Invalid Donation Address\n");
            return false;
        }
        donationAddress.SetDestination(address.Get());

        try {
            donationPercentage = boost::lexical_cast<int>( strDonationPercentage );
        } catch( boost::bad_lexical_cast const& ) {
            LogPrintf("CActiveFundamentalnode::Register - Invalid Donation Percentage (Couldn't cast)\n");
            return false;
        }

        if(donationPercentage < 0 || donationPercentage > 100)
        {
            LogPrintf("CActiveFundamentalnode::Register - Donation Percentage Out Of Range\n");
            return false;
        }
    }

    return Register(vin, CService(strService), keyCollateralAddress, pubKeyCollateralAddress, keyFundamentalnode, pubKeyFundamentalnode, donationAddress, donationPercentage, errorMessage);
}

bool CActiveFundamentalnode::Register(CTxIn vin, CService service, CKey keyCollateralAddress, CPubKey pubKeyCollateralAddress, CKey keyFundamentalnode, CPubKey pubKeyFundamentalnode, CScript donationAddress, int donationPercentage, std::string &retErrorMessage) {
    std::string errorMessage;
    std::vector<unsigned char> vchFundamentalNodeSignature;
    std::string strFundamentalNodeSignMessage;
    int64_t fundamentalNodeSignatureTime = GetAdjustedTime();

    std::string vchPubKey(pubKeyCollateralAddress.begin(), pubKeyCollateralAddress.end());
    std::string vchPubKey2(pubKeyFundamentalnode.begin(), pubKeyFundamentalnode.end());

    std::string strMessage = service.ToString() + boost::lexical_cast<std::string>(fundamentalNodeSignatureTime) + vchPubKey + vchPubKey2 + boost::lexical_cast<std::string>(PROTOCOL_VERSION) + donationAddress.ToString() + boost::lexical_cast<std::string>(donationPercentage);

    if(!fnSigner.SignMessage(strMessage, errorMessage, vchFundamentalNodeSignature, keyCollateralAddress)) {
        retErrorMessage = "sign message failed: " + errorMessage;
        LogPrintf("CActiveFundamentalnode::Register() - Error: %s\n", retErrorMessage.c_str());
        return false;
    }

    if(!fnSigner.VerifyMessage(pubKeyCollateralAddress, vchFundamentalNodeSignature, strMessage, errorMessage)) {
        retErrorMessage = "Verify message failed: " + errorMessage;
        LogPrintf("CActiveFundamentalnode::Register() - Error: %s\n", retErrorMessage.c_str());
        return false;
    }

    CFundamentalnode* pfn = fnmanager.Find(vin);
    if(pfn == NULL)
    {
        LogPrintf("CActiveFundamentalnode::Register() - Adding to Fundamentalnode list service: %s - vin: %s\n", service.ToString().c_str(), vin.ToString().c_str());
        CFundamentalnode fn(service, vin, pubKeyCollateralAddress, vchFundamentalNodeSignature, fundamentalNodeSignatureTime, pubKeyFundamentalnode, PROTOCOL_VERSION, donationAddress, donationPercentage);
        fn.UpdateLastSeen(fundamentalNodeSignatureTime);
        fnmanager.Add(fn);
    }

    //send to all peers
    LogPrintf("CActiveFundamentalnode::Register() - RelayElectionEntry vin = %s\n", vin.ToString().c_str());
    fnmanager.RelayFundamentalnodeEntry(vin, service, vchFundamentalNodeSignature, fundamentalNodeSignatureTime, pubKeyCollateralAddress, pubKeyFundamentalnode, -1, -1, fundamentalNodeSignatureTime, PROTOCOL_VERSION, donationAddress, donationPercentage);

    return true;
}

bool CActiveFundamentalnode::GetFundamentalNodeVin(CTxIn& vin, CPubKey& pubkey, CKey& secretKey) {
    return GetFundamentalNodeVin(vin, pubkey, secretKey, "", "");
}

bool CActiveFundamentalnode::GetFundamentalNodeVin(CTxIn& vin, CPubKey& pubkey, CKey& secretKey, std::string strTxHash, std::string strOutputIndex) {
    CScript pubScript;

    // Find possible candidates
    vector<COutput> possibleCoins = SelectCoinsFundamentalnode();
    COutput *selectedOutput;

    // Find the vin
    if(!strTxHash.empty()) {
        // Let's find it
        uint256 txHash(strTxHash);
        int outputIndex = boost::lexical_cast<int>(strOutputIndex);
        bool found = false;
        BOOST_FOREACH(COutput& out, possibleCoins) {
            if(out.tx->GetHash() == txHash && out.i == outputIndex)
            {
                selectedOutput = &out;
                found = true;
                break;
            }
        }
        if(!found) {
            LogPrintf("CActiveFundamentalnode::GetFundamentalNodeVin - Could not locate valid vin\n");
            return false;
        }
    } else {
        // No output specified,  Select the first one
        if(possibleCoins.size() > 0) {
            selectedOutput = &possibleCoins[0];
        } else {
            LogPrintf("CActiveFundamentalnode::GetFundamentalNodeVin - Could not locate specified vin from possible list\n");
            return false;
        }
    }

    // At this point we have a selected output, retrieve the associated info
    return GetVinFromOutput(*selectedOutput, vin, pubkey, secretKey);
}


// Extract Fundamentalnode vin information from output
bool CActiveFundamentalnode::GetVinFromOutput(COutput out, CTxIn& vin, CPubKey& pubkey, CKey& secretKey) {

    CScript pubScript;

    vin = CTxIn(out.tx->GetHash(),out.i);
    pubScript = out.tx->vout[out.i].scriptPubKey; // the inputs PubKey

    CTxDestination address1;
    ExtractDestination(pubScript, address1);
    CBitcoinAddress address2(address1);

    CKeyID keyID;
    if (!address2.GetKeyID(keyID)) {
        LogPrintf("CActiveFundamentalnode::GetFundamentalNodeVin - Address does not refer to a key\n");
        return false;
    }

    if (!pwalletMain->GetKey(keyID, secretKey)) {
        LogPrintf ("CActiveFundamentalnode::GetFundamentalNodeVin - Private key for address is not known\n");
        return false;
    }

    pubkey = secretKey.GetPubKey();
    return true;
}

// get all possible outputs for running Fundamentalnode
vector<COutput> CActiveFundamentalnode::SelectCoinsFundamentalnode()
{
    vector<COutput> vCoins;
    vector<COutput> filteredCoins;

    // Retrieve all possible outputs
    pwalletMain->AvailableCoins(vCoins, true, NULL, true);

    // Filter
    BOOST_FOREACH(const COutput& out, vCoins)
    {
        if(out.tx->vout[out.i].nValue == 1 * COIN/*FUNDAMENTALNODEAMOUNT*COIN*/) { //exactly        bitsenddev   04-2015
            filteredCoins.push_back(out);
        }
    }
    return filteredCoins;
}

// when starting a Fundamentalnode, this can enable to run as a hot wallet with no funds
bool CActiveFundamentalnode::EnableHotColdFundamentalNode(CTxIn& newVin, CService& newService)
{
    if(!fFundamentalNode) return false;

    status = FUNDAMENTALNODE_REMOTELY_ENABLED;

    //The values below are needed for signing dseep messages going forward
    this->vin = newVin;
    this->service = newService;

    LogPrintf("CActiveFundamentalnode::EnableHotColdFundamentalNode() - Enabled! You may shut down the cold daemon.\n");

    return true;
}

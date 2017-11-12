
#include "net.h"
#include "fn-config.h"
#include "util.h"
#include <base58.h>

CFundamentalnodeConfig fundamentalnodeConfig;

void CFundamentalnodeConfig::add(std::string alias, std::string ip, std::string privKey, std::string txHash, std::string outputIndex, std::string donationAddress, std::string donationPercent) {
    CFundamentalnodeEntry cme(alias, ip, privKey, txHash, outputIndex, donationAddress, donationPercent);
    entries.push_back(cme);
}

bool CFundamentalnodeConfig::read(std::string& strErr) {
    boost::filesystem::ifstream streamConfig(GetFundamentalnodeConfigFile());
    if (!streamConfig.good()) {
		LogPrintf("Fundamental node conf file not found. \n");
        return true; // No fundamentalnode.conf file is OK
    }

    for(std::string line; std::getline(streamConfig, line); )
    {
        if(line.empty()) {
            continue;
        }
        std::istringstream iss(line);
        std::string alias, ip, privKey, txHash, outputIndex, donation, donationAddress, donationPercent;
        if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex >> donation)) {
            donationAddress = "";
            donationPercent = "";
            iss.str(line);
            iss.clear();
            if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) {
                strErr = "Could not parse fundamentalnode.conf line: " + line;
                streamConfig.close();
                return false;
            }
        } else {
            size_t pos = donation.find_first_of(":");
            if(pos == string::npos) { // no ":" found
                donationPercent = "100";
                donationAddress = donation;
            } else {
                donationPercent = donation.substr(pos + 1);
                donationAddress = donation.substr(0, pos);
            }
            CBitcoinAddress address(donationAddress);
            if (!address.IsValid()) {
                strErr = "Invalid Bitsend address in fundamentalnode.conf line: " + line;
                streamConfig.close();
                return false;
            }
        }

        if(Params().NetworkID() == CChainParams::MAIN){
            if(CService(ip).GetPort() != 5647) {
                strErr = "Invalid port detected in fundamentalnode.conf: " + line + " (must be 5647 for mainnet)";
                streamConfig.close();
                return false;
            }
        } else if(CService(ip).GetPort() != 30420) {
            strErr = "Invalid port detected in fundamentalnode.conf: " + line + " (30420 must be only on mainnet)";
            streamConfig.close();
            return false;
        }


        add(alias, ip, privKey, txHash, outputIndex, donationAddress, donationPercent);
    }

    streamConfig.close();
    return true;
}

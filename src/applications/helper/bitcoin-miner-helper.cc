/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#include "bitcoin-miner-helper.h"
#include "ns3/string.h"
#include "ns3/inet-socket-address.h"
#include "ns3/names.h"
#include "ns3/uinteger.h"
#include "../model/bitcoin-miner.h"
#include "ns3/log.h"
#include "ns3/double.h"

namespace ns3 {

BitcoinMinerHelper::BitcoinMinerHelper (std::string protocol, Address address, std::vector<Ipv4Address> peers, nodeStatistics *stats,
										double hashRate, double blockGenBinSize, double blockGenParameter,
										double averageBlockGenIntervalSeconds) : BitcoinNodeHelper (),  m_minerType (NORMAL_MINER), m_secureBlocks (6)
{
  m_factory.SetTypeId ("ns3::BitcoinMiner");
  commonConstructor(protocol, address, peers, stats);
  
  m_hashRate = hashRate;
  m_blockGenBinSize = blockGenBinSize;
  m_blockGenParameter = blockGenParameter;
  m_averageBlockGenIntervalSeconds = averageBlockGenIntervalSeconds;
  
  m_factory.Set ("HashRate", DoubleValue(m_hashRate));
  m_factory.Set ("BlockGenBinSize", DoubleValue(m_blockGenBinSize));
  m_factory.Set ("BlockGenParameter", DoubleValue(m_blockGenParameter));
  m_factory.Set ("AverageBlockGenIntervalSeconds", DoubleValue(m_averageBlockGenIntervalSeconds));

}

Ptr<Application>
BitcoinMinerHelper::InstallPriv (Ptr<Node> node)
{

   switch (m_minerType) 
   {
      case NORMAL_MINER: 
	  {
        Ptr<BitcoinMiner> app = m_factory.Create<BitcoinMiner> ();
        app->SetPeersAddresses(m_peersAddresses);
        app->SetNodeStats(m_nodeStats);

        node->AddApplication (app);
        return app;
	  }
      case SIMPLE_ATTACKER: 
	  {
        Ptr<BitcoinSimpleAttacker> app = m_factory.Create<BitcoinSimpleAttacker> ();
        app->SetPeersAddresses(m_peersAddresses);
        app->SetNodeStats(m_nodeStats);

        node->AddApplication (app);
        return app;
	  }
   }
   
}

enum MinerType 
BitcoinMinerHelper::GetMinerType(void)
{
  return m_minerType;
}

void 
BitcoinMinerHelper::SetMinerType (enum MinerType m)
{
  m_minerType = m;
  m_factory.SetTypeId ("ns3::BitcoinSimpleAttacker");
  
  m_factory.Set ("Protocol", StringValue (m_protocol));
  m_factory.Set ("Local", AddressValue (m_address));
  m_factory.Set ("HashRate", DoubleValue(m_hashRate));
  m_factory.Set ("BlockGenBinSize", DoubleValue(m_blockGenBinSize));
  m_factory.Set ("BlockGenParameter", DoubleValue(m_blockGenParameter));
  m_factory.Set ("AverageBlockGenIntervalSeconds", DoubleValue(m_averageBlockGenIntervalSeconds));
  
  if (m_minerType != NORMAL_MINER)
    m_factory.Set ("SecureBlocks", UintegerValue(m_secureBlocks));
  std::cout << "Changed minerType to " << getMinerType(m) << std::endl;
}

} // namespace ns3

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

BitcoinMinerHelper::BitcoinMinerHelper (std::string protocol, Address address, std::vector<Address> peers,
										double hashRate, double blockGenBinSize, double blockGenParameter,
										double averageBlockGenIntervalSeconds) : BitcoinNodeHelper ()
{
  m_factory.SetTypeId ("ns3::BitcoinMiner");
  commonConstructor(protocol, address, peers);
  m_factory.Set ("HashRate", DoubleValue(hashRate));
  m_factory.Set ("BlockGenBinSize", DoubleValue(blockGenBinSize));
  m_factory.Set ("BlockGenParameter", DoubleValue(blockGenParameter));
  m_factory.Set ("AverageBlockGenIntervalSeconds", DoubleValue(averageBlockGenIntervalSeconds));

}

Ptr<Application>
BitcoinMinerHelper::InstallPriv (Ptr<Node> node) const
{

  Ptr<BitcoinMiner> app = m_factory.Create<BitcoinMiner> ();
  app->SetPeersAddresses(m_peersAddresses);

  node->AddApplication (app);

  return app;
}

} // namespace ns3

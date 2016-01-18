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
#ifndef BITCOIN_MINER_HELPER_H
#define BITCOIN_MINER_HELPER_H

#include "bitcoin-node-helper.h"


namespace ns3 {

/**
 * Based on packet-sink-helper
 */
 
class BitcoinMinerHelper : public BitcoinNodeHelper
{
  public:
  /**
   * Create a PacketSinkHelper to make it easier to work with PacketSinkApplications
   *
   * \param protocol the name of the protocol to use to receive traffic
   *        This string identifies the socket factory type used to create
   *        sockets for the applications.  A typical value would be 
   *        ns3::TcpSocketFactory.
   * \param address the address of the bitcoin node,
   *
   */
  BitcoinMinerHelper (std::string protocol, Address address, std::vector<Ipv4Address> peers, nodeStatistics *stats,
					  double hashRate, double blockGenBinSize, double blockGenParameter, double averageBlockGenIntervalSeconds);
  
protected:
  /**
   * Install an ns3::PacketSink on the node configured with all the
   * attributes set with SetAttribute.
   *
   * \param node The node on which an PacketSink will be installed.
   * \returns Ptr to the application installed.
   */
  virtual Ptr<Application> InstallPriv (Ptr<Node> node) const;
    
};

} // namespace ns3

#endif /* BITCOIN_MINER_HELPER_H */

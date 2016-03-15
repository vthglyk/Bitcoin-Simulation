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
#ifndef BITCOIN_NODE_HELPER_H
#define BITCOIN_NODE_HELPER_H

#include "ns3/object-factory.h"
#include "ns3/ipv4-address.h"
#include "ns3/node-container.h"
#include "ns3/application-container.h"
#include "ns3/uinteger.h"
#include "ns3/bitcoin.h"

namespace ns3 {

/**
 * Based on packet-sink-helper
 */
 
class BitcoinNodeHelper
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
  BitcoinNodeHelper (std::string protocol, Address address, std::vector<Ipv4Address> &peers, 
                     std::map<Ipv4Address, double> &peersDownloadSpeeds, nodeInternetSpeeds &internetSpeeds, nodeStatistics *stats);
  
  /**
   * Called by subclasses to set a different factory TypeId
   */
  BitcoinNodeHelper (void);
  
  /**
   * Common Constructor called both from the base class and the subclasses
   */
   void commonConstructor(std::string protocol, Address address, std::vector<Ipv4Address> &peers, 
                          std::map<Ipv4Address, double> &peersDownloadSpeeds, nodeInternetSpeeds &internetSpeeds, nodeStatistics *stats);
  
  /**
   * Helper function used to set the underlying application attributes.
   *
   * \param name the name of the application attribute to set
   * \param value the value of the application attribute to set
   */
  void SetAttribute (std::string name, const AttributeValue &value);

  /**
   * Install an ns3::PacketSinkApplication on each node of the input container
   * configured with all the attributes set with SetAttribute.
   *
   * \param c NodeContainer of the set of nodes on which a PacketSinkApplication 
   * will be installed.
   * \returns Container of Ptr to the applications installed.
   */
  ApplicationContainer Install (NodeContainer c);

  /**
   * Install an ns3::PacketSinkApplication on each node of the input container
   * configured with all the attributes set with SetAttribute.
   *
   * \param node The node on which a PacketSinkApplication will be installed.
   * \returns Container of Ptr to the applications installed.
   */
  ApplicationContainer Install (Ptr<Node> node);

  /**
   * Install an ns3::PacketSinkApplication on each node of the input container
   * configured with all the attributes set with SetAttribute.
   *
   * \param nodeName The name of the node on which a PacketSinkApplication will be installed.
   * \returns Container of Ptr to the applications installed.
   */
  ApplicationContainer Install (std::string nodeName);

  void SetPeersAddresses (std::vector<Ipv4Address> &peersAddresses);
  
  void SetPeersDownloadSpeeds (std::map<Ipv4Address, double> &peersDownloadSpeeds);

  void SetNodeInternetSpeeds (nodeInternetSpeeds &internetSpeeds);

  void SetNodeStats (nodeStatistics *nodeStats);

  void SetProtocolType (enum ProtocolType protocolType);
  
protected:
  /**
   * Install an ns3::PacketSink on the node configured with all the
   * attributes set with SetAttribute.
   *
   * \param node The node on which an PacketSink will be installed.
   * \returns Ptr to the application installed.
   */
  virtual Ptr<Application> InstallPriv (Ptr<Node> node);
  
  ObjectFactory                                       m_factory; //!< Object factory.
  std::string                                         m_protocol;
  Address                                             m_address;
  std::vector<Ipv4Address>		                      m_peersAddresses; //!< The addresses of peers
  std::map<Ipv4Address, double>                       m_peersDownloadSpeeds;
  nodeInternetSpeeds                                  m_internetSpeeds;
  nodeStatistics                                      *m_nodeStats;
  enum ProtocolType									  m_protocolType;

};

} // namespace ns3

#endif /* BITCOIN_NODE_HELPER_H */

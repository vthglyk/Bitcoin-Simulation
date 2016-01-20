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

#include "bitcoin-node-helper.h"
#include "ns3/string.h"
#include "ns3/inet-socket-address.h"
#include "ns3/names.h"
#include "../model/bitcoin-node.h"

namespace ns3 {

BitcoinNodeHelper::BitcoinNodeHelper (std::string protocol, Address address, std::vector<Ipv4Address> &peers, nodeStatistics *stats)
{
  m_factory.SetTypeId ("ns3::BitcoinNode");
  commonConstructor (protocol, address, peers, stats);
}

BitcoinNodeHelper::BitcoinNodeHelper (void)
{
}

void 
BitcoinNodeHelper::commonConstructor(std::string protocol, Address address, std::vector<Ipv4Address> &peers, nodeStatistics *stats) 
{
  m_protocol = protocol;
  m_address = address;
  m_peersAddresses = peers;
  m_nodeStats = stats;

  m_factory.Set ("Protocol", StringValue (m_protocol));
  m_factory.Set ("Local", AddressValue (m_address));

}

void 
BitcoinNodeHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
BitcoinNodeHelper::Install (Ptr<Node> node)
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
BitcoinNodeHelper::Install (std::string nodeName)
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
BitcoinNodeHelper::Install (NodeContainer c)
{ 

  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
BitcoinNodeHelper::InstallPriv (Ptr<Node> node)
{
  Ptr<BitcoinNode> app = m_factory.Create<BitcoinNode> ();
  app->SetPeersAddresses(m_peersAddresses);
  app->SetNodeStats(m_nodeStats);
  
  node->AddApplication (app);

  return app;
}

void 
BitcoinNodeHelper::SetPeersAddresses (std::vector<Ipv4Address> &peersAddresses)
{
  m_peersAddresses = peersAddresses;	
}

void 
BitcoinNodeHelper::SetNodeStats (nodeStatistics *nodeStats)
{
  m_nodeStats = nodeStats;
}

} // namespace ns3

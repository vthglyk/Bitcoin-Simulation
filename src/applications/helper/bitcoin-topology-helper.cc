/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 * Author: Josh Pelkey <jpelkey@gatech.edu>
 */

#include "ns3/bitcoin-topology-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/string.h"
#include "ns3/vector.h"
#include "ns3/log.h"
#include "ns3/ipv6-address-generator.h"
#include "ns3/random-variable-stream.h"
#include "ns3/double.h"
#include <algorithm>
#include <fstream>
#include <time.h>
#include <sys/time.h>


static double GetWallTime();
namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BitcoinTopologyHelper");

BitcoinTopologyHelper::BitcoinTopologyHelper (uint32_t noCpus, uint32_t totalNoNodes, uint32_t noMiners,
                                              double bandwidth, int minConnectionsPerNode, int maxConnectionsPerNode,
						                      double latencyParetoMean, double latencyParetoShape, uint32_t systemId)
  : m_noCpus(noCpus), m_totalNoNodes (totalNoNodes), m_noMiners (noMiners), m_bandwidth (bandwidth), 
    m_minConnectionsPerNode (minConnectionsPerNode), m_maxConnectionsPerNode (maxConnectionsPerNode), 
	m_totalNoLinks (0), m_latencyParetoMean (latencyParetoMean), m_latencyParetoShape (latencyParetoShape), 
	m_systemId (systemId)
{
  
  std::vector<uint32_t>     nodes;    //nodes contain the ids of the nodes
  double                    tStart = GetWallTime();
  double                    tFinish;
  
  // Bounds check
  if (m_noMiners > m_totalNoNodes)
  {
    NS_FATAL_ERROR ("The number of miners is larger than the total number of nodes\n");
  }
  
  if (m_noMiners < 1)
  {
    NS_FATAL_ERROR ("You need at least one miner\n");
  }


  for (int i = 0; i < m_totalNoNodes; i++)
  {
    nodes.push_back(i);
  }

/*   //Print the initialized nodes
  if (m_systemId == 0)
  {
    for (std::vector<uint32_t>::iterator j = nodes.begin(); j != nodes.end(); j++)
    {
	  std::cout << *j << " " ;
    }
  } */

  //Choose the miners randomly. They should be unique (no miner should be chosen twice).
  //So, remove each chose miner from nodes vector
  for (int i = 0; i < noMiners; i++)
  {
    uint32_t index = rand() % nodes.size();
    m_miners.push_back(nodes[index]);
	
/*     if (m_systemId == 0)
      std::cout << "\n" << "Chose " << nodes[index] << "     "; */

    nodes.erase(nodes.begin() + index);
	  
/* 	if (m_systemId == 0) 
	{		
      for (std::vector<uint32_t>::iterator it = nodes.begin(); it != nodes.end(); it++)
      {
	    std::cout << *it << " " ;
      }
	} */
  }
  
  
/*   //Print the miners
  if (m_systemId == 0)
  {
    std::cout << "\n\nThe miners are:\n";
    for (std::vector<uint32_t>::iterator j = m_miners.begin(); j != m_miners.end(); j++)
    {
	  std::cout << *j << " " ;
    }
    std::cout << "\n\n";
  } */
  
  //Interconnect the miners
  for(auto &miner : m_miners)
  {
    for(auto &peer : m_miners)
    {
      if (miner != peer)
        m_nodesConnections[miner].push_back(peer);
	}
  }
  
  
/*   //Print the miners' connections
  if (m_systemId == 0)
  {
    std::cout << "The miners are interconnected:";
    for(auto &miner : m_nodesConnections)
    {
	  std::cout << "\nMiner " << miner.first << ":\t" ;
	  for(std::vector<uint32_t>::const_iterator it = miner.second.begin(); it != miner.second.end(); it++)
	  {
        std::cout << *it << "\t" ;
	  }
    }
    std::cout << "\n" << std::endl;
  } */
  
  //Interconnect the nodes
 
  //nodes contain the ids of the nodes
  nodes.clear();

  for (int i = 0; i < m_totalNoNodes; i++)
  {
    nodes.push_back(i);
  }
	
  for(int i = 0; i < m_totalNoNodes; i++)
  {
	int count = 0;
    while (m_nodesConnections[i].size() < m_minConnectionsPerNode && count < 2*m_minConnectionsPerNode)
    {
      uint32_t index = rand() % nodes.size();
	  uint32_t candidatePeer = nodes[index];
		
      if (candidatePeer == i)
      {
/* 		if (m_systemId == 0)
          std::cout << "Node " << i << " does not need a connection with itself" << "\n"; */
      }
      else if (std::find(m_nodesConnections[i].begin(), m_nodesConnections[i].end(), candidatePeer) != m_nodesConnections[i].end())
      {
/* 		if (m_systemId == 0)
          std::cout << "Node " << i << " has already a connection to Node " << nodes[index] << "\n"; */
      }
      else if (m_nodesConnections[candidatePeer].size() >= m_maxConnectionsPerNode)
      {
/* 		if (m_systemId == 0)
          std::cout << "Node " << nodes[index] << " has already " << m_maxConnectionsPerNode << " connections" << "\n"; */
      }
      else
      {
        m_nodesConnections[i].push_back(candidatePeer);
        m_nodesConnections[candidatePeer].push_back(i);
        if (m_nodesConnections[candidatePeer].size() == m_maxConnectionsPerNode)
        {
/* 		  if (m_systemId == 0)
            std::cout << "Node " << nodes[index] << " is removed from index\n"; */
          nodes.erase(nodes.begin() + index);
        }
      }
      count++;
	}
	  
	if (m_nodesConnections[i].size() < m_minConnectionsPerNode && m_systemId == 0)
	  std::cout << "Node " << i << " has only " << m_nodesConnections[i].size() << " connections\n";

  }
	
/*   //Print the nodes' connections
  if (m_systemId == 0)
  {
    std::cout << "The nodes connections are:" << std::endl;
    for(auto &node : m_nodesConnections)
    {
  	  std::cout << "\nNode " << node.first << ":    " ;
	  for(std::vector<uint32_t>::const_iterator it = node.second.begin(); it != node.second.end(); it++)
	  {
        std::cout  << "\t" << *it;
	  }
    }
    std::cout << "\n" << std::endl;
  } */

  tFinish = GetWallTime();
  if (m_systemId == 0)
  {
    std::cout << "The nodes connections were created in " << tFinish - tStart << "s.\n";
    std::cout << "The minimum number of connections for each node are " << m_minConnectionsPerNode 
              << " and whereas the maximum are " << m_maxConnectionsPerNode << "\n";
  }
  
  
  InternetStackHelper stack;
  
  Ptr<ParetoRandomVariable> paretoDistribution = CreateObject<ParetoRandomVariable> ();
  paretoDistribution->SetAttribute ("Mean", DoubleValue (m_latencyParetoMean));
  paretoDistribution->SetAttribute ("Shape", DoubleValue (m_latencyParetoShape));
  
  std::ostringstream latencyStringStream; 
  std::ostringstream bandwidthStream;
  
  bandwidthStream << m_bandwidth << "Mbps";
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (bandwidthStream.str()));

  
  tStart = GetWallTime();
  
  for (uint32_t i = 0; i < m_totalNoNodes; i++)
  {
    NodeContainer currentNode;
    currentNode.Create (1, i % m_noCpus);
/* 	if (m_systemId == 0)
      std::cout << "Creating a node with Id = " << i << " and systemId = " << i % m_noCpus << "\n"; */
    m_nodes.push_back (currentNode);
  }
  
  tFinish = GetWallTime();
  if (m_systemId == 0)
    std::cout << "The nodes were created in " << tFinish - tStart << "s.\n";

  tStart = GetWallTime();
  for(auto &node : m_nodesConnections)  
  {

    for(std::vector<uint32_t>::const_iterator it = node.second.begin(); it != node.second.end(); it++)
    {
      if ( *it > node.first)	//Do not recreate links
      {
        NetDeviceContainer newDevices;
		
        m_totalNoLinks++;
        latencyStringStream.str("");
        latencyStringStream.clear();
        latencyStringStream << paretoDistribution->GetValue() << "ms";
        pointToPoint.SetChannelAttribute ("Delay", StringValue (latencyStringStream.str()));
        newDevices.Add (pointToPoint.Install (m_nodes.at (node.first).Get (0), m_nodes.at (*it).Get (0)));
		m_devices.push_back (newDevices);
/* 		if (m_systemId == 0)
          std::cout << "Creating link " << m_totalNoLinks << " between nodes " 
                    << (m_nodes.at (node.first).Get (0))->GetId() 
                    << " and " << (m_nodes.at (*it).Get (0))->GetId() << "\n"; */
      }
    }
  }
  
  tFinish = GetWallTime();

  if (m_systemId == 0)
    std::cout << "The total number of links is " << m_totalNoLinks << " (" << tFinish - tStart << "s).\n";
}

BitcoinTopologyHelper::~BitcoinTopologyHelper ()
{
}

void
BitcoinTopologyHelper::InstallStack (InternetStackHelper stack)
{
  double tStart = GetWallTime();
  double tFinish;
  
  for (uint32_t i = 0; i < m_nodes.size (); ++i)
    {
      NodeContainer currentNode = m_nodes[i];
      for (uint32_t j = 0; j < currentNode.GetN (); ++j)
        {
          stack.Install (currentNode.Get (j));
        }
    }
	
  tFinish = GetWallTime();
  if (m_systemId == 0)
    std::cout << "Internet stack installed in " << tFinish - tStart << "s.\n";
}

void
BitcoinTopologyHelper::AssignIpv4Addresses (Ipv4AddressHelperCustom ip)
{
  double tStart = GetWallTime();
  double tFinish;
  
  // Assign addresses to all devices in the network.
  // These devices are stored in a vector. 
  for (uint32_t i = 0; i < m_devices.size (); ++i)
  {
    Ipv4InterfaceContainer newInterfaces; 
    NetDeviceContainer currentContainer = m_devices[i];
	  
    newInterfaces.Add (ip.Assign (currentContainer.Get (0))); 
    newInterfaces.Add (ip.Assign (currentContainer.Get (1)));
	  
    auto interfaceAddress1 = newInterfaces.GetAddress (0);
    auto interfaceAddress2 = newInterfaces.GetAddress (1);
    uint32_t node1 = (currentContainer.Get (0))->GetNode()->GetId();
    uint32_t node2 = (currentContainer.Get (1))->GetNode()->GetId();

/*     if (m_systemId == 0)
      std::cout << i << "/" << m_devices.size () << "\n"; */
/* 	if (m_systemId == 0)
	  std::cout << "Node " << node1 << "(" << interfaceAddress1 << ") is connected with node  " 
                << node2 << "(" << interfaceAddress2 << ")\n"; */
				
	m_nodesConnectionsIps[node1].push_back(interfaceAddress2);
	m_nodesConnectionsIps[node2].push_back(interfaceAddress1);

    ip.NewNetwork ();
        
    m_interfaces.push_back (newInterfaces);
  }

  
/*   //Print the nodes' connections
  if (m_systemId == 0)
  {
    std::cout << "The nodes connections are:" << std::endl;
    for(auto &node : m_nodesConnectionsIps)
    {
  	  std::cout << "\nNode " << node.first << ":    " ;
	  for(std::vector<Ipv4Address>::const_iterator it = node.second.begin(); it != node.second.end(); it++)
	  {
        std::cout  << "\t" << *it ;
	  }
    }
    std::cout << "\n" << std::endl;
  } */
  
  tFinish = GetWallTime();
  if (m_systemId == 0)
    std::cout << "The Ip addresses have been assigned in " << tFinish - tStart << "s.\n";
}


Ptr<Node> 
BitcoinTopologyHelper::GetNode (uint32_t id)
{
  if (id > m_nodes.size () - 1 ) 
    {
      NS_FATAL_ERROR ("Index out of bounds in BitcoinTopologyHelper::GetNode.");
    }

  return (m_nodes.at (id)).Get (0);
}



Ipv4InterfaceContainer
BitcoinTopologyHelper::GetIpv4InterfaceContainer (void) const
{
  Ipv4InterfaceContainer ipv4InterfaceContainer;
  
  for (auto container = m_interfaces.begin(); container != m_interfaces.end(); container++)
    ipv4InterfaceContainer.Add(*container);

  return ipv4InterfaceContainer;
}


std::map<uint32_t, std::vector<Ipv4Address>> 
BitcoinTopologyHelper::GetNodesConnectionsIps (void) const
{
  return m_nodesConnectionsIps;
}


std::vector<uint32_t> 
BitcoinTopologyHelper::GetMiners (void) const
{
  return m_miners;
}

} // namespace ns3

static double GetWallTime()
{
    struct timeval time;
    if (gettimeofday(&time,NULL)){
        //  Handle error
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}
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

BitcoinTopologyHelper::BitcoinTopologyHelper (uint32_t noCpus, uint32_t totalNoNodes, uint32_t noMiners, enum BitcoinRegion *minersRegions,
                                              double bandwidth, int minConnectionsPerNode, int maxConnectionsPerNode,
						                      double latencyParetoMean, double latencyParetoShape, uint32_t systemId)
  : m_noCpus(noCpus), m_totalNoNodes (totalNoNodes), m_noMiners (noMiners), m_bandwidth (bandwidth), 
    m_minConnectionsPerNode (minConnectionsPerNode), m_maxConnectionsPerNode (maxConnectionsPerNode), 
	m_totalNoLinks (0), m_latencyParetoMean (latencyParetoMean), m_latencyParetoShape (latencyParetoShape), 
	m_systemId (systemId), m_minConnectionsPerMiner (1000), m_maxConnectionsPerMiner (1100),
	m_minerDownloadSpeed (100), m_minerUploadSpeed (100)
{
  
  std::vector<uint32_t>     nodes;    //nodes contain the ids of the nodes
  double                    tStart = GetWallTime();
  double                    tFinish;
  double regionLatencies[6][6] = { {35.5, 119.49, 254.79, 310.11, 154.36, 207.91},
	                               {119.49, 11.61, 221.08, 241.9, 266.45, 350.07},
	                               {254.79, 221.08, 137.09, 346.65, 255.95, 268.91},
	                               {310.11, 241.9, 346.65, 99.46, 172.24, 277.8},
	                               {154.36, 266.45, 255.95, 172.24, 8.76, 162.59},
	                               {207.91, 350.07, 268.91, 277.8, 162.59, 21.72}};
  for (int k = 0; k < 6; k++)
    for (int j = 0; j < 6; j++)
	  m_regionLatencies[k][j] = regionLatencies[k][j];
  
  m_regionBandwidths[NORTH_AMERICA] = 3.58;
  m_regionBandwidths[EUROPE] = 5.93;
  m_regionBandwidths[SOUTH_AMERICA] = 2.55;
  m_regionBandwidths[ASIA_PACIFIC] = 4.61;
  m_regionBandwidths[JAPAN] = 1.5;
  m_regionBandwidths[AUSTRALIA] = 5.2;
  
  srand (1000);

  // Bounds check
  if (m_noMiners > m_totalNoNodes)
  {
    NS_FATAL_ERROR ("The number of miners is larger than the total number of nodes\n");
  }
  
  if (m_noMiners < 1)
  {
    NS_FATAL_ERROR ("You need at least one miner\n");
  }

  m_bitcoinNodesRegion = new uint32_t[m_totalNoNodes];
  std::array<double,7> intervals {NORTH_AMERICA, EUROPE, SOUTH_AMERICA, ASIA_PACIFIC, JAPAN, AUSTRALIA, OTHER};
  std::array<double,6> weights {38.69, 51.59, 1.13, 5.74, 1.19, 1.66 };
                                
  m_nodesDistribution = std::piecewise_constant_distribution<double> (intervals.begin(), intervals.end(), weights.begin());
  
  m_minersRegions = new enum BitcoinRegion[m_noMiners];
  for (int i = 0; i < m_noMiners; i++)
  {
    m_minersRegions[i] = minersRegions[i];
  }
  
  /**
   * Create a vector containing all the nodes ids
   */
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

  sort(m_miners.begin(), m_miners.end());
  
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
	int minConnections;
	int maxConnections;
	
	if ( std::find(m_miners.begin(), m_miners.end(), i) != m_miners.end() )
	{
	  minConnections = m_minConnectionsPerMiner;
	  maxConnections = m_maxConnectionsPerMiner;
	}
	else
	{
	  minConnections = m_minConnectionsPerNode;
	  maxConnections = m_maxConnectionsPerNode;
	}
	
    while (m_nodesConnections[i].size() < minConnections && count < 2*minConnections)
    {
      uint32_t index = rand() % nodes.size();
	  uint32_t candidatePeer = nodes[index];
      int candidatesMaxConnections; 
		
	  if ( std::find(m_miners.begin(), m_miners.end(), candidatePeer) != m_miners.end() )
        maxConnections = m_maxConnectionsPerMiner;
	  else
        maxConnections = m_maxConnectionsPerNode;
   
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
      else if (m_nodesConnections[candidatePeer].size() >= maxConnections)
      {
/* 		if (m_systemId == 0)
          std::cout << "Node " << nodes[index] << " has already " << maxConnections << " connections" << "\n"; */
      }
      else
      {
        m_nodesConnections[i].push_back(candidatePeer);
        m_nodesConnections[candidatePeer].push_back(i);
		
        if (m_nodesConnections[candidatePeer].size() == maxConnections)
        {
/* 		  if (m_systemId == 0)
            std::cout << "Node " << nodes[index] << " is removed from index\n"; */
          nodes.erase(nodes.begin() + index);
        }
      }
      count++;
	}
	  
	if (m_nodesConnections[i].size() < minConnections && m_systemId == 0)
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
    std::cout << "The minimum number of connections for each node is " << m_minConnectionsPerNode 
              << " and whereas the maximum is " << m_maxConnectionsPerNode << ".\n";
  }
  
  
  InternetStackHelper stack;
  
  Ptr<ParetoRandomVariable> paretoDistribution = CreateObject<ParetoRandomVariable> ();
  paretoDistribution->SetAttribute ("Mean", DoubleValue (m_latencyParetoMean));
  paretoDistribution->SetAttribute ("Shape", DoubleValue (m_latencyParetoShape));
  
  std::ostringstream latencyStringStream; 
  std::ostringstream bandwidthStream;
  
  PointToPointHelper pointToPoint;
  
  tStart = GetWallTime();
  //Create the bitcoin nodes
  for (uint32_t i = 0; i < m_totalNoNodes; i++)
  {
    NodeContainer currentNode;
    currentNode.Create (1, i % m_noCpus);
/* 	if (m_systemId == 0)
      std::cout << "Creating a node with Id = " << i << " and systemId = " << i % m_noCpus << "\n"; */
    m_nodes.push_back (currentNode);
	AssignRegion(i);
    AssignInternetSpeeds(i);
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
		
		double bandwidth = std::min(std::min(m_nodesInternetSpeeds[m_nodes.at (node.first).Get (0)->GetId()].uploadSpeed, 
                                    m_nodesInternetSpeeds[m_nodes.at (node.first).Get (0)->GetId()].downloadSpeed),
                                    std::min(m_nodesInternetSpeeds[m_nodes.at (*it).Get (0)->GetId()].uploadSpeed, 
                                    m_nodesInternetSpeeds[m_nodes.at (*it).Get (0)->GetId()].downloadSpeed));					
		bandwidthStream.str("");
        bandwidthStream.clear();
		bandwidthStream << bandwidth << "Mbps";
		
        latencyStringStream.str("");
        latencyStringStream.clear();
        //latencyStringStream << paretoDistribution->GetValue() << "ms";
        latencyStringStream << m_regionLatencies[m_bitcoinNodesRegion[(m_nodes.at (node.first).Get (0))->GetId()]]
                                                [m_bitcoinNodesRegion[(m_nodes.at (*it).Get (0))->GetId()]] << "ms";
        
		pointToPoint.SetDeviceAttribute ("DataRate", StringValue (bandwidthStream.str()));
		pointToPoint.SetChannelAttribute ("Delay", StringValue (latencyStringStream.str()));
		
        newDevices.Add (pointToPoint.Install (m_nodes.at (node.first).Get (0), m_nodes.at (*it).Get (0)));
		m_devices.push_back (newDevices);
/* 		if (m_systemId == 0)
          std::cout << "Creating link " << m_totalNoLinks << " between nodes " 
                    << (m_nodes.at (node.first).Get (0))->GetId() << " (" 
                    <<  getBitcoinRegion(getBitcoinEnum(m_bitcoinNodesRegion[(m_nodes.at (node.first).Get (0))->GetId()]))
                    << ") and node " << (m_nodes.at (*it).Get (0))->GetId() << " (" 
                    <<  getBitcoinRegion(getBitcoinEnum(m_bitcoinNodesRegion[(m_nodes.at (*it).Get (0))->GetId()]))
                    << ") with latency = " << latencyStringStream.str() 
                    << " and bandwidth = " << bandwidthStream.str() << ".\n"; */
      }
    }
  }
  
  tFinish = GetWallTime();

  if (m_systemId == 0)
    std::cout << "The total number of links is " << m_totalNoLinks << " (" << tFinish - tStart << "s).\n";
}

BitcoinTopologyHelper::~BitcoinTopologyHelper ()
{
  delete[] m_bitcoinNodesRegion;
  delete[] m_minersRegions;
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
	
	m_peersDownloadSpeeds[node1][interfaceAddress2] = m_nodesInternetSpeeds[node2].downloadSpeed;
	m_peersDownloadSpeeds[node2][interfaceAddress1] = m_nodesInternetSpeeds[node1].downloadSpeed;

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

void
BitcoinTopologyHelper::AssignRegion (uint32_t id)
{
  auto index = std::find(m_miners.begin(), m_miners.end(), id);
  if ( index != m_miners.end() )
  {
    m_bitcoinNodesRegion[id] = m_minersRegions[index - m_miners.begin()];
  }
  else{
    int number = m_nodesDistribution(m_generator); 
    m_bitcoinNodesRegion[id] = number;
  }
  
/*   if (m_systemId == 0)
    std::cout << "SystemId = " << m_systemId << " assigned node " << id << " in " << getBitcoinRegion(getBitcoinEnum(m_bitcoinNodesRegion[id])) << "\n"; */
}


void 
BitcoinTopologyHelper::AssignInternetSpeeds(uint32_t id)
{
  auto index = std::find(m_miners.begin(), m_miners.end(), id);
  if ( index != m_miners.end() )
  {
    m_nodesInternetSpeeds[id].downloadSpeed = m_minerDownloadSpeed;
    m_nodesInternetSpeeds[id].uploadSpeed = m_minerUploadSpeed;
  }
  else{
    m_nodesInternetSpeeds[id].downloadSpeed = m_regionBandwidths[m_bitcoinNodesRegion[id]];
    m_nodesInternetSpeeds[id].uploadSpeed = m_regionBandwidths[m_bitcoinNodesRegion[id]];
  }
}


uint32_t* 
BitcoinTopologyHelper::GetBitcoinNodesRegions (void)
{
  return m_bitcoinNodesRegion;
}


std::map<uint32_t, std::map<Ipv4Address, double>> 
BitcoinTopologyHelper::GetPeersDownloadSpeeds (void) const
{
  return m_peersDownloadSpeeds;
}


std::map<uint32_t, nodeInternetSpeeds> 
BitcoinTopologyHelper::GetNodesInternetSpeeds (void) const
{
  return m_nodesInternetSpeeds;
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
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
 */

#include <fstream>
#include <time.h>
#include <sys/time.h>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/mpi-interface.h"

#ifdef NS3_MPI
#include <mpi.h>
#endif

using namespace ns3;

double get_wall_time();
int GetNodeIdByIpv4 (Ipv4InterfaceContainer container, Ipv4Address addr);
void PrintStatsForEachNode (nodeStatistics *stats, int totalNodes);
void PrintTotalStats (nodeStatistics *stats, int totalNodes);

NS_LOG_COMPONENT_DEFINE ("MyMpiTest");

int 
main (int argc, char *argv[])
{
#ifdef NS3_MPI
  bool nullmsg = false;
  double tStart = get_wall_time(), tFinish;
  const int secsPerMin = 60;
  const uint16_t bitcoinPort = 8333;
  const double realAverageBlockGenIntervalMinutes = 10; //minutes
  int targetNumberOfBlocks = 8;
  double averageBlockGenIntervalSeconds = 10 * secsPerMin; //seconds
  double fixedHashRate = 0.5;
  int start = 0;
  bool test = true;
  
  int xSize = 1;
  int ySize = 2;
  int minConnectionsPerNode = 1;
  int maxConnectionsPerNode = 1;
  int noMiners = 2;
  double minersHash[] = {0.6, 0.4};

  
  int totalNoNodes = xSize * ySize;
  nodeStatistics stats[totalNoNodes];
  double averageBlockGenIntervalMinutes = averageBlockGenIntervalSeconds/secsPerMin;
  double stop = targetNumberOfBlocks * averageBlockGenIntervalMinutes; //seconds
  double blockGenBinSize = 1./secsPerMin/1000;					       //minutes
  double blockGenParameter = 0.19 * blockGenBinSize / 2 * (realAverageBlockGenIntervalMinutes / averageBlockGenIntervalMinutes);	//0.19 for blockGenBinSize = 2mins

  std::map<int, Ipv4Address>                 miners; // key = nodeId
  std::map<int, std::vector<Ipv4Address>>    nodesConnections; // key = nodeId
  Ipv4InterfaceContainer                     ipv4InterfaceContainer;
  int                                        nodesInSystemId0 = 0;
  srand (1000);
  Time::SetResolution (Time::NS);
  
  CommandLine cmd;
  cmd.AddValue ("nullmsg", "Enable the use of null-message synchronization", nullmsg);
  cmd.Parse(argc, argv);

  // Distributed simulation setup; by default use granted time window algorithm.
  if(nullmsg) 
    {
      GlobalValue::Bind ("SimulatorImplementationType",
                         StringValue ("ns3::NullMessageSimulatorImpl"));
    } 
  else 
    {
      GlobalValue::Bind ("SimulatorImplementationType",
                         StringValue ("ns3::DistributedSimulatorImpl"));
    }

  // Enable parallel simulator with the command line arguments
  MpiInterface::Enable (&argc, &argv);
  uint32_t systemId = MpiInterface::GetSystemId ();
  uint32_t systemCount = MpiInterface::GetSize ();
  
  LogComponentEnable("BitcoinNode", LOG_LEVEL_WARN);
  LogComponentEnable("BitcoinMiner", LOG_LEVEL_DEBUG);
  LogComponentEnable("BitcoinSimpleAttacker", LOG_LEVEL_DEBUG);
  
  //LogComponentEnable("ObjectFactory", LOG_LEVEL_FUNCTION);
  //LogComponentEnable("Ipv4AddressGenerator", LOG_LEVEL_INFO);
  //LogComponentEnable("OnOffApplication", LOG_LEVEL_DEBUG);
  //LogComponentEnable("OnOffApplication", LOG_LEVEL_WARN);

  if (noMiners > totalNoNodes)
  {
    std::cout << "The number of miners is larger than the total number of nodes\n";
    return 0;
  }
  
  if (noMiners < 1)
  {
    std::cout << "You need at least one miner\n";
    return 0;
  }
  
  if (sizeof(minersHash)/sizeof(double) != noMiners)
  {
    std::cout << "The minersHash entries does not match the number of miners\n";
    return 0;
  }
  
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("8Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ms"));

  // Create Grid
  PointToPointGridHelperCustom grid (xSize, ySize, systemCount, pointToPoint);
  grid.BoundingBox(100, 100, 200, 200);
  
  if (systemId == 0 )
    std::cout << totalNoNodes << " nodes created.\n";

  // Install stack on Grid
  InternetStackHelper stack;
  grid.InstallStack (stack);

  // Assign Addresses to Grid
  grid.AssignIpv4Addresses (Ipv4AddressHelper ("1.0.0.0", "255.255.255.0"),
                            Ipv4AddressHelper ("61.0.0.0", "255.255.255.0"));
  ipv4InterfaceContainer = grid.GetIpv4InterfaceContainer();
  
  if (systemId == 0 )
    std::cout << "Ipv4 addresses were assigned.\n";

  {
    //nodes contain the ids of the nodes
    std::vector<int> nodes;

    for (int i = 0; i < totalNoNodes; i++)
    {
      nodes.push_back(i);
	}

/* 	//print the initialized nodes
	for (std::vector<int>::iterator j = nodes.begin(); j != nodes.end(); j++)
	{
	  std::cout << *j << " " ;
	} */

	//Choose the miners randomly. They should be unique (no miner should be chosen twice).
	//So, remove each chose miner from nodes vector
    for (int i = 0; i < noMiners; i++)
	{
      int index = rand() % nodes.size();
      miners[nodes[index]] = grid.GetIpv4Address (nodes[index] / ySize, nodes[index] % ySize);
      //std::cout << "\n" << "Chose " << nodes[index] << "     ";
      nodes.erase(nodes.begin() + index);
	  
/* 	  for (std::vector<int>::iterator it = nodes.begin(); it != nodes.end(); it++)
	  {
	    std::cout << *it << " " ;
	  } */
	}
  }
  
/*   //Print the miners
  std::cout << "\n\nThe miners are:\n" << std::endl;
  for(auto &miner : miners)
  {
    std::cout << miner.first << "\t" << miner.second << "\n";
  }
  std::cout << std::endl; */

  //Interconnect the miners
  for(auto &miner : miners)
  {
    for(auto &peer : miners)
    {
      if (peer.first != miner.first)
        nodesConnections[miner.first].push_back(peer.second);
	}
  }
  
/*   //Print the miners' connections
  std::cout << "The miners are interconnected:" << std::endl;
  for(auto &miner : nodesConnections)
  {
	std::cout << "\nMiner " << miner.first << ":\t" ;
	for(std::vector<Ipv4Address>::const_iterator it = miner.second.begin(); it != miner.second.end(); it++)
	{
      std::cout << GetNodeIdByIpv4(ipv4InterfaceContainer, *it) << "\t" ;
	}
  }
  std::cout << "\n" << std::endl; */
  
  
  //Interconnect the nodes
  {
    //nodes contain the ids of the nodes
    std::vector<int> nodes;

    for (int i = 0; i < totalNoNodes; i++)
    {
      nodes.push_back(i);
	}
	
    for(int i = 0; i < totalNoNodes; i++)
    {
	  int count = 0;
      while (nodesConnections[i].size() < minConnectionsPerNode && count < 2*minConnectionsPerNode)
      {
        int index = rand() % nodes.size();
	    Ipv4Address candidatePeer = grid.GetIpv4Address (nodes[index] / ySize, nodes[index] % ySize);
		
        if (nodes[index] == i)
		{
          //std::cout << "Node " << i << " does not need a connection with itself" << "\n";
		}
		else if (std::find(nodesConnections[i].begin(), nodesConnections[i].end(), candidatePeer) != nodesConnections[i].end())
		{
          //std::cout << "Node " << i << " has already a connection to Node " << nodes[index] << "\n";
		}
        else if (nodesConnections[nodes[index]].size() >= maxConnectionsPerNode)
		{
          //std::cout << "Node " << nodes[index] << " has already " << maxConnectionsPerNode << " connections" << "\n";
		}
		else
		{
		  nodesConnections[i].push_back(candidatePeer);
		  nodesConnections[nodes[index]].push_back(grid.GetIpv4Address (i / ySize, i % ySize));
		  if (nodesConnections[nodes[index]].size() == maxConnectionsPerNode)
		  {
			//std::cout << "Node " << nodes[index] << " is removed from index\n";
		    nodes.erase(nodes.begin() + index);
		  }
		}
		count++;
	  }
	  
	  if (nodesConnections[i].size() < minConnectionsPerNode)
	    std::cout << "Node " << i << " has only " << nodesConnections[i].size() << " connections\n";
	  if (nodes[0] == i)
	    nodes.erase(nodes.begin());
    }
	//return 0;
/*     //Print the nodes' connections
    std::cout << "The nodes connections are:" << std::endl;
    for(auto &node : nodesConnections)
    {
  	  std::cout << "\nNode " << node.first << ":    " ;
	  for(std::vector<Ipv4Address>::const_iterator it = node.second.begin(); it != node.second.end(); it++)
	  {
        std::cout  << "\t" << *it;//GetNodeIdByIpv4(ipv4InterfaceContainer, *it) ;
	  }
    }
    std::cout << "\n" << std::endl; */
  }
  
  if (systemId == 0 )
  {
    std::cout << "The nodes connections were created.\n";
    std::cout << "minConnectionsPerNode = " << minConnectionsPerNode 
	          << " and maxConnectionsPerNode = " << maxConnectionsPerNode << "\n";
  }
  
  //Install miners
  BitcoinMinerHelper bitcoinMinerHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), bitcoinPort),
                                          nodesConnections[miners.begin()-> first], stats, minersHash[0], 
                                          blockGenBinSize, blockGenParameter, averageBlockGenIntervalSeconds);
  ApplicationContainer bitcoinMiners;
  int count = 0;
  if (test == true)
    bitcoinMinerHelper.SetAttribute("FixedBlockIntervalGeneration", DoubleValue(600));

  for(auto &miner : miners)
  {
	Ptr<Node> targetNode = grid.GetNode (miner.first / ySize, miner.first % ySize);
	
	if (systemId == targetNode->GetSystemId())
	{
      bitcoinMinerHelper.SetAttribute("HashRate", DoubleValue(minersHash[count]));
	  bitcoinMinerHelper.SetPeersAddresses (nodesConnections[miner.first]);
	  bitcoinMinerHelper.SetNodeStats (&stats[miner.first]);
	  bitcoinMiners.Add(bitcoinMinerHelper.Install (targetNode));
      std::cout << "SystemId " << systemId << ": Miner " << miner.first << " with hash power = " << minersHash[count] 
	            << " and systemId = " << targetNode->GetSystemId() << " was installed in node (" 
                << miner.first / ySize << ", " << miner.first % ySize << ")" << std::endl; 
	  
	  if (systemId == 0)
        nodesInSystemId0++;
	}				
	count++;

    bitcoinMinerHelper.SetMinerType (SIMPLE_ATTACKER);
	if (test == true)
	  bitcoinMinerHelper.SetAttribute("FixedBlockIntervalGeneration", DoubleValue(100));
  }
  bitcoinMiners.Start (Seconds (start));
  bitcoinMiners.Stop (Minutes (stop));

  
  //Install simple nodes
  BitcoinNodeHelper bitcoinNodeHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), bitcoinPort), nodesConnections[0], stats);
  ApplicationContainer bitcoinNodes;
  
  for(auto &node : nodesConnections)
  {
    Ptr<Node> targetNode = grid.GetNode (node.first / ySize, node.first % ySize);
	
	if (systemId == targetNode->GetSystemId())
	{
      auto it = miners.find(node.first);
	  if ( it == miners.end())
	  {
	    bitcoinNodeHelper.SetPeersAddresses (node.second);
		bitcoinNodeHelper.SetNodeStats (&stats[node.first]);
	    bitcoinNodes.Add(bitcoinNodeHelper.Install (targetNode));
        std::cout << "SystemId " << systemId << ": Node " << node.first << " with systemId = " << targetNode->GetSystemId() 
		          << " was installed in node (" << node.first / ySize << ", " 
				  << node.first % ySize << ")"<< std::endl;
				  
	    if (systemId == 0)
          nodesInSystemId0++;
	  }	
	}	  
  }
  bitcoinNodes.Start (Seconds (start));
  bitcoinNodes.Stop (Minutes (stop));
  
  
  // Set up the actual simulation
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  
  Simulator::Stop (Minutes (stop + 0.1));
  Simulator::Run ();
  Simulator::Destroy ();
 

  
  if (systemId == 0)
  {
    tFinish=get_wall_time();
	
    //PrintStatsForEachNode(stats, totalNoNodes);
    //PrintTotalStats(stats, totalNoNodes);
	if (stats[1].attackSuccess == 1)
      std::cout << "SUCCESS!\n" << std::endl;
    else
      std::cout << "FAIL\n" << std::endl;

    std::cout << "\nThe simulation ran for " << tFinish - tStart << "s simulating "
              << stop << "mins. Performed " << stop * secsPerMin / (tFinish - tStart)
              << " faster than realtime.\n" << "It consisted of " << totalNoNodes
              << " nodes (" << noMiners << " miners) with minConnectionsPerNode = "
              << minConnectionsPerNode << " and maxConnectionsPerNode = " << maxConnectionsPerNode 
              << ".\nThe averageBlockGenIntervalMinutes was " << averageBlockGenIntervalMinutes << "min.\n";

  }  
  
  
  // Exit the MPI execution environment
  MpiInterface::Disable ();
  return 0;
  
#else
  NS_FATAL_ERROR ("Can't use distributed simulator without MPI compiled in");
#endif
}

double get_wall_time()
{
    struct timeval time;
    if (gettimeofday(&time,NULL)){
        //  Handle error
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

int GetNodeIdByIpv4 (Ipv4InterfaceContainer container, Ipv4Address addr)
{
  for (auto it = container.Begin(); it != container.End(); it++)
  {
	int32_t interface = it->first->GetInterfaceForAddress (addr);
	if ( interface != -1)
      return it->first->GetNetDevice (interface)-> GetNode()->GetId();
  }	  
  return -1; //if not found
}

void PrintStatsForEachNode (nodeStatistics *stats, int totalNodes)
{
  int secPerMin = 60;
  
  for (int it = 0; it < totalNodes; it++ )
  {
    std::cout << "\nNode " << stats[it].nodeId << " statistics:\n";
    std::cout << "Mean Block Receive Time = " << stats[it].meanBlockReceiveTime << " or " 
              << static_cast<int>(stats[it].meanBlockReceiveTime) / secPerMin << "min and " 
			  << stats[it].meanBlockReceiveTime - static_cast<int>(stats[it].meanBlockReceiveTime) / secPerMin * secPerMin << "s\n";
    std::cout << "Mean Block Propagation Time = " << stats[it].meanBlockPropagationTime << "s\n";
    std::cout << "Mean Block Size = " << stats[it].meanBlockSize << " Bytes\n";
    std::cout << "Total Blocks = " << stats[it].totalBlocks << "\n";
    std::cout << "Stale Blocks = " << stats[it].staleBlocks << " (" 
              << 100. * stats[it].staleBlocks / stats[it].totalBlocks << "%)\n";
	
    if ( stats[it].miner == 1)
    {
      std::cout << "The miner " << stats[it].nodeId << " with hash rate = " << stats[it].hashRate*100 << "% generated " << stats[it].minerGeneratedBlocks 
                << " blocks "<< "(" << 100. * stats[it].minerGeneratedBlocks / (stats[it].totalBlocks - 1)
                << "%) with average block generation time = " << stats[it].minerAverageBlockGenInterval
                << "s or " << static_cast<int>(stats[it].minerAverageBlockGenInterval) / secPerMin << "min and " 
                << stats[it].minerAverageBlockGenInterval - static_cast<int>(stats[it].minerAverageBlockGenInterval) / secPerMin * secPerMin << "s"
                << " and average size " << stats[it].minerAverageBlockSize << " Bytes\n";
    }
  }
}


void PrintTotalStats (nodeStatistics *stats, int totalNodes)
{
  const int  secPerMin = 60;
  double     meanBlockReceiveTime = 0;
  double     meanBlockPropagationTime = 0;
  double     meanBlockSize = 0;
  int        totalBlocks = 0;
  int        staleBlocks = 0;
  
  for (int it = 0; it < totalNodes; it++ )
  {
    meanBlockReceiveTime = meanBlockReceiveTime*totalBlocks/(totalBlocks + stats[it].totalBlocks)
                         + stats[it].meanBlockReceiveTime*stats[it].totalBlocks/(totalBlocks + stats[it].totalBlocks);
    meanBlockPropagationTime = meanBlockPropagationTime*totalBlocks/(totalBlocks + stats[it].totalBlocks)
                         + stats[it].meanBlockPropagationTime*stats[it].totalBlocks/(totalBlocks + stats[it].totalBlocks);
    meanBlockSize = meanBlockSize*totalBlocks/(totalBlocks + stats[it].totalBlocks)
                         + stats[it].meanBlockSize*stats[it].totalBlocks/(totalBlocks + stats[it].totalBlocks);
    totalBlocks += stats[it].totalBlocks;
    staleBlocks += stats[it].staleBlocks;
  }
  
  totalBlocks /= static_cast<double>(totalNodes);
  staleBlocks /= static_cast<double>(totalNodes);

  std::cout << "\nTotal Stats:\n";
  std::cout << "Mean Block Receive Time = " << meanBlockReceiveTime << " or " 
            << static_cast<int>(meanBlockReceiveTime) / secPerMin << "min and " 
	        << meanBlockReceiveTime - static_cast<int>(meanBlockReceiveTime) / secPerMin * secPerMin << "s\n";
  std::cout << "Mean Block Propagation Time = " << meanBlockPropagationTime << "s\n";
  std::cout << "Mean Block Size = " << meanBlockSize << " Bytes\n";
  std::cout << "Total Blocks = " << totalBlocks << "\n";
  std::cout << "Stale Blocks = " << staleBlocks << " (" 
            << 100. * staleBlocks / totalBlocks << "%)\n";
}
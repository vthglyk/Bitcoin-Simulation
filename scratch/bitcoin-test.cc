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
#define MPI_TEST

#ifdef NS3_MPI
#include <mpi.h>
#endif

using namespace ns3;

double get_wall_time();
int GetNodeIdByIpv4 (Ipv4InterfaceContainer container, Ipv4Address addr);
void PrintStatsForEachNode (nodeStatistics *stats, int totalNodes);
void PrintTotalStats (nodeStatistics *stats, int totalNodes, double start, double finish);
void PrintBitcoinRegionStats (uint32_t *bitcoinNodesRegions, uint32_t totalNodes);

NS_LOG_COMPONENT_DEFINE ("MyMpiTest");

int 
main (int argc, char *argv[])
{
#ifdef NS3_MPI
  bool nullmsg = false;
  double tStart = get_wall_time(), tStartSimulation, tFinish;
  const int secsPerMin = 60;
  const uint16_t bitcoinPort = 8333;
  const double realAverageBlockGenIntervalMinutes = 10; //minutes
  int targetNumberOfBlocks = 100;
  double averageBlockGenIntervalSeconds = 10 * secsPerMin; //seconds
  double fixedHashRate = 0.5;
  int start = 0;
  double bandwidth = 8;
  double latency = 40;
  bool testScalability = false;

  
  int totalNoNodes = 4;

  int minConnectionsPerNode = 2;
  int maxConnectionsPerNode = 3;
#ifdef MPI_TEST
  int noMiners = 16;
  double minersHash[] = {0.289, 0.196, 0.159, 0.133, 0.066, 0.054,
                         0.029, 0.016, 0.012, 0.012, 0.012, 0.009,
                         0.005, 0.005, 0.002, 0.002};
  enum BitcoinRegion minersRegions[] = {ASIA_PACIFIC, ASIA_PACIFIC, ASIA_PACIFIC, NORTH_AMERICA, ASIA_PACIFIC, NORTH_AMERICA,
                                        EUROPE, EUROPE, NORTH_AMERICA, NORTH_AMERICA, NORTH_AMERICA, EUROPE,
                                        NORTH_AMERICA, NORTH_AMERICA, NORTH_AMERICA, NORTH_AMERICA};
                          
#else
  int noMiners = 3;
  double minersHash[] = {0.4, 0.3, 0.3};
  enum BitcoinRegion minersRegions[] = {ASIA_PACIFIC, ASIA_PACIFIC, ASIA_PACIFIC};
#endif

  double averageBlockGenIntervalMinutes = averageBlockGenIntervalSeconds/secsPerMin;
  double stop;


  Ipv4InterfaceContainer                               ipv4InterfaceContainer;
  std::map<uint32_t, std::vector<Ipv4Address>>         nodesConnections;
  std::map<uint32_t, std::map<Ipv4Address, double>>    nodesBandwidths;
  std::vector<uint32_t>                                miners;
  int                                                  nodesInSystemId0 = 0;
  
  Time::SetResolution (Time::NS);
  
  CommandLine cmd;
  cmd.AddValue ("nullmsg", "Enable the use of null-message synchronization", nullmsg);
  cmd.AddValue ("bandwidth", "The bandwidth of the nodes (Mbps)", bandwidth);
  cmd.AddValue ("latency", "The latency of the nodes (ms)", latency);
  cmd.AddValue ("noBlocks", "The number of generated blocks", targetNumberOfBlocks);
  cmd.AddValue ("nodes", "The total number of nodes in the network", totalNoNodes);
  cmd.AddValue ("minConnections", "The minConnectionsPerNode of the grid", minConnectionsPerNode);
  cmd.AddValue ("maxConnections", "The maxConnectionsPerNode of the grid", maxConnectionsPerNode);
  cmd.AddValue ("blockIntervalMinutes", "The average block generation interval in minutes", averageBlockGenIntervalMinutes);
  cmd.AddValue ("test", "Test the scalability of the simulation", testScalability);

  cmd.Parse(argc, argv);
  
  averageBlockGenIntervalSeconds = averageBlockGenIntervalMinutes * secsPerMin;
  stop = targetNumberOfBlocks * averageBlockGenIntervalMinutes; //seconds
  nodeStatistics *stats = new nodeStatistics[totalNoNodes];
  averageBlockGenIntervalMinutes = averageBlockGenIntervalSeconds/secsPerMin;

#ifdef MPI_TEST
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
#else
  uint32_t systemId = 0;
  uint32_t systemCount = 1;
#endif

/*   LogComponentEnable("BitcoinNode", LOG_LEVEL_INFO);
  LogComponentEnable("BitcoinMiner", LOG_LEVEL_DEBUG); */
  //LogComponentEnable("Ipv4AddressGenerator", LOG_LEVEL_FUNCTION);
  //LogComponentEnable("OnOffApplication", LOG_LEVEL_DEBUG);
  //LogComponentEnable("OnOffApplication", LOG_LEVEL_WARN);

  
  if (sizeof(minersHash)/sizeof(double) != noMiners)
  {
    std::cout << "The minersHash entries does not match the number of miners\n";
    return 0;
  }
 
  BitcoinTopologyHelper bitcoinTopologyHelper (systemCount, totalNoNodes, noMiners, minersRegions,
                                               bandwidth, minConnectionsPerNode, 
                                               maxConnectionsPerNode, latency, 2, systemId);

  // Install stack on Grid
  InternetStackHelper stack;
  bitcoinTopologyHelper.InstallStack (stack);

  // Assign Addresses to Grid
  bitcoinTopologyHelper.AssignIpv4Addresses (Ipv4AddressHelperCustom ("1.0.0.0", "255.255.255.0", false));
  ipv4InterfaceContainer = bitcoinTopologyHelper.GetIpv4InterfaceContainer();
  nodesConnections = bitcoinTopologyHelper.GetNodesConnectionsIps();
  miners = bitcoinTopologyHelper.GetMiners();
  nodesBandwidths = bitcoinTopologyHelper.GetNodesBandwidths();
  if (systemId == 0)
    PrintBitcoinRegionStats(bitcoinTopologyHelper.GetBitcoinNodesRegions(), totalNoNodes);
											   
  //Install miners
  BitcoinMinerHelper bitcoinMinerHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), bitcoinPort),
                                          nodesConnections[miners[0]], nodesBandwidths[0], stats, minersHash[0], 
                                          averageBlockGenIntervalSeconds);
  ApplicationContainer bitcoinMiners;
  int count = 0;
  if (testScalability == true)
    bitcoinMinerHelper.SetAttribute("FixedBlockIntervalGeneration", DoubleValue(600));

  for(auto &miner : miners)
  {
	Ptr<Node> targetNode = bitcoinTopologyHelper.GetNode (miner);
	
	if (systemId == targetNode->GetSystemId())
	{
      bitcoinMinerHelper.SetAttribute("HashRate", DoubleValue(minersHash[count]));
	  bitcoinMinerHelper.SetPeersAddresses (nodesConnections[miner]);
	  bitcoinMinerHelper.SetNodeBandwidths (nodesBandwidths[miner]);
	  bitcoinMinerHelper.SetNodeStats (&stats[miner]);
	  bitcoinMiners.Add(bitcoinMinerHelper.Install (targetNode));
/*       std::cout << "SystemId " << systemId << ": Miner " << miner << " with hash power = " << minersHash[count] 
	            << " and systemId = " << targetNode->GetSystemId() << " was installed in node " 
                << targetNode->GetId () << std::endl;  */
	  
	  if (systemId == 0)
        nodesInSystemId0++;
	}				
	count++;
	if (testScalability == true)
	  bitcoinMinerHelper.SetAttribute("FixedBlockIntervalGeneration", DoubleValue(1000));

  }
  bitcoinMiners.Start (Seconds (start));
  bitcoinMiners.Stop (Minutes (stop));

  
  //Install simple nodes
  BitcoinNodeHelper bitcoinNodeHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), bitcoinPort), nodesConnections[0], nodesBandwidths[0], stats);
  ApplicationContainer bitcoinNodes;
  
  for(auto &node : nodesConnections)
  {
    Ptr<Node> targetNode = bitcoinTopologyHelper.GetNode (node.first);
	
	if (systemId == targetNode->GetSystemId())
	{
  
      if ( std::find(miners.begin(), miners.end(), node.first) == miners.end() )
	  {
	    bitcoinNodeHelper.SetPeersAddresses (node.second);
	    bitcoinNodeHelper.SetNodeBandwidths (nodesBandwidths[node.first]);
		bitcoinNodeHelper.SetNodeStats (&stats[node.first]);
	    bitcoinNodes.Add(bitcoinNodeHelper.Install (targetNode));
/*         std::cout << "SystemId " << systemId << ": Node " << node.first << " with systemId = " << targetNode->GetSystemId() 
		          << " was installed in node " << targetNode->GetId () <<  std::endl; */
				  
	    if (systemId == 0)
          nodesInSystemId0++;
	  }	
	}	  
  }
  bitcoinNodes.Start (Seconds (start));
  bitcoinNodes.Stop (Minutes (stop));
  
  if (systemId == 0)
    std::cout << "The applications have been setup.\n";
  
  // Set up the actual simulation
  //Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  tStartSimulation = get_wall_time();
  if (systemId == 0)
    std::cout << "Setup time = " << tStartSimulation - tStart << "s\n";
  Simulator::Stop (Minutes (stop + 0.1));
  Simulator::Run ();
  Simulator::Destroy ();

#ifdef MPI_TEST

  int            blocklen[12] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}; 
  MPI_Aint       disp[12]; 
  MPI_Datatype   dtypes[12] = {MPI_INT, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_INT}; 
  MPI_Datatype   mpi_nodeStatisticsType;

  disp[0] = offsetof(nodeStatistics, nodeId);
  disp[1] = offsetof(nodeStatistics, meanBlockReceiveTime);
  disp[2] = offsetof(nodeStatistics, meanBlockPropagationTime);
  disp[3] = offsetof(nodeStatistics, meanBlockSize);
  disp[4] = offsetof(nodeStatistics, totalBlocks);
  disp[5] = offsetof(nodeStatistics, staleBlocks);
  disp[6] = offsetof(nodeStatistics, miner);
  disp[7] = offsetof(nodeStatistics, minerGeneratedBlocks);
  disp[8] = offsetof(nodeStatistics, minerAverageBlockGenInterval);
  disp[9] = offsetof(nodeStatistics, minerAverageBlockSize);
  disp[10] = offsetof(nodeStatistics, hashRate);
  disp[11] = offsetof(nodeStatistics, attackSuccess);

  MPI_Type_create_struct (12, blocklen, disp, dtypes, &mpi_nodeStatisticsType);
  MPI_Type_commit (&mpi_nodeStatisticsType);

  if (systemId != 0 && systemCount > 1)
  {
    /**
     * Sent all the systemId stats to systemId == 0
	 */
	/* std::cout << "SystemId = " << systemId << "\n"; */

    for(int i = 0; i < totalNoNodes; i++)
    {
      Ptr<Node> targetNode = bitcoinTopologyHelper.GetNode (i);
	
	  if (systemId == targetNode->GetSystemId())
	  {
        MPI_Send(&stats[i], 1, mpi_nodeStatisticsType, 0, 8888, MPI_COMM_WORLD);
	  }
    }
  }
  else if (systemId == 0 && systemCount > 1)
  {
    int count = nodesInSystemId0;
	
	while (count < totalNoNodes)
	{
	  MPI_Status status;
      nodeStatistics recv;
	  
	  /* std::cout << "SystemId = " << systemId << "\n"; */
	  MPI_Recv(&recv, 1, mpi_nodeStatisticsType, MPI_ANY_SOURCE, 8888, MPI_COMM_WORLD, &status);
    
/* 	  std::cout << "SystemId 0 received: statistics for node " << recv.nodeId 
                <<  " from systemId = " << status.MPI_SOURCE << "\n"; */
      stats[recv.nodeId].nodeId = recv.nodeId;
      stats[recv.nodeId].meanBlockReceiveTime = recv.meanBlockReceiveTime;
      stats[recv.nodeId].meanBlockPropagationTime = recv.meanBlockPropagationTime;
      stats[recv.nodeId].meanBlockSize = recv.meanBlockSize;
      stats[recv.nodeId].totalBlocks = recv.totalBlocks;
      stats[recv.nodeId].staleBlocks = recv.staleBlocks;
      stats[recv.nodeId].miner = recv.miner;
      stats[recv.nodeId].minerGeneratedBlocks = recv.minerGeneratedBlocks;
      stats[recv.nodeId].minerAverageBlockGenInterval = recv.minerAverageBlockGenInterval;
      stats[recv.nodeId].minerAverageBlockSize = recv.minerAverageBlockSize;
      stats[recv.nodeId].hashRate = recv.hashRate;
	  count++;
    }
  }	  
#endif

  if (systemId == 0)
  {
    tFinish=get_wall_time();
	
    //PrintStatsForEachNode(stats, totalNoNodes);
    PrintTotalStats(stats, totalNoNodes, tStartSimulation, tFinish);
    std::cout << "\nThe simulation ran for " << tFinish - tStart << "s simulating "
              << stop << "mins. Performed " << stop * secsPerMin / (tFinish - tStart)
              << " faster than realtime.\n" << "Setup time = " << tStartSimulation - tStart << "s\n"
              <<"It consisted of " << totalNoNodes << " nodes (" << noMiners << " miners) with minConnectionsPerNode = "
              << minConnectionsPerNode << " and maxConnectionsPerNode = " << maxConnectionsPerNode 
              << ".\nThe averageBlockGenIntervalMinutes was " << averageBlockGenIntervalMinutes << "min."
			  << "\nThe bandwidth of the nodes was " << bandwidth
			  << "Mbps.\nThe latency of the nodes was " << latency << "s.\n";

  }  
  
#ifdef MPI_TEST

  // Exit the MPI execution environment
  MpiInterface::Disable ();
#endif

  delete[] stats;
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


void PrintTotalStats (nodeStatistics *stats, int totalNodes, double start, double finish)
{
  const int  secPerMin = 60;
  double     meanBlockReceiveTime = 0;
  double     meanBlockPropagationTime = 0;
  double     meanBlockSize = 0;
  int        totalBlocks = 0;
  int        staleBlocks = 0;
  
  std::vector<double>    propagationTimes;
  
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
	
	propagationTimes.push_back(stats[it].meanBlockPropagationTime);
  }
  
  
  totalBlocks /= static_cast<double>(totalNodes);
  staleBlocks /= static_cast<double>(totalNodes);
  sort(propagationTimes.begin(), propagationTimes.end());
  double median = *(propagationTimes.begin()+propagationTimes.size()/2);
  double p_90 = *(propagationTimes.begin()+int(propagationTimes.size()*.9));
  
  std::cout << "\nTotal Stats:\n";
  std::cout << "Mean Block Receive Time = " << meanBlockReceiveTime << " or " 
            << static_cast<int>(meanBlockReceiveTime) / secPerMin << "min and " 
	        << meanBlockReceiveTime - static_cast<int>(meanBlockReceiveTime) / secPerMin * secPerMin << "s\n";
  std::cout << "Mean Block Propagation Time = " << meanBlockPropagationTime << "s\n";
  std::cout << "Median Block Propagation Time = " << median << "s\n";
  std::cout << "90% percentile of Block Propagation Time = " << p_90 << "s\n";
  std::cout << "Mean Block Size = " << meanBlockSize << " Bytes\n";
  std::cout << "Total Blocks = " << totalBlocks << "\n";
  std::cout << "Stale Blocks = " << staleBlocks << " (" 
            << 100. * staleBlocks / totalBlocks << "%)\n";
  std::cout << (finish - start)/ (totalBlocks - 1)<< "s per generated block\n";
}

void PrintBitcoinRegionStats (uint32_t *bitcoinNodesRegions, uint32_t totalNodes)
{
  uint32_t regions[7] = {0, 0, 0, 0, 0, 0, 0};
  
  for (uint32_t i = 0; i < totalNodes; i++)
    regions[bitcoinNodesRegions[i]]++;
  
  std::cout << "Nodes distribution: \n";
  for (uint32_t i = 0; i < 7; i++)
  {
    std::cout << getBitcoinRegion(getBitcoinEnum(i)) << ": " << regions[i] * 100.0 / totalNodes << "%\n";
  }
}
	
	
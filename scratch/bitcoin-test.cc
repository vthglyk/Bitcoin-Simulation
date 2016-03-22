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
void PrintTotalStats (nodeStatistics *stats, int totalNodes, double start, double finish, double averageBlockGenIntervalMinutes, bool relayNetwork);
void PrintBitcoinRegionStats (uint32_t *bitcoinNodesRegions, uint32_t totalNodes);

NS_LOG_COMPONENT_DEFINE ("MyMpiTest");

int 
main (int argc, char *argv[])
{
#ifdef NS3_MPI
  bool nullmsg = false;
  bool testScalability = false;
  bool unsolicited = false;
  bool relayNetwork = false;
  bool litecoin = false;
  bool dogecoin = false;
  bool sendheaders = false;
  enum Cryptocurrency  cryptocurrency = BITCOIN;
  double tStart = get_wall_time(), tStartSimulation, tFinish;
  const int secsPerMin = 60;
  const uint16_t bitcoinPort = 8333;
  const double realAverageBlockGenIntervalMinutes = 10; //minutes
  int targetNumberOfBlocks = 100;
  double averageBlockGenIntervalSeconds = 10 * secsPerMin; //seconds
  double fixedHashRate = 0.5;
  int start = 0;
  double bandwidthSDDivider = -1;
  double latency = 40;

  
  int totalNoNodes = 4;

  int minConnectionsPerNode = -1;
  int maxConnectionsPerNode = -1;
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
  long blockSize = 450000*averageBlockGenIntervalMinutes/realAverageBlockGenIntervalMinutes;


  Ipv4InterfaceContainer                               ipv4InterfaceContainer;
  std::map<uint32_t, std::vector<Ipv4Address>>         nodesConnections;
  std::map<uint32_t, std::map<Ipv4Address, double>>    peersDownloadSpeeds;
  std::map<uint32_t, nodeInternetSpeeds>               nodesInternetSpeeds;
  std::vector<uint32_t>                                miners;
  int                                                  nodesInSystemId0 = 0;
  
  Time::SetResolution (Time::NS);
  
  CommandLine cmd;
  cmd.AddValue ("nullmsg", "Enable the use of null-message synchronization", nullmsg);
  cmd.AddValue ("bandwidthSDDivider", "The divider for the standard deviation of bandwidths", bandwidthSDDivider);
  cmd.AddValue ("latency", "The latency of the nodes (ms)", latency);
  cmd.AddValue ("blockSize", "The the fixed block size (Bytes)", blockSize);
  cmd.AddValue ("noBlocks", "The number of generated blocks", targetNumberOfBlocks);
  cmd.AddValue ("nodes", "The total number of nodes in the network", totalNoNodes);
  cmd.AddValue ("minConnections", "The minConnectionsPerNode of the grid", minConnectionsPerNode);
  cmd.AddValue ("maxConnections", "The maxConnectionsPerNode of the grid", maxConnectionsPerNode);
  cmd.AddValue ("blockIntervalMinutes", "The average block generation interval in minutes", averageBlockGenIntervalMinutes);
  cmd.AddValue ("test", "Test the scalability of the simulation", testScalability);
  cmd.AddValue ("unsolicited", "Change the miners block broadcast type to UNSOLICITED", unsolicited);
  cmd.AddValue ("relayNetwork", "Change the miners block broadcast type to RELAY_NETWORK", relayNetwork);
  cmd.AddValue ("sendheaders", "Change the protocol to sendheaders", sendheaders);
  cmd.AddValue ("litecoin", "Imitate the litecoin network behaviour", litecoin);
  cmd.AddValue ("dogecoin", "Imitate the litecoin network behaviour", dogecoin);

  cmd.Parse(argc, argv);
  
  if (litecoin && dogecoin)
  {
    std::cout << "You cannot select both litecoin and dogecoin behaviour" << std::endl;
	return 0;
  }
  
  if (litecoin)
  {
    averageBlockGenIntervalMinutes =  2.5;
    totalNoNodes = 1000;
    cryptocurrency = LITECOIN;
  }
  else if (dogecoin)
  {
    averageBlockGenIntervalMinutes =  1;
	totalNoNodes = 650;
    cryptocurrency = DOGECOIN;
  }

  averageBlockGenIntervalSeconds = averageBlockGenIntervalMinutes * secsPerMin;
  stop = targetNumberOfBlocks * averageBlockGenIntervalMinutes; //seconds
  nodeStatistics *stats = new nodeStatistics[totalNoNodes];
  averageBlockGenIntervalMinutes = averageBlockGenIntervalSeconds/secsPerMin;
  blockSize = blockSize*averageBlockGenIntervalMinutes/realAverageBlockGenIntervalMinutes;

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

  //LogComponentEnable("BitcoinNode", LOG_LEVEL_INFO);
  //LogComponentEnable("BitcoinMiner", LOG_LEVEL_WARN);
  //LogComponentEnable("Ipv4AddressGenerator", LOG_LEVEL_FUNCTION);
  //LogComponentEnable("OnOffApplication", LOG_LEVEL_DEBUG);
  //LogComponentEnable("OnOffApplication", LOG_LEVEL_WARN);

  
  if (sizeof(minersHash)/sizeof(double) != noMiners)
  {
    std::cout << "The minersHash entries does not match the number of miners\n";
    return 0;
  }
  if (unsolicited && relayNetwork)
  {
    std::cout << "You have set both the unsolicited and the relayNetwork flag\n";
    return 0;
  }
  
  BitcoinTopologyHelper bitcoinTopologyHelper (systemCount, totalNoNodes, noMiners, minersRegions,
                                               bandwidthSDDivider, cryptocurrency, minConnectionsPerNode, 
                                               maxConnectionsPerNode, latency, 5, systemId);

  // Install stack on Grid
  InternetStackHelper stack;
  bitcoinTopologyHelper.InstallStack (stack);

  // Assign Addresses to Grid
  bitcoinTopologyHelper.AssignIpv4Addresses (Ipv4AddressHelperCustom ("1.0.0.0", "255.255.255.0", false));
  ipv4InterfaceContainer = bitcoinTopologyHelper.GetIpv4InterfaceContainer();
  nodesConnections = bitcoinTopologyHelper.GetNodesConnectionsIps();
  miners = bitcoinTopologyHelper.GetMiners();
  peersDownloadSpeeds = bitcoinTopologyHelper.GetPeersDownloadSpeeds();
  nodesInternetSpeeds = bitcoinTopologyHelper.GetNodesInternetSpeeds();
  if (systemId == 0)
    PrintBitcoinRegionStats(bitcoinTopologyHelper.GetBitcoinNodesRegions(), totalNoNodes);
											   
  //Install miners
  BitcoinMinerHelper bitcoinMinerHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), bitcoinPort),
                                          nodesConnections[miners[0]], noMiners, peersDownloadSpeeds[0], nodesInternetSpeeds[0], 
										  stats, minersHash[0], averageBlockGenIntervalSeconds);
  ApplicationContainer bitcoinMiners;
  int count = 0;
  if (testScalability == true)
    bitcoinMinerHelper.SetAttribute("FixedBlockIntervalGeneration", DoubleValue(averageBlockGenIntervalSeconds));

  for(auto &miner : miners)
  {
	Ptr<Node> targetNode = bitcoinTopologyHelper.GetNode (miner);
	
	if (systemId == targetNode->GetSystemId())
	{
      bitcoinMinerHelper.SetAttribute("HashRate", DoubleValue(minersHash[count]));
      bitcoinMinerHelper.SetAttribute("InvTimeoutMinutes", TimeValue (Minutes (2*averageBlockGenIntervalMinutes)));
      if (litecoin)	  
        bitcoinMinerHelper.SetAttribute("Cryptocurrency", UintegerValue (LITECOIN));	  
      else if (dogecoin)	  
        bitcoinMinerHelper.SetAttribute("Cryptocurrency", UintegerValue (DOGECOIN));
	
      if (sendheaders)	  
        bitcoinMinerHelper.SetProtocolType(SENDHEADERS);	  

      bitcoinMinerHelper.SetPeersAddresses (nodesConnections[miner]);
	  bitcoinMinerHelper.SetPeersDownloadSpeeds (peersDownloadSpeeds[miner]);
	  bitcoinMinerHelper.SetNodeInternetSpeeds (nodesInternetSpeeds[miner]);
	  bitcoinMinerHelper.SetNodeStats (&stats[miner]);
      
	  if(unsolicited)
	    bitcoinMinerHelper.SetBlockBroadcastType (UNSOLICITED);
	  if(relayNetwork)
	    bitcoinMinerHelper.SetBlockBroadcastType (RELAY_NETWORK);
	  //bitcoinMinerHelper.SetAttribute("FixedBlockSize", UintegerValue(blockSize));

	  bitcoinMiners.Add(bitcoinMinerHelper.Install (targetNode));
/*       std::cout << "SystemId " << systemId << ": Miner " << miner << " with hash power = " << minersHash[count] 
	            << " and systemId = " << targetNode->GetSystemId() << " was installed in node " 
                << targetNode->GetId () << std::endl;  */
	  
	  if (systemId == 0)
        nodesInSystemId0++;
	}				
	count++;
	if (testScalability == true)
	  bitcoinMinerHelper.SetAttribute("FixedBlockIntervalGeneration", DoubleValue(3*averageBlockGenIntervalSeconds));

  }
  bitcoinMiners.Start (Seconds (start));
  bitcoinMiners.Stop (Minutes (stop));

  
  //Install simple nodes
  BitcoinNodeHelper bitcoinNodeHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), bitcoinPort), 
                                        nodesConnections[0], peersDownloadSpeeds[0], nodesInternetSpeeds[0], stats);
  ApplicationContainer bitcoinNodes;
  
  for(auto &node : nodesConnections)
  {
    Ptr<Node> targetNode = bitcoinTopologyHelper.GetNode (node.first);
	
	if (systemId == targetNode->GetSystemId())
	{
  
      if ( std::find(miners.begin(), miners.end(), node.first) == miners.end() )
	  {
        bitcoinNodeHelper.SetAttribute("InvTimeoutMinutes", TimeValue (Minutes (2*averageBlockGenIntervalMinutes)));	  
	    bitcoinNodeHelper.SetPeersAddresses (node.second);
	    bitcoinNodeHelper.SetPeersDownloadSpeeds (peersDownloadSpeeds[node.first]);
	    bitcoinNodeHelper.SetNodeInternetSpeeds (nodesInternetSpeeds[node.first]);
		bitcoinNodeHelper.SetNodeStats (&stats[node.first]);
		
        if (sendheaders)	  
          bitcoinNodeHelper.SetProtocolType(SENDHEADERS);	
	  
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

  int            blocklen[25] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}; 
  MPI_Aint       disp[25]; 
  MPI_Datatype   dtypes[25] = {MPI_INT, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_INT,
                               MPI_LONG, MPI_LONG, MPI_LONG, MPI_LONG, MPI_LONG, MPI_LONG, MPI_LONG, MPI_LONG, MPI_LONG, MPI_LONG, MPI_INT, MPI_INT, MPI_INT}; 
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
  disp[12] = offsetof(nodeStatistics, invReceivedBytes);
  disp[13] = offsetof(nodeStatistics, invSentBytes);
  disp[14] = offsetof(nodeStatistics, getHeadersReceivedBytes);
  disp[15] = offsetof(nodeStatistics, getHeadersSentBytes);
  disp[16] = offsetof(nodeStatistics, headersReceivedBytes);
  disp[17] = offsetof(nodeStatistics, headersSentBytes);
  disp[18] = offsetof(nodeStatistics, getDataReceivedBytes);
  disp[19] = offsetof(nodeStatistics, getDataSentBytes);
  disp[20] = offsetof(nodeStatistics, blockReceivedBytes);
  disp[21] = offsetof(nodeStatistics, blockSentBytes);
  disp[22] = offsetof(nodeStatistics, longestFork);
  disp[23] = offsetof(nodeStatistics, blocksInForks);
  disp[24] = offsetof(nodeStatistics, connections);

  MPI_Type_create_struct (25, blocklen, disp, dtypes, &mpi_nodeStatisticsType);
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
      stats[recv.nodeId].invReceivedBytes = recv.invReceivedBytes;
      stats[recv.nodeId].invSentBytes = recv.invSentBytes;
      stats[recv.nodeId].getHeadersReceivedBytes = recv.getHeadersReceivedBytes;
      stats[recv.nodeId].getHeadersSentBytes = recv.getHeadersSentBytes;
      stats[recv.nodeId].headersReceivedBytes = recv.headersReceivedBytes;
      stats[recv.nodeId].headersSentBytes = recv.headersSentBytes;
      stats[recv.nodeId].getDataReceivedBytes = recv.getDataReceivedBytes;
      stats[recv.nodeId].getDataSentBytes = recv.getDataSentBytes;
      stats[recv.nodeId].blockReceivedBytes = recv.blockReceivedBytes;
      stats[recv.nodeId].blockSentBytes = recv.blockSentBytes;
      stats[recv.nodeId].longestFork = recv.longestFork;
      stats[recv.nodeId].blocksInForks = recv.blocksInForks;
      stats[recv.nodeId].connections = recv.connections;

	  count++;
    }
  }	  
#endif

  if (systemId == 0)
  {
    tFinish=get_wall_time();
	
    //PrintStatsForEachNode(stats, totalNoNodes);
    PrintTotalStats(stats, totalNoNodes, tStartSimulation, tFinish, averageBlockGenIntervalMinutes, relayNetwork);
	
    if(unsolicited)
      std::cout << "The broadcast type was UNSOLICITED.\n";
    else if(relayNetwork)
      std::cout << "The broadcast type was RELAY_NETWORK.\n";
    else
      std::cout << "The broadcast type was STANDARD.\n";

    std::cout << "\nThe simulation ran for " << tFinish - tStart << "s simulating "
              << stop << "mins. Performed " << stop * secsPerMin / (tFinish - tStart)
              << " faster than realtime.\n" << "Setup time = " << tStartSimulation - tStart << "s\n"
              <<"It consisted of " << totalNoNodes << " nodes (" << noMiners << " miners) with minConnectionsPerNode = "
              << minConnectionsPerNode << " and maxConnectionsPerNode = " << maxConnectionsPerNode 
              << ".\nThe averageBlockGenIntervalMinutes was " << averageBlockGenIntervalMinutes << "min.\n";

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
    std::cout << "Connections = " << stats[it].connections << "\n";
    std::cout << "Mean Block Receive Time = " << stats[it].meanBlockReceiveTime << " or " 
              << static_cast<int>(stats[it].meanBlockReceiveTime) / secPerMin << "min and " 
			  << stats[it].meanBlockReceiveTime - static_cast<int>(stats[it].meanBlockReceiveTime) / secPerMin * secPerMin << "s\n";
    std::cout << "Mean Block Propagation Time = " << stats[it].meanBlockPropagationTime << "s\n";
    std::cout << "Mean Block Size = " << stats[it].meanBlockSize << " Bytes\n";
    std::cout << "Total Blocks = " << stats[it].totalBlocks << "\n";
    std::cout << "Stale Blocks = " << stats[it].staleBlocks << " (" 
              << 100. * stats[it].staleBlocks / stats[it].totalBlocks << "%)\n";
    std::cout << "The size of the longest fork was " << stats[it].longestFork << " blocks\n";
    std::cout << "There were in total " << stats[it].blocksInForks << " blocks in forks\n";
    std::cout << "The total received INV messages were " << stats[it].invReceivedBytes << " Bytes\n";
    std::cout << "The total received GET_HEADERS messages were " << stats[it].getHeadersReceivedBytes << " Bytes\n";
    std::cout << "The total received HEADERS messages were " << stats[it].headersReceivedBytes << " Bytes\n";
    std::cout << "The total received GET_DATA messages were " << stats[it].getDataReceivedBytes << " Bytes\n";
    std::cout << "The total received BLOCK messages were " << stats[it].blockReceivedBytes << " Bytes\n";
    std::cout << "The total sent INV messages were " << stats[it].invSentBytes << " Bytes\n";
    std::cout << "The total sent GET_HEADERS messages were " << stats[it].getHeadersSentBytes << " Bytes\n";
    std::cout << "The total sent HEADERS messages were " << stats[it].headersSentBytes << " Bytes\n";
    std::cout << "The total sent GET_DATA messages were " << stats[it].getDataSentBytes << " Bytes\n";
    std::cout << "The total sent BLOCK messages were " << stats[it].blockSentBytes << " Bytes\n";


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


void PrintTotalStats (nodeStatistics *stats, int totalNodes, double start, double finish, double averageBlockGenIntervalMinutes, bool relayNetwork)
{
  const int  secPerMin = 60;
  double     meanBlockReceiveTime = 0;
  double     meanBlockPropagationTime = 0;
  double     meanMinersBlockPropagationTime = 0;
  double     meanBlockSize = 0;
  double     totalBlocks = 0;
  double     staleBlocks = 0;
  double     invReceivedBytes = 0;
  double     invSentBytes = 0;
  double     getHeadersReceivedBytes = 0;
  double     getHeadersSentBytes = 0;
  double     headersReceivedBytes = 0;
  double     headersSentBytes = 0;
  double     getDataReceivedBytes = 0;
  double     getDataSentBytes = 0;
  double     blockReceivedBytes = 0;
  double     blockSentBytes = 0;
  double     longestFork = 0;
  double     blocksInForks = 0;
  double     averageBandwidthPerNode = 0;
  double     connectionsPerNode = 0;
  double     connectionsPerMiner = 0;
  double     download = 0;
  double     upload = 0;

  uint32_t   nodes = 0;
  uint32_t   miners = 0;
  std::vector<double>    propagationTimes;
  std::vector<double>    minersPropagationTimes;
  std::vector<double>    downloadBandwidths;
  std::vector<double>    uploadBandwidths;
  std::vector<double>    totalBandwidths;
  
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
    invReceivedBytes = invReceivedBytes*it/static_cast<double>(it + 1) + stats[it].invReceivedBytes/static_cast<double>(it + 1);
    invSentBytes = invSentBytes*it/static_cast<double>(it + 1) + stats[it].invSentBytes/static_cast<double>(it + 1);
    getHeadersReceivedBytes = getHeadersReceivedBytes*it/static_cast<double>(it + 1) + stats[it].getHeadersReceivedBytes/static_cast<double>(it + 1);
    getHeadersSentBytes = getHeadersSentBytes*it/static_cast<double>(it + 1) + stats[it].getHeadersSentBytes/static_cast<double>(it + 1);
    headersReceivedBytes = headersReceivedBytes*it/static_cast<double>(it + 1) + stats[it].headersReceivedBytes/static_cast<double>(it + 1);
    headersSentBytes = headersSentBytes*it/static_cast<double>(it + 1) + stats[it].headersSentBytes/static_cast<double>(it + 1);
    getDataReceivedBytes = getDataReceivedBytes*it/static_cast<double>(it + 1) + stats[it].getDataReceivedBytes/static_cast<double>(it + 1);
    getDataSentBytes = getDataSentBytes*it/static_cast<double>(it + 1) + stats[it].getDataSentBytes/static_cast<double>(it + 1);
    blockReceivedBytes = blockReceivedBytes*it/static_cast<double>(it + 1) + stats[it].blockReceivedBytes/static_cast<double>(it + 1);
    blockSentBytes = blockSentBytes*it/static_cast<double>(it + 1) + stats[it].blockSentBytes/static_cast<double>(it + 1);
    longestFork = longestFork*it/static_cast<double>(it + 1) + stats[it].longestFork/static_cast<double>(it + 1);
    blocksInForks = blocksInForks*it/static_cast<double>(it + 1) + stats[it].blocksInForks/static_cast<double>(it + 1);
	
	propagationTimes.push_back(stats[it].meanBlockPropagationTime);

    download = stats[it].invReceivedBytes + stats[it].getHeadersReceivedBytes + stats[it].headersReceivedBytes +
             + stats[it].getDataReceivedBytes + stats[it].blockReceivedBytes;
    upload = stats[it].invSentBytes + stats[it].getHeadersSentBytes + stats[it].headersSentBytes
           + stats[it].getDataSentBytes + stats[it].blockSentBytes;
    download = download / (1000 *(stats[it].totalBlocks - 1) * averageBlockGenIntervalMinutes * secPerMin) * 8;
    upload = upload / (1000 *(stats[it].totalBlocks - 1) * averageBlockGenIntervalMinutes * secPerMin) * 8;
    downloadBandwidths.push_back(download);  
    uploadBandwidths.push_back(upload);     	  
    totalBandwidths.push_back(download + upload); 

	
	if(stats[it].miner == 0)
    {
      connectionsPerNode = connectionsPerNode*nodes/static_cast<double>(nodes + 1) + stats[it].connections/static_cast<double>(nodes + 1);
      nodes++;
    }
    else
    {
      connectionsPerMiner = connectionsPerMiner*miners/static_cast<double>(miners + 1) + stats[it].connections/static_cast<double>(miners + 1);
      meanMinersBlockPropagationTime = meanMinersBlockPropagationTime*miners/static_cast<double>(miners + 1) + stats[it].meanBlockPropagationTime/static_cast<double>(miners + 1);
      minersPropagationTimes.push_back(stats[it].meanBlockPropagationTime);
      miners++;
    }
  }
  
  averageBandwidthPerNode = invReceivedBytes + invSentBytes + getHeadersReceivedBytes + getHeadersSentBytes + headersReceivedBytes
                          + headersSentBytes + getDataReceivedBytes + getDataSentBytes + blockReceivedBytes + blockSentBytes;
				   
  totalBlocks /= totalNodes;
  staleBlocks /= totalNodes;
  sort(propagationTimes.begin(), propagationTimes.end());
  sort(minersPropagationTimes.begin(), minersPropagationTimes.end());

  double median = *(propagationTimes.begin()+propagationTimes.size()/2);
  double p_25 = *(propagationTimes.begin()+int(propagationTimes.size()*.25));
  double p_75 = *(propagationTimes.begin()+int(propagationTimes.size()*.75));
  double p_90 = *(propagationTimes.begin()+int(propagationTimes.size()*.90));
  double minersMedian = *(minersPropagationTimes.begin()+int(minersPropagationTimes.size()/2));
  
  std::cout << "\nTotal Stats:\n";
  std::cout << "Average Connections/node = " << connectionsPerNode << "\n";
  std::cout << "Average Connections/miner = " << connectionsPerMiner << "\n";
  std::cout << "Mean Block Receive Time = " << meanBlockReceiveTime << " or " 
            << static_cast<int>(meanBlockReceiveTime) / secPerMin << "min and " 
	        << meanBlockReceiveTime - static_cast<int>(meanBlockReceiveTime) / secPerMin * secPerMin << "s\n";
  std::cout << "Mean Block Propagation Time = " << meanBlockPropagationTime << "s\n";
  std::cout << "Median Block Propagation Time = " << median << "s\n";
  std::cout << "25% percentile of Block Propagation Time = " << p_25 << "s\n";
  std::cout << "75% percentile of Block Propagation Time = " << p_75 << "s\n";
  std::cout << "90% percentile of Block Propagation Time = " << p_90 << "s\n";
  std::cout << "Miners Mean Block Propagation Time = " << meanMinersBlockPropagationTime << "s\n";
  std::cout << "Miners Median Block Propagation Time = " << minersMedian << "s\n";
  std::cout << "Mean Block Size = " << meanBlockSize << " Bytes\n";
  std::cout << "Total Blocks = " << totalBlocks << "\n";
  std::cout << "Stale Blocks = " << staleBlocks << " (" 
            << 100. * staleBlocks / totalBlocks << "%)\n";
  std::cout << "The size of the longest fork was " << longestFork << " blocks\n";
  std::cout << "There were in total " << blocksInForks << " blocks in forks\n";
  std::cout << "The average received INV messages were " << invReceivedBytes << " Bytes (" 
            << 100. * invReceivedBytes / averageBandwidthPerNode << "%)\n";
  std::cout << "The average received GET_HEADERS messages were " << getHeadersReceivedBytes << " Bytes (" 
            << 100. * getHeadersReceivedBytes / averageBandwidthPerNode << "%)\n";
  std::cout << "The average received HEADERS messages were " << headersReceivedBytes << " Bytes (" 
            << 100. * headersReceivedBytes / averageBandwidthPerNode << "%)\n";
  std::cout << "The average received GET_DATA messages were " << getDataReceivedBytes << " Bytes (" 
            << 100. * getDataReceivedBytes / averageBandwidthPerNode << "%)\n";
  std::cout << "The average received BLOCK messages were " << blockReceivedBytes << " Bytes (" 
            << 100. * blockReceivedBytes / averageBandwidthPerNode << "%)\n";
  std::cout << "The average sent INV messages were " << invSentBytes << " Bytes (" 
            << 100. * invSentBytes / averageBandwidthPerNode << "%)\n";
  std::cout << "The average sent GET_HEADERS messages were " << getHeadersSentBytes << " Bytes (" 
            << 100. * getHeadersSentBytes / averageBandwidthPerNode << "%)\n";
  std::cout << "The average sent HEADERS messages were " << headersSentBytes << " Bytes (" 
            << 100. * headersSentBytes / averageBandwidthPerNode << "%)\n";
  std::cout << "The average sent GET_DATA messages were " << getDataSentBytes << " Bytes (" 
            << 100. * getDataSentBytes / averageBandwidthPerNode << "%)\n";
  std::cout << "The average sent BLOCK messages were " << blockSentBytes << " Bytes (" 
            << 100. * blockSentBytes / averageBandwidthPerNode << "%)\n";
  std::cout << "Total average traffic due to INV messages = " << invReceivedBytes +  invSentBytes << " Bytes(" 
            << 100. * (invReceivedBytes +  invSentBytes) / averageBandwidthPerNode << "%)\n";	
  std::cout << "Total average traffic due to GET_HEADERS messages = " << getHeadersReceivedBytes +  getHeadersSentBytes << " Bytes(" 
            << 100. * (getHeadersReceivedBytes +  getHeadersSentBytes) / averageBandwidthPerNode << "%)\n";
  std::cout << "Total average traffic due to HEADERS messages = " << headersReceivedBytes +  headersSentBytes << " Bytes(" 
            << 100. * (headersReceivedBytes +  headersSentBytes) / averageBandwidthPerNode << "%)\n";
  std::cout << "Total average traffic due to GET_DATA messages = " << getDataReceivedBytes +  getDataSentBytes << " Bytes(" 
            << 100. * (getDataReceivedBytes +  getDataSentBytes) / averageBandwidthPerNode << "%)\n";
  std::cout << "Total average traffic due to BLOCK messages = " << blockReceivedBytes +  blockSentBytes << " Bytes(" 
            << 100. * (blockReceivedBytes +  blockSentBytes) / averageBandwidthPerNode << "%)\n";
  std::cout << "Total average traffic/node = " << averageBandwidthPerNode << " Bytes (" 
            << averageBandwidthPerNode / (1000 *(totalBlocks - 1) * averageBlockGenIntervalMinutes * secPerMin) * 8
            << " Kbps and " << averageBandwidthPerNode / (1000 * (totalBlocks - 1)) << " KB/block)\n";
  std::cout << (finish - start)/ (totalBlocks - 1)<< "s per generated block\n";
  
  
  std::cout << "\nDownload Bandwidths = [";
  double average = 0;
  for(auto it = downloadBandwidths.begin(); it != downloadBandwidths.end(); it++)
  {
    if (it == downloadBandwidths.begin())
      std::cout << *it;
    else
      std::cout << ", " << *it ;
    average += *it;
  }
  std::cout << "] average = " << average/totalBandwidths.size() << "\n" ;
  
  std::cout << "\nUpload Bandwidths = [";
  average = 0;
  for(auto it = uploadBandwidths.begin(); it != uploadBandwidths.end(); it++)
  {
    if (it == uploadBandwidths.begin())
      std::cout << *it;
    else
      std::cout << ", " << *it ;
    average += *it;
  }
  std::cout << "] average = " << average/totalBandwidths.size() << "\n" ;
  
  std::cout << "\nTotal Bandwidths = [";
  average = 0;
  for(auto it = totalBandwidths.begin(); it != totalBandwidths.end(); it++)
  {
    if (it == totalBandwidths.begin())
      std::cout << *it;
    else
      std::cout << ", " << *it ;
    average += *it;
  }
  std::cout << "] average = " << average/totalBandwidths.size() << "\n" ;

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
	
	
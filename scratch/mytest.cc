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

using namespace ns3;

double get_wall_time();
int GetNodeIdByIpv4 (Ipv4InterfaceContainer container, Ipv4Address addr);

NS_LOG_COMPONENT_DEFINE ("FifthScriptExample");

int 
main (int argc, char *argv[])
{
  double tStart = get_wall_time(), tFinish;
  const int secsPerMin = 60;
  const uint16_t bitcoinPort = 8333;
  const double realAverageBlockGenIntervalMinutes = 10; //minutes
  int targetNumberOfBlocks = 1;
  double averageBlockGenIntervalSeconds = 10 * secsPerMin; //seconds
  double fixedHashRate = 0.5;
  int start = 0;
  
  int xSize = 3;
  int ySize = 4;
  int connectionsPerNode = 2;
  int noMiners = 3;
  
  int totalNoNodes = xSize * ySize;
  double averageBlockGenIntervalMinutes = averageBlockGenIntervalSeconds/secsPerMin;
  double stop = targetNumberOfBlocks * averageBlockGenIntervalMinutes; //seconds
  double blockGenBinSize = 1./secsPerMin/1000;					       //minutes
  double blockGenParameter = 0.19 * blockGenBinSize / 2 * (realAverageBlockGenIntervalMinutes / averageBlockGenIntervalMinutes);	//0.19 for blockGenBinSize = 2mins

  std::map<int, Ipv4Address>                 miners; // key = nodeId
  std::map<int, std::vector<Ipv4Address>>    nodesConnections; // key = nodeId
  Ipv4InterfaceContainer                     ipv4InterfaceContainer;
  
  srand (1000);
  Time::SetResolution (Time::NS);
  
  CommandLine cmd;
  cmd.Parse(argc, argv);
  
  LogComponentEnable("BitcoinNode", LOG_LEVEL_WARN);
  LogComponentEnable("BitcoinMiner", LOG_LEVEL_WARN);
  //LogComponentEnable("OnOffApplication", LOG_LEVEL_DEBUG);
  //LogComponentEnable("OnOffApplication", LOG_LEVEL_WARN);

  if (noMiners > totalNoNodes)
  {
    std::cout << "The number of miners is larger than the total number of nodes\n";
    return 0;
  }
  
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("8Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ms"));

  // Create Grid
  PointToPointGridHelperCustom grid (xSize, ySize, 1, pointToPoint);
  grid.BoundingBox(100, 100, 200, 200);

  // Install stack on Grid
  InternetStackHelper stack;
  grid.InstallStack (stack);

  // Assign Addresses to Grid
  grid.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                            Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"));
  ipv4InterfaceContainer = grid.GetIpv4InterfaceContainer();
  
  
  {
    //nodes contain the ids of the nodes
    std::vector<int> nodes;

    for (int i = 0; i < totalNoNodes; i++)
    {
      nodes.push_back(i);
	}

	//print the initialized nodes
	for (std::vector<int>::iterator j = nodes.begin(); j != nodes.end(); j++)
	{
	  std::cout << *j << " " ;
	}

	//Choose the miners randomly. They should be unique (no miner should be chosen twice).
	//So, remove each chose miner from nodes vector
    for (int i = 0; i < noMiners; i++)
	{
      int index = rand() % nodes.size();
      miners[nodes[index]] = grid.GetIpv4Address (nodes[index] / ySize, nodes[index] % ySize);
      std::cout << "\n" << "Chose " << nodes[index] << "     ";
      nodes.erase(nodes.begin() + index);
	  
	  
	  for (std::vector<int>::iterator it = nodes.begin(); it != nodes.end(); it++)
	  {
	    std::cout << *it << " " ;
	  }
	}
  }
  
  //Print the miners
  std::cout << "\n\nThe miners are:\n" << std::endl;
  for(auto &miner : miners)
  {
    std::cout << miner.first << "     " << miner.second << ":\n";
  }
  std::cout << std::endl;

  //Interconnect the miners
  for(auto &miner : miners)
  {
    for(auto &peer : miners)
    {
      if (peer.first != miner.first)
        nodesConnections[miner.first].push_back(peer.second);
	}
  }
  
  //Print the miners' connections
  std::cout << "The miners are interconnected:" << std::endl;
  for(auto &miner : nodesConnections)
  {
	std::cout << "\nMiner " << miner.first << ":   " ;
	for(std::vector<Ipv4Address>::const_iterator it = miner.second.begin(); it != miner.second.end(); it++)
	{
      std::cout << GetNodeIdByIpv4(ipv4InterfaceContainer, *it) << " " ;
	}
  }
  std::cout << "\n" << std::endl;
  
  
  Ipv4Address bitcoinMiner1Address (grid.GetIpv4Address (0,0));
  Ipv4Address bitcoinMiner2Address (grid.GetIpv4Address (xSize - 1, ySize - 1));
  Ipv4Address bitcoinNode1Address (grid.GetIpv4Address (xSize - 1, 0));
  Ipv4Address bitcoinNode2Address (grid.GetIpv4Address (0, ySize - 1));

  Ipv4Address testAddress[] =  {bitcoinNode1Address, bitcoinNode2Address};
  std::vector<Ipv4Address> peers (testAddress, testAddress + sizeof(testAddress) / sizeof(Ipv4Address) );
  for (std::vector<Ipv4Address>::const_iterator i = peers.begin(); i != peers.end(); ++i)
    std::cout << "testAddress: " << *i << std::endl;

/*   BitcoinMinerHelper bitcoinMinerHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), bitcoinPort),
                                          peers, 0.67, blockGenBinSize, blockGenParameter, averageBlockGenIntervalSeconds);
  //bitcoinMinerHelper.SetAttribute("FixedBlockIntervalGeneration", DoubleValue(300));
  ApplicationContainer bitcoinMiners = bitcoinMinerHelper.Install (grid.GetNode (0,0));
  //bitcoinMinerHelper.SetAttribute("FixedBlockIntervalGeneration", DoubleValue(1300.1));
  bitcoinMinerHelper.SetAttribute("HashRate", DoubleValue(0.33));
  //bitcoinMiners.Add(bitcoinMinerHelper.Install (grid.GetNode (xSize - 1, ySize - 1)));

  bitcoinMiners.Start (Seconds (start));
  bitcoinMiners.Stop (Minutes (stop)); */

  
  Ipv4Address testAddress2[] =  {bitcoinMiner1Address, bitcoinMiner2Address};
  peers.assign (testAddress2,testAddress2 + sizeof(testAddress2) / sizeof(Ipv4Address));
  for (std::vector<Ipv4Address>::const_iterator i = peers.begin(); i != peers.end(); ++i)
    std::cout << "testAddress2: " << *i << std::endl;
	
/*   BitcoinNodeHelper bitcoinNodeHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), bitcoinPort), peers);
  ApplicationContainer bitcoinNodes = bitcoinNodeHelper.Install (grid.GetNode (xSize - 1, 0));
  bitcoinNodes.Add(bitcoinNodeHelper.Install (grid.GetNode (0, ySize - 1)));
  bitcoinNodes.Start (Seconds (start));
  bitcoinNodes.Stop (Minutes (stop)); */

  
  // Set up the actual simulation
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  
  //Simulator::Stop (Minutes (stop));
  Simulator::Run ();
  Simulator::Destroy ();

  tFinish=get_wall_time();
  std::cout << "\nThe simulation ran for " << tFinish - tStart << "s simulating "
            << stop << "mins. Performed " << stop * secsPerMin / (tFinish - tStart) 
			<< " faster than realtime.\n";
  return 0;
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
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

double get_wall_time();

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FifthScriptExample");

int 
main (int argc, char *argv[])
{
  double tStart = get_wall_time(), tFinish;
  const int secsPerMin = 60;
  const double realAverageBlockGenIntervalMinutes = 10; //minutes
  int xSize = 2;
  int ySize = 2;
  int targetNumberOfBlocks = 10000;
  double averageBlockGenIntervalSeconds = 10 * secsPerMin; //seconds
  double fixedHashRate = 0.5;
  int start = 0;
  
  double averageBlockGenIntervalMinutes = averageBlockGenIntervalSeconds/secsPerMin;
  double stop = targetNumberOfBlocks * averageBlockGenIntervalMinutes; //seconds
  double blockGenBinSize = 1./secsPerMin/1000;					       //minutes
  double blockGenParameter = 0.19 * blockGenBinSize / 2 * (realAverageBlockGenIntervalMinutes / averageBlockGenIntervalMinutes);	//0.19 for blockGenBinSize = 2mins

  Time::SetResolution (Time::NS);
  
  CommandLine cmd;
  cmd.Parse(argc, argv);
  
  LogComponentEnable("BitcoinNode", LOG_LEVEL_WARN);
  LogComponentEnable("BitcoinMiner", LOG_LEVEL_WARN);
  //LogComponentEnable("OnOffApplication", LOG_LEVEL_DEBUG);
  //LogComponentEnable("OnOffApplication", LOG_LEVEL_WARN);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("8Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ms"));

  // Create Grid
  PointToPointGridHelper grid (xSize, ySize, pointToPoint);
  grid.BoundingBox(100, 100, 200, 200);

  // Install stack on Grid
  InternetStackHelper stack;
  grid.InstallStack (stack);

  // Assign Addresses to Grid
  grid.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                            Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"));

  uint16_t bitcoinPort = 8333;
  Ipv4Address bitcoinMiner1Address (grid.GetIpv4Address (0,0));
  Ipv4Address bitcoinMiner2Address (grid.GetIpv4Address (xSize - 1, ySize - 1));
  Ipv4Address bitcoinNode1Address (grid.GetIpv4Address (xSize - 1, 0));
  Ipv4Address bitcoinNode2Address (grid.GetIpv4Address (0, ySize - 1));

  Ipv4Address testAddress[] =  {bitcoinNode1Address, bitcoinNode2Address};
  std::vector<Ipv4Address> peers (testAddress, testAddress + sizeof(testAddress) / sizeof(Ipv4Address) );
  for (std::vector<Ipv4Address>::const_iterator i = peers.begin(); i != peers.end(); ++i)
    std::cout << "testAddress: " << *i << std::endl;

  BitcoinMinerHelper bitcoinMinerHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), bitcoinPort),
                                          peers, 0.67, blockGenBinSize, blockGenParameter, averageBlockGenIntervalSeconds);
  //bitcoinMinerHelper.SetAttribute("FixedBlockIntervalGeneration", DoubleValue(300));
  ApplicationContainer bitcoinMiners = bitcoinMinerHelper.Install (grid.GetNode (0,0));
  //bitcoinMinerHelper.SetAttribute("FixedBlockIntervalGeneration", DoubleValue(1300.1));
  bitcoinMinerHelper.SetAttribute("HashRate", DoubleValue(0.33));
  bitcoinMiners.Add(bitcoinMinerHelper.Install (grid.GetNode (xSize - 1, ySize - 1)));


  bitcoinMiners.Start (Seconds (start));
  bitcoinMiners.Stop (Minutes (stop));

  
  Ipv4Address testAddress2[] =  {bitcoinMiner1Address, bitcoinMiner2Address};
  peers.assign (testAddress2,testAddress2 + sizeof(testAddress2) / sizeof(Ipv4Address));
  for (std::vector<Ipv4Address>::const_iterator i = peers.begin(); i != peers.end(); ++i)
    std::cout << "testAddress2: " << *i << std::endl;
	
  BitcoinNodeHelper bitcoinNodeHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), bitcoinPort), peers);
  ApplicationContainer bitcoinNodes = bitcoinNodeHelper.Install (grid.GetNode (xSize - 1, 0));
  bitcoinNodes.Add(bitcoinNodeHelper.Install (grid.GetNode (0, ySize - 1)));
  bitcoinNodes.Start (Seconds (start));
  bitcoinNodes.Stop (Minutes (stop));

  
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
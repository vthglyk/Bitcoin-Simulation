#include "ns3/address.h"
#include "ns3/address-utils.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/bitcoin-simple-attacker.h"
#include "../../rapidjson/document.h"
#include "../../rapidjson/writer.h"
#include "../../rapidjson/stringbuffer.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BitcoinSimpleAttacker");

NS_OBJECT_ENSURE_REGISTERED (BitcoinSimpleAttacker);

TypeId 
BitcoinSimpleAttacker::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BitcoinSimpleAttacker")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<BitcoinSimpleAttacker> ()
    .AddAttribute ("Local",
                   "The Address on which to Bind the rx socket.",
                   AddressValue (),
                   MakeAddressAccessor (&BitcoinSimpleAttacker::m_local),
                   MakeAddressChecker ())
    .AddAttribute ("Protocol",
                   "The type id of the protocol to use for the rx socket.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&BitcoinSimpleAttacker::m_tid),
                   MakeTypeIdChecker ())
    .AddAttribute ("FixedBlockSize", 
				   "The fixed size of the block",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BitcoinSimpleAttacker::m_fixedBlockSize),
                   MakeUintegerChecker<uint32_t> ())				   
    .AddAttribute ("FixedBlockIntervalGeneration", 
                   "The fixed time to wait between two consecutive block generations",
                   DoubleValue (0),
                   MakeDoubleAccessor (&BitcoinSimpleAttacker::m_fixedBlockTimeGeneration),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("InvTimeoutMinutes", 
				   "The timeout of inv messages in minutes",
                   TimeValue (Minutes (20)),
                   MakeTimeAccessor (&BitcoinSimpleAttacker::m_invTimeoutMinutes),
                   MakeTimeChecker())
    .AddAttribute ("HashRate", 
				   "The hash rate of the simple attacker",
                   DoubleValue (0.2),
                   MakeDoubleAccessor (&BitcoinSimpleAttacker::m_hashRate),
                   MakeDoubleChecker<double> ())	
    .AddAttribute ("BlockGenBinSize", 
				   "The block generation bin size",
                   DoubleValue (1./60),
                   MakeDoubleAccessor (&BitcoinSimpleAttacker::m_blockGenBinSize),
                   MakeDoubleChecker<double> ())	
    .AddAttribute ("BlockGenParameter", 
				   "The block generation distribution parameter",
                   DoubleValue (0.183/120),
                   MakeDoubleAccessor (&BitcoinSimpleAttacker::m_blockGenParameter),
                   MakeDoubleChecker<double> ())	
    .AddAttribute ("AverageBlockGenIntervalSeconds", 
				   "The average block generation interval we aim at (in seconds)",
                   DoubleValue (10*60),
                   MakeDoubleAccessor (&BitcoinSimpleAttacker::m_averageBlockGenIntervalSeconds),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("SecureBlocks", 
				   "The number of blocks required for the secure confirmation of the transactions",
                   UintegerValue (6),
                   MakeUintegerAccessor (&BitcoinSimpleAttacker::m_secureBlocks),
                   MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("Rx",
                     "A packet has been received",
                     MakeTraceSourceAccessor (&BitcoinSimpleAttacker::m_rxTrace),
                     "ns3::Packet::AddressTracedCallback")
  ;
  return tid;
}


BitcoinSimpleAttacker::BitcoinSimpleAttacker () : BitcoinMiner(), m_attackStarted (false), m_attackFinished(false)
{
  NS_LOG_FUNCTION (this);
}


BitcoinSimpleAttacker::~BitcoinSimpleAttacker(void)
{
  NS_LOG_FUNCTION (this);
}


void 
BitcoinSimpleAttacker::StartApplication ()    // Called at time specified by Start
{
  BitcoinNode::StartApplication ();
  NS_LOG_WARN ("Simple Attacker " << GetNode()->GetId() << " m_realAverageBlockGenIntervalSeconds = " << m_realAverageBlockGenIntervalSeconds << "s");
  NS_LOG_WARN ("Simple Attacker " << GetNode()->GetId() << " m_averageBlockGenIntervalSeconds = " << m_averageBlockGenIntervalSeconds << "s");
  NS_LOG_WARN ("Simple Attacker " << GetNode()->GetId() << " m_fixedBlockTimeGeneration = " << m_fixedBlockTimeGeneration << "s");
  NS_LOG_WARN ("Simple Attacker " << GetNode()->GetId() << " m_secureBlocks = " << m_secureBlocks);

  m_blockGenParameter *= m_hashRate;
	
  if (m_fixedBlockTimeGeneration == 0)
	m_blockGenTimeDistribution.param(std::geometric_distribution<int>::param_type(m_blockGenParameter)); 

  if (m_fixedBlockSize > 0)
    m_nextBlockSize = m_fixedBlockSize;
  else
  {
    std::array<double,201> intervals {0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115, 120, 125, 
                                     130, 135, 140, 145, 150, 155, 160, 165, 170, 175, 180, 185, 190, 195, 200, 205, 210, 215, 220, 225, 230, 235, 
                                     240, 245, 250, 255, 260, 265, 270, 275, 280, 285, 290, 295, 300, 305, 310, 315, 320, 325, 330, 335, 340, 345, 
                                     350, 355, 360, 365, 370, 375, 380, 385, 390, 395, 400, 405, 410, 415, 420, 425, 430, 435, 440, 445, 450, 455, 
                                     460, 465, 470, 475, 480, 485, 490, 495, 500, 505, 510, 515, 520, 525, 530, 535, 540, 545, 550, 555, 560, 565, 
                                     570, 575, 580, 585, 590, 595, 600, 605, 610, 615, 620, 625, 630, 635, 640, 645, 650, 655, 660, 665, 670, 675, 
                                     680, 685, 690, 695, 700, 705, 710, 715, 720, 725, 730, 735, 740, 745, 750, 755, 760, 765, 770, 775, 780, 785, 
                                     790, 795, 800, 805, 810, 815, 820, 825, 830, 835, 840, 845, 850, 855, 860, 865, 870, 875, 880, 885, 890, 895, 
                                     900, 905, 910, 915, 920, 925, 930, 935, 940, 945, 950, 955, 960, 965, 970, 975, 980, 985, 990, 995, 1000};
    std::array<double,200> weights {3.58, 0.33, 0.35, 0.4, 0.38, 0.4, 0.53, 0.46, 0.43, 0.48, 0.56, 0.69, 0.62, 0.62, 0.63, 0.62, 0.62, 0.63, 0.73, 
                                    1.96, 0.75, 0.76, 0.73, 0.64, 0.66, 0.66, 0.66, 0.7, 0.66, 0.73, 0.68, 0.66, 0.67, 0.66, 0.72, 0.68, 0.64, 0.61, 
                                    0.63, 0.58, 0.66, 0.6, 0.7, 0.62, 0.49, 0.59, 0.58, 0.59, 0.63, 1.59, 0.6, 0.58, 0.54, 0.62, 0.55, 0.54, 0.52, 
                                    0.5, 0.53, 0.55, 0.49, 0.47, 0.51, 0.49, 0.52, 0.49, 0.49, 0.49, 0.56, 0.75, 0.51, 0.42, 0.46, 0.47, 0.43, 0.38, 
                                    0.39, 0.39, 0.41, 0.43, 0.38, 0.41, 0.36, 0.41, 0.38, 0.42, 0.42, 0.37, 0.41, 0.41, 0.34, 0.32, 0.37, 0.32, 0.34, 
                                    0.34, 0.34, 0.32, 0.41, 0.62, 0.33, 0.4, 0.32, 0.32, 0.29, 0.35, 0.32, 0.32, 0.28, 0.26, 0.25, 0.29, 0.26, 0.27, 
                                    0.27, 0.24, 0.28, 0.3, 0.27, 0.23, 0.23, 0.28, 0.25, 0.29, 0.24, 0.21, 0.26, 0.29, 0.23, 0.2, 0.24, 0.25, 0.23, 
                                    0.21, 0.26, 0.38, 0.24, 0.21, 0.25, 0.23, 0.22, 0.22, 0.24, 0.23, 0.23, 0.26, 0.24, 0.28, 0.64, 9.96, 0.15, 0.11, 
                                    0.11, 0.1, 0.1, 0.1, 0.11, 0.11, 0.12, 0.13, 0.12, 0.16, 0.12, 0.13, 0.12, 0.1, 0.13, 0.13, 0.13, 0.25, 0.1, 0.14, 
                                    0.14, 0.12, 0.14, 0.14, 0.17, 0.15, 0.19, 0.38, 0.2, 0.19, 0.24, 0.26, 0.36, 1.58, 1.49, 0.1, 0.2, 1.98, 0.05, 0.08, 
                                    0.07, 0.07, 0.14, 0.08, 0.08, 0.53, 3.06, 3.31};
                                
    m_blockSizeDistribution = std::piecewise_constant_distribution<double> (intervals.begin(), intervals.end(), weights.begin());
  }
  
/*   if (GetNode()->GetId() == 0)
  {
    Block newBlock(1, 0, -1, 500000, 0, 0, Ipv4Address("0.0.0.0"));
    m_blockchain.AddBlock(newBlock); 
  } */
  
  if (m_secureBlocks == 0)
    ScheduleNextMiningEvent ();
}

void 
BitcoinSimpleAttacker::StopApplication ()
{
  BitcoinNode::StopApplication ();  
  Simulator::Cancel (m_nextMiningEvent);
  
  NS_LOG_WARN ("The simple attacker " << GetNode ()->GetId () << " with hash rate = " << m_hashRate << " generated " << m_minerGeneratedBlocks 
                << " blocks "<< "(" << 100. * m_minerGeneratedBlocks / (m_blockchain.GetTotalBlocks() - 1) 
                << "%) with average block generation time = " << m_minerAverageBlockGenInterval
                << "s or " << static_cast<int>(m_minerAverageBlockGenInterval) / m_secondsPerMin << "min and " 
                << m_minerAverageBlockGenInterval - static_cast<int>(m_minerAverageBlockGenInterval) / m_secondsPerMin * m_secondsPerMin << "s"
                << " and average size " << m_minerAverageBlockSize << " Bytes");
				
  m_nodeStats->minerGeneratedBlocks = m_minerGeneratedBlocks;
  m_nodeStats->minerAverageBlockGenInterval = m_minerAverageBlockGenInterval;
  m_nodeStats->minerAverageBlockSize = m_minerAverageBlockSize;
}

void 
BitcoinSimpleAttacker::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  BitcoinMiner::DoDispose ();

}

void 
BitcoinSimpleAttacker::MineBlock (void)  //FIX ME
{
  NS_LOG_FUNCTION (this);
  rapidjson::Document d; 
  int height =  m_minerGeneratedBlocks + 1;
  int minerId = GetNode ()->GetId ();
  int parentBlockMinerId;
  double currentTime = Simulator::Now ().GetSeconds ();
  std::ostringstream stringStream;  
  std::string blockHash = stringStream.str();
  
  d.SetObject();

  if (m_minerGeneratedBlocks == 0)
    parentBlockMinerId = -1;
  else 
	parentBlockMinerId = GetNode ()->GetId ();

  if (height > m_blockchain.GetCurrentTopBlock()->GetBlockHeight())
  {
     NS_LOG_WARN ("The attack was successful");
     m_attackFinished = true;
	 m_nodeStats->attackSuccess = 1;
	 Simulator::Stop (Seconds (0));
  }

    
  rapidjson::Value value(INV);
  rapidjson::Value array(rapidjson::kArrayType);
  d.AddMember("message", value, d.GetAllocator());
  
  value.SetString("block"); //Remove
  d.AddMember("type", value, d.GetAllocator());
  
  stringStream << height << "/" << minerId;
  blockHash = stringStream.str();
  value.SetString(blockHash.c_str(), blockHash.size(), d.GetAllocator());
  array.PushBack(value, d.GetAllocator());
 
/*   stringStream.clear();
  stringStream.str(std::string());
  blockHash.clear();
  
  stringStream << height << "/" << minerId;
  blockHash = stringStream.str();
  value.SetString(blockHash.c_str(), blockHash.size(), d.GetAllocator()); 
  array.PushBack(value, d.GetAllocator()); */
  
  d.AddMember("inv", array, d.GetAllocator());
  
  if (m_fixedBlockSize > 0)
    m_nextBlockSize = m_fixedBlockSize;
  else
    m_nextBlockSize = m_blockSizeDistribution(m_generator) * 1000 * // *1000 because the m_blockSizeDistribution returns KBytes
                      m_averageBlockGenIntervalSeconds / m_realAverageBlockGenIntervalSeconds;	// The block size is linearly dependent on the averageBlockGenIntervalSeconds


  Block newBlock (height, minerId, parentBlockMinerId, m_nextBlockSize,
                  currentTime, currentTime, Ipv4Address("127.0.0.1"));

  /**
   * Update m_meanBlockReceiveTime with the timeCreated of the newly generated block
   */
  m_meanBlockReceiveTime = (m_blockchain.GetTotalBlocks() - 1)/static_cast<double>(m_blockchain.GetTotalBlocks())*m_meanBlockReceiveTime + 
						   (currentTime - m_previousBlockReceiveTime)/(m_blockchain.GetTotalBlocks());
  m_previousBlockReceiveTime = currentTime;	
  
  m_meanBlockPropagationTime = (m_blockchain.GetTotalBlocks() - 1)/static_cast<double>(m_blockchain.GetTotalBlocks())*m_meanBlockPropagationTime;
  
  m_meanBlockSize = (m_blockchain.GetTotalBlocks() - 1)/static_cast<double>(m_blockchain.GetTotalBlocks())*m_meanBlockSize  
                  + (m_nextBlockSize)/static_cast<double>(m_blockchain.GetTotalBlocks());
				  
  m_blockchain.AddBlock(newBlock);
  
  // Stringify the DOM
  rapidjson::StringBuffer packetInfo;
  rapidjson::Writer<rapidjson::StringBuffer> writer(packetInfo);
  d.Accept(writer);
  
  //Ptr<Packet> packet = Create<Packet> (reinterpret_cast<const uint8_t*>(packetInfo.GetString()), packetInfo.GetSize());
  
  for (std::vector<Ipv4Address>::const_iterator i = m_peersAddresses.begin(); i != m_peersAddresses.end(); ++i)
  {
	const uint8_t delimiter[] = "#";

    m_peersSockets[*i]->Send (reinterpret_cast<const uint8_t*>(packetInfo.GetString()), packetInfo.GetSize(), 0);
	m_peersSockets[*i]->Send (delimiter, 1, 0);
	
/* 	//Send large packet
	int k;
	for (k = 0; k < 4; k++)
	{
      ns3TcpSocket->Send (reinterpret_cast<const uint8_t*>(packetInfo.GetString()), packetInfo.GetSize(), 0);
	  ns3TcpSocket->Send (delimiter, 1, 0);
	} */
	

  }

  m_minerAverageBlockGenInterval = m_minerGeneratedBlocks/static_cast<double>(m_minerGeneratedBlocks+1)*m_minerAverageBlockGenInterval 
                             + (Simulator::Now ().GetSeconds () - m_previousBlockGenerationTime)/(m_minerGeneratedBlocks+1);
  m_minerAverageBlockSize = m_minerGeneratedBlocks/static_cast<double>(m_minerGeneratedBlocks+1)*m_minerAverageBlockSize 
                            + static_cast<double>(m_nextBlockSize)/(m_minerGeneratedBlocks+1);
  m_previousBlockGenerationTime = Simulator::Now ().GetSeconds ();
  m_minerGeneratedBlocks++;

  NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
               << "s bitcoin simple attacker " << GetNode ()->GetId () 
               << " sent a packet " << packetInfo.GetString() 
	           << " " << m_minerAverageBlockSize);
	
  if (m_attackFinished == false)	
    ScheduleNextMiningEvent ();

}

void 
BitcoinSimpleAttacker::ReceivedHigherBlock(const Block &newBlock) //FIX ME
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO("Bitcoin simple attacker "<< GetNode ()->GetId () << " added a new block in the m_blockchain with higher height: " << newBlock);
  
  if (m_attackStarted == false && newBlock.GetBlockHeight () >= m_secureBlocks)
  {
	m_attackStarted = true;
    ScheduleNextMiningEvent();
  }
}
} // Namespace ns3

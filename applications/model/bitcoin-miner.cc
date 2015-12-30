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
#include "bitcoin-miner.h"
#include "../../rapidjson/document.h"
#include "../../rapidjson/writer.h"
#include "../../rapidjson/stringbuffer.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BitcoinMiner");

NS_OBJECT_ENSURE_REGISTERED (BitcoinMiner);

TypeId 
BitcoinMiner::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BitcoinMiner")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<BitcoinMiner> ()
    .AddAttribute ("Local",
                   "The Address on which to Bind the rx socket.",
                   AddressValue (),
                   MakeAddressAccessor (&BitcoinMiner::m_local),
                   MakeAddressChecker ())
    .AddAttribute ("Protocol",
                   "The type id of the protocol to use for the rx socket.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&BitcoinMiner::m_tid),
                   MakeTypeIdChecker ())
    .AddAttribute ("FixedBlockSize", 
				   "The fixed size of the block",
                   UintegerValue (500),
                   MakeUintegerAccessor (&BitcoinMiner::m_fixedBlockSize),
                   MakeUintegerChecker<uint32_t> ())				   
    .AddAttribute ("FixedBlockIntervalGeneration", 
                   "The fixed time to wait between two consecutive block generations",
                   DoubleValue (0),
                   MakeDoubleAccessor (&BitcoinMiner::m_fixedBlockTimeGeneration),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("NumberOfPeers", 
				   "The number of peers for the node",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BitcoinMiner::m_numberOfPeers),
                   MakeUintegerChecker<uint32_t> ())	
    .AddAttribute ("HashRate", 
				   "The hash rate of the miner",
                   DoubleValue (0.2),
                   MakeDoubleAccessor (&BitcoinMiner::m_hashRate),
                   MakeDoubleChecker<double> ())	
    .AddAttribute ("BlockGenBinSize", 
				   "The block generation bin size",
                   DoubleValue (1./60),
                   MakeDoubleAccessor (&BitcoinMiner::m_blockGenBinSize),
                   MakeDoubleChecker<double> ())	
    .AddAttribute ("BlockGenParameter", 
				   "The block generation distribution parameter",
                   DoubleValue (0.183/120),
                   MakeDoubleAccessor (&BitcoinMiner::m_blockGenParameter),
                   MakeDoubleChecker<double> ())					   
    .AddTraceSource ("Rx",
                     "A packet has been received",
                     MakeTraceSourceAccessor (&BitcoinMiner::m_rxTrace),
                     "ns3::Packet::AddressTracedCallback")
  ;
  return tid;
}

BitcoinMiner::BitcoinMiner () : BitcoinNode()
{
  NS_LOG_FUNCTION (this);
  m_minerAverageBlockGenTime = 0;
  m_minerGeneratedBlocks = 0;
  
  std::random_device rd;
  m_generator.seed(rd());
  m_blockGenBinSize = 1./60;
  m_blockGenParameter = 0.01;
  
  if (m_fixedBlockTimeGeneration > 0)
	m_nextBlockTime = m_fixedBlockTimeGeneration;  
  else
    m_nextBlockTime = 0;
}

void 
BitcoinMiner::StartApplication ()    // Called at time specified by Start
{
	BitcoinNode::StartApplication ();
	m_blockGenParameter *= m_hashRate;
	ScheduleNextMiningEvent ();
}

void 
BitcoinMiner::StopApplication ()
{
  BitcoinNode::StopApplication ();  
  Simulator::Cancel (m_nextMiningEvent);
  
  NS_LOG_DEBUG ("The miner " << GetNode ()->GetId () << " generated " << m_minerGeneratedBlocks 
				<< " blocks with average block generation time = " << m_minerAverageBlockGenTime
				<< "s or " << static_cast<int>(m_minerAverageBlockGenTime) / 60 << "min and " 
				<< m_minerAverageBlockGenTime - static_cast<int>(m_minerAverageBlockGenTime) / 60 * 60 << "s");
}

double 
BitcoinMiner::GetFixedBlockTimeGeneration(void) const
{
  NS_LOG_FUNCTION (this);
  return m_fixedBlockTimeGeneration;
}

void 
BitcoinMiner::SetFixedBlockTimeGeneration(double fixedBlockTimeGeneration) 
{
  NS_LOG_FUNCTION (this);
  m_fixedBlockTimeGeneration = fixedBlockTimeGeneration;
}

uint32_t 
BitcoinMiner::GetFixedBlockSize(void) const
{
  NS_LOG_FUNCTION (this);
  return m_fixedBlockSize;
}

void 
BitcoinMiner::SetFixedBlockSize(uint32_t fixedBlockSize) 
{
  NS_LOG_FUNCTION (this);
  m_fixedBlockSize = fixedBlockSize;
}

double 
BitcoinMiner::GetBlockGenBinSize(void) const
{
  NS_LOG_FUNCTION (this);
  return m_blockGenBinSize;	
}

void 
BitcoinMiner::SetBlockGenBinSize (double blockGenBinSize)
{
  NS_LOG_FUNCTION (this);
  m_blockGenBinSize = blockGenBinSize;	
}

double 
BitcoinMiner::GetBlockGenParameter(void) const
{
  NS_LOG_FUNCTION (this);
  return m_blockGenParameter;	
}

void 
BitcoinMiner::SetBlockGenParameter (double blockGenParameter)
{
  NS_LOG_FUNCTION (this);
  m_blockGenParameter = blockGenParameter;
}

double 
BitcoinMiner::GetHashRate(void) const
{
  NS_LOG_FUNCTION (this);
  return m_hashRate;	
}

void 
BitcoinMiner::SetHashRate (double hashRate)
{
  NS_LOG_FUNCTION (this);
  m_hashRate = hashRate;
}
 
void
BitcoinMiner::ScheduleNextMiningEvent (void)
{
  NS_LOG_FUNCTION (this);
  
  if(m_fixedBlockTimeGeneration > 0)
  {
	m_nextBlockTime = m_fixedBlockTimeGeneration;

	NS_LOG_LOGIC ("Fixed Block Time Generation " << m_fixedBlockTimeGeneration << "s");
	m_nextMiningEvent = Simulator::Schedule (Seconds(m_fixedBlockTimeGeneration), &BitcoinMiner::SendPacket, this);
  }
  else
  {
	//unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
    //std::default_random_engine m_generator(seed1);
	const int secondsPerMin = 60;
	
	std::geometric_distribution<int> blockSizeDistribution(m_blockGenParameter);
    m_nextBlockTime = blockSizeDistribution(m_generator)*m_blockGenBinSize*secondsPerMin;
	//NS_LOG_DEBUG("m_nextBlockTime = " << m_nextBlockTime << ", binsize = " << m_blockGenBinSize << ", bill = " << bill << ", hashrate = " << m_hashRate);
	m_nextMiningEvent = Simulator::Schedule (Seconds(m_nextBlockTime), &BitcoinMiner::SendPacket, this);
	

	NS_LOG_INFO ("Time " << Simulator::Now ().GetSeconds () << ": Miner " << GetNode ()->GetId () << " will generate a block in " 
				 << m_nextBlockTime << "s or " << static_cast<int>(m_nextBlockTime) / secondsPerMin 
				 << "  min and  " << static_cast<int>(m_nextBlockTime) % secondsPerMin 
				 << "s using Geometric Block Time Generation with parameter = "<< m_blockGenParameter);
  }
}

void 
BitcoinMiner::SendPacket (void)
{
  NS_LOG_FUNCTION (this);
  
  rapidjson::Document d; 
  d.SetObject();
  
  rapidjson::Value value(m_fixedBlockSize);
  d.AddMember("size", value, d.GetAllocator());
  
  value = GetNode ()->GetId ();
  d.AddMember("miner", value, d.GetAllocator());
  
  // Stringify the DOM
  rapidjson::StringBuffer packetInfo;
  rapidjson::Writer<rapidjson::StringBuffer> writer(packetInfo);
  d.Accept(writer);  
  
  Ptr<Packet> packet = Create<Packet> (reinterpret_cast<const uint8_t*>(packetInfo.GetString()), packetInfo.GetSize());
  
  for (std::vector<Address>::const_iterator i = m_peersAddresses.begin(); i != m_peersAddresses.end(); ++i)
  {
    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
    ns3TcpSocket->Connect(*i);
	ns3TcpSocket->Send (packet);
	ns3TcpSocket->Close();
  }

  m_minerAverageBlockGenTime = m_minerGeneratedBlocks/static_cast<double>(m_minerGeneratedBlocks+1)*m_minerAverageBlockGenTime + m_nextBlockTime/(m_minerGeneratedBlocks+1);
  m_minerGeneratedBlocks++;

  NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
			   << "s bitcoin miner " << GetNode ()->GetId () << " sent a packet " << packetInfo.GetString());
			   
  ScheduleNextMiningEvent ();
}

} // Namespace ns3

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
#include "bitcoin-node.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BitcoinNode");

NS_OBJECT_ENSURE_REGISTERED (BitcoinNode);

TypeId 
BitcoinNode::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BitcoinNode")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<BitcoinNode> ()
    .AddAttribute ("Local",
                   "The Address on which to Bind the rx socket.",
                   AddressValue (),
                   MakeAddressAccessor (&BitcoinNode::m_local),
                   MakeAddressChecker ())
    .AddAttribute ("Protocol",
                   "The type id of the protocol to use for the rx socket.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&BitcoinNode::m_tid),
                   MakeTypeIdChecker ())
    .AddAttribute ("InvTimeoutMinutes", 
				   "The timeout of inv messages in minutes",
                   TimeValue (Minutes (20)),
                   MakeTimeAccessor (&BitcoinNode::m_invTimeoutMinutes),
                   MakeTimeChecker())
    .AddTraceSource ("Rx",
                     "A packet has been received",
                     MakeTraceSourceAccessor (&BitcoinNode::m_rxTrace),
                     "ns3::Packet::AddressTracedCallback")
  ;
  return tid;
}

BitcoinNode::BitcoinNode (void) : m_bitcoinPort (8333), m_secondsPerMin(60), m_isMiner (false), m_countBytes (4),
                                  m_inventorySizeBytes (36), m_getHeadersSizeBytes (72), m_headersSizeBytes (81), m_blockHeadersSizeBytes (80)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_meanBlockReceiveTime = 0;
  m_previousBlockReceiveTime = 0;
  m_meanBlockPropagationTime = 0;
  m_meanBlockSize = 0;
  m_numberOfPeers = m_peersAddresses.size();
  
}

BitcoinNode::~BitcoinNode(void)
{
  NS_LOG_FUNCTION (this);
}

Ptr<Socket>
BitcoinNode::GetListeningSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

  
std::vector<Ipv4Address> 
BitcoinNode::GetPeersAddresses (void) const
{
  NS_LOG_FUNCTION (this);
  return m_peersAddresses;
}


void 
BitcoinNode::SetPeersAddresses (const std::vector<Ipv4Address> &peers)
{
  NS_LOG_FUNCTION (this);
  m_peersAddresses = peers;
  m_numberOfPeers = m_peersAddresses.size();
}


void 
BitcoinNode::SetPeersDownloadSpeeds (const std::map<Ipv4Address, double> &peersDownloadSpeeds)
{
  NS_LOG_FUNCTION (this);
  m_peersDownloadSpeeds = peersDownloadSpeeds;
}


void 
BitcoinNode::SetNodeInternetSpeeds (const nodeInternetSpeeds &internetSpeeds)
{
  NS_LOG_FUNCTION (this);

  m_downloadSpeed = internetSpeeds.downloadSpeed * 1000000 / 8 ;
  m_uploadSpeed = internetSpeeds.uploadSpeed * 1000000 / 8 ; 
}
  
  
void 
BitcoinNode::SetNodeStats (nodeStatistics *nodeStats)
{
  NS_LOG_FUNCTION (this);
  m_nodeStats = nodeStats;
}


void 
BitcoinNode::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;

  // chain up
  Application::DoDispose ();
}


// Application Methods
void 
BitcoinNode::StartApplication ()    // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);
  // Create the socket if not already
  NS_LOG_WARN ("Node " << GetNode()->GetId() << ": download speed = " << m_downloadSpeed << " Mbps");
  NS_LOG_WARN ("Node " << GetNode()->GetId() << ": upload speed = " << m_uploadSpeed << " Mbps");
  NS_LOG_INFO ("Node " << GetNode()->GetId() << ": m_numberOfPeers = " << m_numberOfPeers);
  NS_LOG_INFO ("Node " << GetNode()->GetId() << ": m_invTimeoutMinutes = " << m_invTimeoutMinutes.GetMinutes() << "mins");
  NS_LOG_INFO ("Node " << GetNode()->GetId() << ": My peers are");
  
  for (auto it = m_peersAddresses.begin(); it != m_peersAddresses.end(); it++)
    NS_LOG_INFO("\t" << *it);

  double currentMax = 0;
  
  for(auto it = m_peersDownloadSpeeds.begin(); it != m_peersDownloadSpeeds.end(); ++it ) 
  {
	//std::cout << "Node " << GetNode()->GetId() << ": peer " << it->first << "download speed = " << it->second << " Mbps" << std::endl;
  }
  
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);
      m_socket->Bind (m_local);
      m_socket->Listen ();
      m_socket->ShutdownSend ();
      if (addressUtils::IsMulticast (m_local))
        {
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, m_local);
            }
          else
            {
              NS_FATAL_ERROR ("Error: joining multicast on a non-UDP socket");
            }
        }
    }

  m_socket->SetRecvCallback (MakeCallback (&BitcoinNode::HandleRead, this));
  m_socket->SetAcceptCallback (
    MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
    MakeCallback (&BitcoinNode::HandleAccept, this));
  m_socket->SetCloseCallbacks (
    MakeCallback (&BitcoinNode::HandlePeerClose, this),
    MakeCallback (&BitcoinNode::HandlePeerError, this));
	
  NS_LOG_DEBUG ("Node " << GetNode()->GetId() << ": Before creating sockets");
  for (std::vector<Ipv4Address>::const_iterator i = m_peersAddresses.begin(); i != m_peersAddresses.end(); ++i)
  {
    m_peersSockets[*i] = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
	m_peersSockets[*i]->Connect (InetSocketAddress (*i, m_bitcoinPort));
  }
  NS_LOG_DEBUG ("Node " << GetNode()->GetId() << ": After creating sockets");

  m_nodeStats->nodeId = GetNode ()->GetId ();
  m_nodeStats->meanBlockReceiveTime = 0;
  m_nodeStats->meanBlockPropagationTime = 0;
  m_nodeStats->meanBlockSize = 0;
  m_nodeStats->totalBlocks = 0;
  m_nodeStats->staleBlocks = 0;
  m_nodeStats->miner = 0;
  m_nodeStats->minerGeneratedBlocks = 0;
  m_nodeStats->minerAverageBlockGenInterval = 0;
  m_nodeStats->minerAverageBlockSize = 0;
  m_nodeStats->hashRate = 0;
  m_nodeStats->attackSuccess = 0;
  m_nodeStats->invReceivedBytes = 0;
  m_nodeStats->invSentBytes = 0;
  m_nodeStats->getHeadersReceivedBytes = 0;
  m_nodeStats->getHeadersSentBytes = 0;
  m_nodeStats->headersReceivedBytes = 0;
  m_nodeStats->headersSentBytes = 0;
  m_nodeStats->getDataReceivedBytes = 0;
  m_nodeStats->getDataSentBytes = 0;
  m_nodeStats->blockReceivedBytes = 0;
  m_nodeStats->blockSentBytes = 0;
  m_nodeStats->longestFork = 0;
  m_nodeStats->blocksInForks = 0;
  m_nodeStats->connections = m_peersAddresses.size();
}

void 
BitcoinNode::StopApplication ()     // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);

  for (auto &elem : m_peersSockets)
    NS_LOG_DEBUG("\t" << elem.first);

  for (std::vector<Ipv4Address>::iterator i = m_peersAddresses.begin(); i != m_peersAddresses.end(); ++i) //close the outgoing sockets
  {
	m_peersSockets[*i]->Close ();
  }
  

  if (m_socket) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }

  NS_LOG_WARN("\n\nBITCOIN NODE " << GetNode ()->GetId () << ":");
  NS_LOG_WARN ("Current Top Block is:\n" << *(m_blockchain.GetCurrentTopBlock()));
  NS_LOG_INFO("Current Blockchain is:\n" << m_blockchain);
  //m_blockchain.PrintOrphans();
  //PrintQueueInv();
  //PrintInvTimeouts();
  
  NS_LOG_WARN("Mean Block Receive Time = " << m_meanBlockReceiveTime << " or " 
               << static_cast<int>(m_meanBlockReceiveTime) / m_secondsPerMin << "min and " 
			   << m_meanBlockReceiveTime - static_cast<int>(m_meanBlockReceiveTime) / m_secondsPerMin * m_secondsPerMin << "s");
  NS_LOG_WARN("Mean Block Propagation Time = " << m_meanBlockPropagationTime << "s");
  NS_LOG_WARN("Mean Block Size = " << m_meanBlockSize << " Bytes");
  NS_LOG_WARN("Total Blocks = " << m_blockchain.GetTotalBlocks());
  NS_LOG_WARN("Stale Blocks = " << m_blockchain.GetNoStaleBlocks() << " (" 
              << 100. * m_blockchain.GetNoStaleBlocks() / m_blockchain.GetTotalBlocks() << "%)");
  NS_LOG_WARN("receivedButNotValidated size = " << m_receivedNotValidated.size());
  NS_LOG_WARN("m_sendBlockTimes size = " << m_sendBlockTimes.size());
  NS_LOG_WARN("longest fork = " << m_blockchain.GetLongestForkSize());
  NS_LOG_WARN("blocks in forks = " << m_blockchain.GetBlocksInForks());
  
  m_nodeStats->meanBlockReceiveTime = m_meanBlockReceiveTime;
  m_nodeStats->meanBlockPropagationTime = m_meanBlockPropagationTime;
  m_nodeStats->meanBlockSize = m_meanBlockSize;
  m_nodeStats->totalBlocks = m_blockchain.GetTotalBlocks();
  m_nodeStats->staleBlocks = m_blockchain.GetNoStaleBlocks();
  m_nodeStats->longestFork = m_blockchain.GetLongestForkSize();
  m_nodeStats->blocksInForks = m_blockchain.GetBlocksInForks();
  
/*     std::cout << "\nNode " << m_nodeStats->nodeId << " statistics:\n";
    std::cout << "Mean Block Receive Time = " << m_nodeStats->meanBlockReceiveTime << " or " 
              << static_cast<int>(m_nodeStats->meanBlockReceiveTime) / m_secondsPerMin << "min and " 
			  << m_nodeStats->meanBlockReceiveTime - static_cast<int>(m_nodeStats->meanBlockReceiveTime) / m_secondsPerMin * m_secondsPerMin << "s\n";
    std::cout << "Mean Block Propagation Time = " << m_nodeStats->meanBlockPropagationTime << "s\n";
    std::cout << "Mean Block Size = " << m_nodeStats->meanBlockSize << " Bytes\n";
    std::cout << "Total Blocks = " << m_nodeStats->totalBlocks << "\n";
    std::cout << "Stale Blocks = " << m_nodeStats->staleBlocks << " (" 
              << 100. * m_nodeStats->staleBlocks / m_nodeStats->totalBlocks << "%)\n"; */
}

void 
BitcoinNode::HandleRead (Ptr<Socket> socket)
{	
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  double newBlockReceiveTime = Simulator::Now ().GetSeconds();

  while ((packet = socket->RecvFrom (from)))
  {
      if (packet->GetSize () == 0)
      { //EOF
         break;
      }

      if (InetSocketAddress::IsMatchingType (from))
      {
        /**
         * We may receive more than one packets simultaneously on the socket,
         * so we have to parse each one of them.
         */
        std::string delimiter = "#";
        std::string parsedPacket;
        size_t pos = 0;
        char *packetInfo = new char[packet->GetSize () + 1];
		std::ostringstream totalStream;
		
        packet->CopyData (reinterpret_cast<uint8_t*>(packetInfo), packet->GetSize ());
        packetInfo[packet->GetSize ()] = '\0'; // ensure that it is null terminated to avoid bugs
		  
		/**
         * Add the buffered data to complete the packet
        */
        totalStream << m_bufferedData[from] << packetInfo; 
        std::string totalReceivedData(totalStream.str());
        NS_LOG_INFO("Node " << GetNode ()->GetId () << " Total Received Data: " << totalReceivedData);
		  
        while ((pos = totalReceivedData.find(delimiter)) != std::string::npos) 
        {
          parsedPacket = totalReceivedData.substr(0, pos);
          NS_LOG_INFO("Node " << GetNode ()->GetId () << " Parsed Packet: " << parsedPacket);
		  
          rapidjson::Document d;
		  d.Parse(parsedPacket.c_str());
		  
		  if(!d.IsObject())
          {
            NS_LOG_WARN("The parsed packet is corrupted");
            totalReceivedData.erase(0, pos + delimiter.length()); 
            continue;
          }			
		  
          rapidjson::StringBuffer buffer;
          rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
          d.Accept(writer);
		  
          NS_LOG_INFO ("At time "  << Simulator::Now ().GetSeconds ()
                        << "s bitcoin node " << GetNode ()->GetId () << " received "
                        <<  packet->GetSize () << " bytes from "
                        << InetSocketAddress::ConvertFrom(from).GetIpv4 ()
                        << " port " << InetSocketAddress::ConvertFrom (from).GetPort () 
		        	    << " with info = " << buffer.GetString());	
						
          switch (d["message"].GetInt())
          {
            case INV:
            {
              //NS_LOG_INFO ("INV");
			  int j;
			  std::vector<std::string>            requestBlocks;
			  std::vector<std::string>::iterator  block_it;
			  
              m_nodeStats->invReceivedBytes += m_countBytes + d["inv"].Size()*m_inventorySizeBytes;
			  
			  for (j=0; j<d["inv"].Size(); j++)
			  {  
			    std::string   invDelimiter = "/";
				std::string   parsedInv = d["inv"][j].GetString();
				size_t        invPos = parsedInv.find(invDelimiter);
			    EventId       timeout;

				int height = atoi(parsedInv.substr(0, invPos).c_str());
				int minerId = atoi(parsedInv.substr(invPos+1, parsedInv.size()).c_str());
				  
                								  
                if (m_blockchain.HasBlock(height, minerId) || ReceivedButNotValidated(parsedInv))
                {
                  NS_LOG_INFO("INV: Bitcoin node " << GetNode ()->GetId () 
				  << " has already received the block with height = " 
				  << height << " and minerId = " << minerId);				  
                }
                else
                {
                  NS_LOG_INFO("INV: Bitcoin node " << GetNode ()->GetId () 
                  << " does not have the block with height = " 
                  << height << " and minerId = " << minerId);
				  
                  /**
                   * Check if we have already request the block
                   */
				   
                  if (m_queueInv.find(parsedInv) == m_queueInv.end())
                  {
                    NS_LOG_INFO("INV: Bitcoin node " << GetNode ()->GetId ()
                                 << " has not requested the block yet");
                    requestBlocks.push_back(parsedInv);
                    timeout = Simulator::Schedule (m_invTimeoutMinutes, &BitcoinNode::InvTimeoutExpired, this, parsedInv);
                    m_invTimeouts[parsedInv] = timeout;
                  }
                  else
                  {
                    NS_LOG_INFO("INV: Bitcoin node " << GetNode ()->GetId ()
                                 << " has already requested the block");
                  }
				  
                  m_queueInv[parsedInv].push_back(from);
                  //PrintQueueInv();
				  //PrintInvTimeouts();
                }								  
              }
			
              if (!requestBlocks.empty())
              {
                rapidjson::Value   value;
                rapidjson::Value   array(rapidjson::kArrayType);
                d.RemoveMember("inv");

                for (block_it = requestBlocks.begin(); block_it < requestBlocks.end(); block_it++) 
                {
                  value.SetString(block_it->c_str(), block_it->size(), d.GetAllocator());
                  array.PushBack(value, d.GetAllocator());
                }		
			  
                d.AddMember("blocks", array, d.GetAllocator());
					
                SendMessage(INV, GET_HEADERS, d, from);				
                SendMessage(INV, GET_DATA, d, from);	
				
			  }
              break;
            }
            case GET_HEADERS:
            {
              int j;
              std::vector<Block>              requestBlocks;
              std::vector<Block>::iterator    block_it;
			  
			  m_nodeStats->getHeadersReceivedBytes += m_getHeadersSizeBytes;
			  
              for (j=0; j<d["blocks"].Size(); j++)
              {  
                std::string   invDelimiter = "/";
                std::string   parsedInv = d["blocks"][j].GetString();
                size_t        invPos = parsedInv.find(invDelimiter);
				  
                int height = atoi(parsedInv.substr(0, invPos).c_str());
                int minerId = atoi(parsedInv.substr(invPos+1, parsedInv.size()).c_str());
				
                if (m_blockchain.HasBlock(height, minerId) || m_blockchain.IsOrphan(height, minerId))
                {
                  NS_LOG_INFO("GET_HEADERS: Bitcoin node " << GetNode ()->GetId () 
                  << " has the block with height = " 
                  << height << " and minerId = " << minerId);
                  Block newBlock (m_blockchain.ReturnBlock (height, minerId));
                  requestBlocks.push_back(newBlock);
                }
                else
                {
                  NS_LOG_INFO("GET_HEADERS: Bitcoin node " << GetNode ()->GetId () 
                  << " does not have the block with height = " 
                  << height << " and minerId = " << minerId);                
                }	
              }
			  
              if (!requestBlocks.empty())
              {
                rapidjson::Value value;
                rapidjson::Value array(rapidjson::kArrayType);
                rapidjson::Value blockInfo(rapidjson::kObjectType);

                d.RemoveMember("blocks");
				
                for (block_it = requestBlocks.begin(); block_it < requestBlocks.end(); block_it++) 
                {
                  NS_LOG_INFO ("In requestBlocks " << *block_it);
    
                  value = block_it->GetBlockHeight ();
                  blockInfo.AddMember("height", value, d.GetAllocator ());
  
                  value = block_it->GetMinerId ();
                  blockInfo.AddMember("minerId", value, d.GetAllocator ());

                  value = block_it->GetParentBlockMinerId ();
                  blockInfo.AddMember("parentBlockMinerId", value, d.GetAllocator ());
  
                  value = block_it->GetBlockSizeBytes ();
                  blockInfo.AddMember("size", value, d.GetAllocator ());
  
                  value = block_it->GetTimeCreated ();
                  blockInfo.AddMember("timeCreated", value, d.GetAllocator ());
  
                  value = block_it->GetTimeReceived ();							
                  blockInfo.AddMember("timeReceived", value, d.GetAllocator ());
				  
                  array.PushBack(blockInfo, d.GetAllocator());
                }	
				
                d.AddMember("blocks", array, d.GetAllocator());
				
                SendMessage(GET_HEADERS, HEADERS, d, from);
              }
              break;
            }
            case GET_DATA:
            {
              NS_LOG_INFO ("GET_DATA");
			  
              int j;
              int totalBlockMessageSize = 0;
              std::vector<Block>              requestBlocks;
              std::vector<Block>::iterator    block_it;

              m_nodeStats->getDataReceivedBytes += m_countBytes + d["blocks"].Size()*m_inventorySizeBytes;

              for (j=0; j<d["blocks"].Size(); j++)
              {  
                std::string    invDelimiter = "/";
                std::string    parsedInv = d["blocks"][j].GetString();
                size_t         invPos = parsedInv.find(invDelimiter);
				  
                int height = atoi(parsedInv.substr(0, invPos).c_str());
                int minerId = atoi(parsedInv.substr(invPos+1, parsedInv.size()).c_str());
				
                if (m_blockchain.HasBlock(height, minerId) || ReceivedButNotValidated(parsedInv))
                {
                  NS_LOG_INFO("GET_DATA: Bitcoin node " << GetNode ()->GetId () 
                  << " has already received the block with height = " 
                  << height << " and minerId = " << minerId);
                  Block newBlock (m_blockchain.ReturnBlock (height, minerId));
                  requestBlocks.push_back(newBlock);
                }
                else
                {
                  NS_LOG_INFO("GET_DATA: Bitcoin node " << GetNode ()->GetId () 
                  << " does not have the block with height = " 
                  << height << " and minerId = " << minerId);                
                }	
              }
			  
              if (!requestBlocks.empty())
              {
                rapidjson::Value value;
                rapidjson::Value array(rapidjson::kArrayType);
                rapidjson::Value blockInfo(rapidjson::kObjectType);

                d.RemoveMember("blocks");
				
                for (block_it = requestBlocks.begin(); block_it < requestBlocks.end(); block_it++) 
                {
                  NS_LOG_INFO ("In requestBlocks " << *block_it);
    
                  value = block_it->GetBlockHeight ();
                  blockInfo.AddMember("height", value, d.GetAllocator ());
  
                  value = block_it->GetMinerId ();
                  blockInfo.AddMember("minerId", value, d.GetAllocator ());

                  value = block_it->GetParentBlockMinerId ();
                  blockInfo.AddMember("parentBlockMinerId", value, d.GetAllocator ());
  
                  value = block_it->GetBlockSizeBytes ();
				  totalBlockMessageSize += value.GetInt();
                  blockInfo.AddMember("size", value, d.GetAllocator ());
                  
  
                  value = block_it->GetTimeCreated ();
                  blockInfo.AddMember("timeCreated", value, d.GetAllocator ());
  
                  value = block_it->GetTimeReceived ();							
                  blockInfo.AddMember("timeReceived", value, d.GetAllocator ());
				  
                  array.PushBack(blockInfo, d.GetAllocator());
                }	
				
                d.AddMember("blocks", array, d.GetAllocator());
				
		        double sendTime;
		        double eventTime;
			  
/* 				std::cout << "Node " << GetNode()->GetId() << "-" << InetSocketAddress::ConvertFrom(from).GetIpv4 () 
				            << " " << m_peersDownloadSpeeds[InetSocketAddress::ConvertFrom(from).GetIpv4 ()] << " Mbps , time = "
							<< Simulator::Now ().GetSeconds() << "s \n"; */
                
                if (m_sendBlockTimes.size() == 0 || Simulator::Now ().GetSeconds() >  m_sendBlockTimes.back())
                {
                  sendTime = totalBlockMessageSize / m_uploadSpeed; //FIX ME: constant MB/s
                  eventTime = totalBlockMessageSize / std::min(m_uploadSpeed, m_peersDownloadSpeeds[InetSocketAddress::ConvertFrom(from).GetIpv4 ()]
                                                                              * 1000000 / 8); //FIX ME: constant MB/s
                }
                else
                {
				  //std::cout << "m_sendBlockTimes.back() = m_sendBlockTimes.back() = " << m_sendBlockTimes.back() << std::endl;
                  sendTime = totalBlockMessageSize / m_uploadSpeed + m_sendBlockTimes.back() - Simulator::Now ().GetSeconds(); //FIX ME: constant MB/s
                  eventTime = totalBlockMessageSize / std::min(m_uploadSpeed, m_peersDownloadSpeeds[InetSocketAddress::ConvertFrom(from).GetIpv4 ()] * 1000000 / 8)
                           + m_sendBlockTimes.back() - Simulator::Now ().GetSeconds(); //FIX ME: constant MB/s
                }
                m_sendBlockTimes.push_back(Simulator::Now ().GetSeconds() + sendTime);
 
                //std::cout << sendTime << " " << eventTime << " " << m_sendBlockTimes.size() << std::endl;
				NS_LOG_INFO("Node " << GetNode()->GetId() << " will send the block to " << InetSocketAddress::ConvertFrom(from).GetIpv4 () 
				            << " at " << Simulator::Now ().GetSeconds() + sendTime << "\n");
							
               
                // Stringify the DOM
                rapidjson::StringBuffer packetInfo;
                rapidjson::Writer<rapidjson::StringBuffer> writer(packetInfo);
                d.Accept(writer);
				std::string packet = packetInfo.GetString();
				NS_LOG_INFO ("DEBUG: " << packetInfo.GetString());
				
                Simulator::Schedule (Seconds(eventTime), &BitcoinNode::SendBlock, this, packet, from);
                
              }
              break;
            }
            case HEADERS:
            {
              NS_LOG_INFO ("HEADERS");

              std::vector<std::string>              requestBlocks;
              std::vector<std::string>::iterator    block_it;
              int j;

              m_nodeStats->headersReceivedBytes += m_countBytes + d["blocks"].Size()*m_headersSizeBytes;

              for (j=0; j<d["blocks"].Size(); j++)
              {  
                int parentHeight = d["blocks"][j]["height"].GetInt() - 1;
                int parentMinerId = d["blocks"][j]["parentBlockMinerId"].GetInt();

                EventId              timeout;
                std::ostringstream   stringStream;  
                std::string          blockHash = stringStream.str();
				
                stringStream << parentHeight << "/" << parentMinerId;
                blockHash = stringStream.str();
				
                if (!m_blockchain.HasBlock(parentHeight, parentMinerId) && !ReceivedButNotValidated(blockHash))
                {				  
                  NS_LOG_INFO("The Block with height = " << d["blocks"][j]["height"].GetInt() 
                               << " and minerId = " << d["blocks"][j]["minerId"].GetInt() 
                               << " is an orphan\n");
				  
                  /**
                   * Acquire parent
                   */
	  
                  if (m_queueInv.find(blockHash) == m_queueInv.end())
                  {
                    NS_LOG_INFO("INV: Bitcoin node " << GetNode ()->GetId ()
                                 << " has not requested its parent block yet");
                    requestBlocks.push_back(blockHash.c_str());
                    timeout = Simulator::Schedule (m_invTimeoutMinutes, &BitcoinNode::InvTimeoutExpired, this, blockHash);
                    m_invTimeouts[blockHash] = timeout;
                  }
                  else
                  {
                    NS_LOG_INFO("INV: Bitcoin node " << GetNode ()->GetId ()
                                 << " has already requested the block");
                  }
				  
                  m_queueInv[blockHash].push_back(from); //FIX ME: check if it should be placed inside if
                  //PrintQueueInv();
				  //PrintInvTimeouts();
				  
                }
                else
                {
				  /**
	               * Block is not orphan, so we can go on validating
	               */
				  NS_LOG_INFO("The Block with height = " << d["blocks"][j]["height"].GetInt() 
				               << " and minerId = " << d["blocks"][j]["minerId"].GetInt() 
							   << " is NOT an orphan\n");			   
                }
              }
			  
              if (!requestBlocks.empty())
              {
                rapidjson::Value   value;
                rapidjson::Value   array(rapidjson::kArrayType);
                Time               timeout;

                d.RemoveMember("blocks");

                for (block_it = requestBlocks.begin(); block_it < requestBlocks.end(); block_it++) 
                {
                  value.SetString(block_it->c_str(), block_it->size(), d.GetAllocator());
                  array.PushBack(value, d.GetAllocator());
                }		
			  
                d.AddMember("blocks", array, d.GetAllocator());

					
                SendMessage(HEADERS, GET_HEADERS, d, from);			
                SendMessage(HEADERS, GET_DATA, d, from);	
              }
              break;
            }
            case BLOCK:
            {
              NS_LOG_INFO ("BLOCK");
              int j;
              double fullBlockReceiveTime = 0;      

              for (j=0; j<d["blocks"].Size(); j++)
              {  
/* 		        double bandwidth = m_peersDownloadSpeeds[InetSocketAddress::ConvertFrom(from).GetIpv4 ()] * 1000000 / 8; //m_bandwidth in Mbps, bandwidth in B/s
				
				NS_LOG_INFO("Node " << GetNode()->GetId() << "-" << InetSocketAddress::ConvertFrom(from).GetIpv4 () 
				            << " " << m_peersDownloadSpeeds[InetSocketAddress::ConvertFrom(from).GetIpv4 ()] << " Mbps\n");
							
                fullBlockReceiveTime = d["blocks"][j]["size"].GetInt() / bandwidth + fullBlockReceiveTime; //FIX ME: constant MB/s
                Block newBlock (d["blocks"][j]["height"].GetInt(), d["blocks"][j]["minerId"].GetInt(), d["blocks"][j]["parentBlockMinerId"].GetInt(), 
                                d["blocks"][j]["size"].GetInt(), d["blocks"][j]["timeCreated"].GetDouble(), 
                                Simulator::Now ().GetSeconds () + fullBlockReceiveTime, InetSocketAddress::ConvertFrom(from).GetIpv4 ());

                Simulator::Schedule (Seconds(fullBlockReceiveTime), &BitcoinNode::ReceiveBlock, this, newBlock);
                NS_LOG_INFO("The full block " << newBlock << " will be received in " << fullBlockReceiveTime << "s"); */
				
                m_nodeStats->blockReceivedBytes += m_blockHeadersSizeBytes + m_countBytes + d["blocks"][j]["size"].GetInt();
                Block newBlock (d["blocks"][j]["height"].GetInt(), d["blocks"][j]["minerId"].GetInt(), d["blocks"][j]["parentBlockMinerId"].GetInt(), 
                                d["blocks"][j]["size"].GetInt(), d["blocks"][j]["timeCreated"].GetDouble(), 
                                Simulator::Now ().GetSeconds (), InetSocketAddress::ConvertFrom(from).GetIpv4 ());

                ReceiveBlock (newBlock);
				
              }
              break;
            }
            default:
              NS_LOG_INFO ("Default");
              break;
          }
			
          totalReceivedData.erase(0, pos + delimiter.length());
        }
		
		/**
         * Buffer the remaining data
         */
		 
		m_bufferedData[from] = totalReceivedData;
        delete[] packetInfo;
      }
      else if (Inet6SocketAddress::IsMatchingType (from))
      {
        NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                     << "s bitcoin node " << GetNode ()->GetId () << " received "
                     <<  packet->GetSize () << " bytes from "
                     << Inet6SocketAddress::ConvertFrom(from).GetIpv6 ()
                     << " port " << Inet6SocketAddress::ConvertFrom (from).GetPort ());
      }
      m_rxTrace (packet, from);
  }
}

void 
BitcoinNode::ReceiveBlock(const Block &newBlock) 
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("ReceiveBlock: At time " << Simulator::Now ().GetSeconds ()
                << "s bitcoin node " << GetNode ()->GetId () << " received " << newBlock);

  std::ostringstream   stringStream;  
  std::string          blockHash = stringStream.str();
				
  stringStream << newBlock.GetBlockHeight() << "/" << newBlock.GetParentBlockMinerId();
  blockHash = stringStream.str();
  
  if (m_blockchain.HasBlock(newBlock))
  {
    NS_LOG_INFO("ReceiveBlock: Bitcoin node " << GetNode ()->GetId () << " has already added this block in the m_blockchain: " << newBlock);
    
	if (m_queueInv.find(blockHash) == m_queueInv.end())
    {
      m_queueInv.erase(blockHash);
      Simulator::Cancel (m_invTimeouts[blockHash]);
      m_invTimeouts.erase(blockHash);
    }
  }
  else
  {
	std::ostringstream stringStream;
    stringStream << newBlock.GetBlockHeight() << "/" << newBlock.GetMinerId();
    std::string blockHash = stringStream.str();
	
    m_receivedNotValidated.push_back(blockHash);
	//PrintQueueInv();
	//PrintInvTimeouts();
	
    m_queueInv.erase(blockHash);
    Simulator::Cancel (m_invTimeouts[blockHash]);
    m_invTimeouts.erase(blockHash);
	
    //PrintQueueInv();
	//PrintInvTimeouts();
    ValidateBlock (newBlock);
  }

}


void 
BitcoinNode::SendBlock(std::string packetInfo, Address& from) 
{
  NS_LOG_FUNCTION (this);
  
  
  NS_LOG_INFO ("SendBlock: At time " << Simulator::Now ().GetSeconds ()
                << "s bitcoin node " << GetNode ()->GetId () << " send " 
                << packetInfo << " to " << InetSocketAddress::ConvertFrom(from).GetIpv4 ());
				
  m_sendBlockTimes.erase(m_sendBlockTimes.begin());				
  SendMessage(GET_DATA, BLOCK, packetInfo, from);
}


void 
BitcoinNode::ReceivedHigherBlock(const Block &newBlock) 
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO("ReceivedHigherBlock: Bitcoin node " << GetNode ()->GetId () << " added a new block in the m_blockchain with higher height: " << newBlock);
}


void 
BitcoinNode::ValidateBlock(const Block &newBlock) 
{
  NS_LOG_FUNCTION (this);
  
  const Block *parent = m_blockchain.GetParent(newBlock);
  
  if (parent == nullptr)
  {
    NS_LOG_INFO("ValidateBlock: Block " << newBlock << " is an orphan\n"); 
	 
	 m_blockchain.AddOrphan(newBlock);
	 //m_blockchain.PrintOrphans();
  }
  else 
  {
    NS_LOG_INFO("ValidateBlock: Block's " << newBlock << " parent is " << *parent << "\n");

	/**
	 * Block is not orphan, so we can go on validating
	 */	
	 
	const int averageBlockSizeBytes = 458263;
	const double averageValidationTimeSeconds = 0.174;
	double validationTime = averageValidationTimeSeconds * newBlock.GetBlockSizeBytes() / averageBlockSizeBytes;		
	
    Simulator::Schedule (Seconds(validationTime), &BitcoinNode::AfterBlockValidation, this, newBlock);
    NS_LOG_INFO ("ValidateBlock: The Block " << newBlock << " will be validated in " 
	              << validationTime << "s");
  }  

}

void 
BitcoinNode::AfterBlockValidation(const Block &newBlock) 
{
  NS_LOG_FUNCTION (this);

  int height = newBlock.GetBlockHeight();
  int minerId = newBlock.GetMinerId();
  std::ostringstream   stringStream;  
  std::string          blockHash = stringStream.str();
				
  stringStream << height << "/" << minerId;
  blockHash = stringStream.str();
  
  RemoveReceivedButNotValidated(blockHash);
  
  NS_LOG_INFO ("AfterBlockValidation: At time " << Simulator::Now ().GetSeconds ()
               << "s bitcoin node " << GetNode ()->GetId () 
			   << " validated block " <<  newBlock);
			   
  if (newBlock.GetBlockHeight() > m_blockchain.GetBlockchainHeight())
    ReceivedHigherBlock(newBlock);

  if (m_blockchain.IsOrphan(newBlock))
  {
    NS_LOG_INFO ("AfterBlockValidation: Block " << newBlock << " was orphan");
	m_blockchain.RemoveOrphan(newBlock);
  }

  /**
   * Add Block in the blockchain.
   * Update m_meanBlockReceiveTime with the timeReceived of the newly received block.
   */
   
  m_meanBlockReceiveTime = (m_blockchain.GetTotalBlocks() - 1)/static_cast<double>(m_blockchain.GetTotalBlocks())*m_meanBlockReceiveTime 
                         + (newBlock.GetTimeReceived() - m_previousBlockReceiveTime)/(m_blockchain.GetTotalBlocks());
  m_previousBlockReceiveTime = newBlock.GetTimeReceived();
  
  m_meanBlockPropagationTime = (m_blockchain.GetTotalBlocks() - 1)/static_cast<double>(m_blockchain.GetTotalBlocks())*m_meanBlockPropagationTime  
                             + (newBlock.GetTimeReceived() - newBlock.GetTimeCreated())/(m_blockchain.GetTotalBlocks());
							 
  m_meanBlockSize = (m_blockchain.GetTotalBlocks() - 1)/static_cast<double>(m_blockchain.GetTotalBlocks())*m_meanBlockSize  
                  + (newBlock.GetBlockSizeBytes())/static_cast<double>(m_blockchain.GetTotalBlocks());
				  
  m_blockchain.AddBlock(newBlock);
  
  AdvertiseNewBlock(newBlock);//////////////////////////////////////
  ValidateOrphanChildren(newBlock);
}  


void 
BitcoinNode::ValidateOrphanChildren(const Block &newBlock) 
{
  NS_LOG_FUNCTION (this);

  std::vector<const Block *> children = m_blockchain.GetOrphanChildrenPointers(newBlock);

  if (children.size() == 0)
  {
    NS_LOG_INFO("ValidateOrphanChildren: Block " << newBlock << " has no orphan children\n");
  }
  else 
  {
    std::vector<const Block *>::iterator  block_it;
	NS_LOG_INFO("ValidateOrphanChildren: Block " << newBlock << " has orphan children:");
	
	for (block_it = children.begin();  block_it < children.end(); block_it++)
    {
       NS_LOG_INFO ("\t" << **block_it);
	   ValidateBlock (**block_it);
    }
  }
}


void 
BitcoinNode::AdvertiseNewBlock (const Block &newBlock) 
{
  NS_LOG_FUNCTION (this);

  rapidjson::Document d;
  rapidjson::Value value(INV);
  rapidjson::Value array(rapidjson::kArrayType);  
  std::ostringstream stringStream;  
  std::string blockHash = stringStream.str();
  d.SetObject();
  
  d.AddMember("message", value, d.GetAllocator());
  
  value.SetString("block");
  d.AddMember("type", value, d.GetAllocator());
  
  stringStream << newBlock.GetBlockHeight () << "/" << newBlock.GetMinerId ();
  blockHash = stringStream.str();
  value.SetString(blockHash.c_str(), blockHash.size(), d.GetAllocator());
  array.PushBack(value, d.GetAllocator());
  d.AddMember("inv", array, d.GetAllocator());

  // Stringify the DOM
  rapidjson::StringBuffer packetInfo;
  rapidjson::Writer<rapidjson::StringBuffer> writer(packetInfo);
  d.Accept(writer);
  
  for (std::vector<Ipv4Address>::const_iterator i = m_peersAddresses.begin(); i != m_peersAddresses.end(); ++i)
  {
	
    if ( *i != newBlock.GetReceivedFromIpv4 () )
    {
	  const uint8_t delimiter[] = "#";

      m_peersSockets[*i]->Send (reinterpret_cast<const uint8_t*>(packetInfo.GetString()), packetInfo.GetSize(), 0);
	  m_peersSockets[*i]->Send (delimiter, 1, 0);
	  
      m_nodeStats->invSentBytes += m_countBytes + d["inv"].Size()*m_inventorySizeBytes;
      NS_LOG_INFO ("AdvertiseNewBlock: At time " << Simulator::Now ().GetSeconds ()
                   << "s bitcoin node " << GetNode ()->GetId () << " advertised a new Block: " 
                   << newBlock << " to " << *i);
    }
  }
 
}


void
BitcoinNode::SendMessage(enum Messages receivedMessage,  enum Messages responseMessage, rapidjson::Document &d, Ptr<Socket> outgoingSocket)
{
  NS_LOG_FUNCTION (this);
  
  const uint8_t delimiter[] = "#";

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
				
  d["message"].SetInt(responseMessage);
  d.Accept(writer);
  NS_LOG_INFO ("Node " << GetNode ()->GetId () << " got a " 
               << getMessageName(receivedMessage) << " message" 
               << " and sent a " << getMessageName(responseMessage) 
			   << " message: " << buffer.GetString());

  outgoingSocket->Send (reinterpret_cast<const uint8_t*>(buffer.GetString()), buffer.GetSize(), 0);
  outgoingSocket->Send (delimiter, 1, 0);	

  switch (d["message"].GetInt()) 
  {
    case INV:
    {
      m_nodeStats->invSentBytes += m_countBytes + d["inv"].Size()*m_inventorySizeBytes;;
      break;
    }	  
    case GET_HEADERS:
    {
      m_nodeStats->getHeadersSentBytes += m_getHeadersSizeBytes;
      break;
    }
    case HEADERS:
    {
      m_nodeStats->headersSentBytes += m_countBytes + d["blocks"].Size()*m_headersSizeBytes;
      break;
    }
    case BLOCK:
    {
	  for(int k = 0; k < d["blocks"].Size(); k++)
        m_nodeStats->blockSentBytes += m_blockHeadersSizeBytes + m_countBytes + d["blocks"][k]["size"].GetInt();
      break;
    }
    case GET_DATA:
    {
      m_nodeStats->getDataSentBytes += m_countBytes + d["blocks"].Size()*m_inventorySizeBytes;
      break;
    }
  }  
}

void
BitcoinNode::SendMessage(enum Messages receivedMessage,  enum Messages responseMessage, rapidjson::Document &d, Address &outgoingAddress)
{
  NS_LOG_FUNCTION (this);
  
  const uint8_t delimiter[] = "#";

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
				
  d["message"].SetInt(responseMessage);
  d.Accept(writer);
  NS_LOG_INFO ("Node " << GetNode ()->GetId () << " got a " 
               << getMessageName(receivedMessage) << " message" 
               << " and sent a " << getMessageName(responseMessage) 
			   << " message: " << buffer.GetString());
			
  Ipv4Address outgoingIpv4Address = InetSocketAddress::ConvertFrom(outgoingAddress).GetIpv4 ();
  std::map<Ipv4Address, Ptr<Socket>>::iterator it = m_peersSockets.find(outgoingIpv4Address);
  
  if (it == m_peersSockets.end()) //Create the socket if it doesn't exist
  {
    m_peersSockets[outgoingIpv4Address] = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());  
    m_peersSockets[outgoingIpv4Address]->Connect (InetSocketAddress (outgoingIpv4Address, m_bitcoinPort));
  }
  
  m_peersSockets[outgoingIpv4Address]->Send (reinterpret_cast<const uint8_t*>(buffer.GetString()), buffer.GetSize(), 0);
  m_peersSockets[outgoingIpv4Address]->Send (delimiter, 1, 0);	

  switch (d["message"].GetInt()) 
  {
    case INV:
    {
      m_nodeStats->invSentBytes += m_countBytes + d["inv"].Size()*m_inventorySizeBytes;;
      break;
    }	  
    case GET_HEADERS:
    {
      m_nodeStats->getHeadersSentBytes += m_getHeadersSizeBytes;
      break;
    }
    case HEADERS:
    {
      m_nodeStats->headersSentBytes += m_countBytes + d["blocks"].Size()*m_headersSizeBytes;
      break;
    }
    case BLOCK:
    {
	  for(int k = 0; k < d["blocks"].Size(); k++)
        m_nodeStats->blockSentBytes += m_blockHeadersSizeBytes + m_countBytes + d["blocks"][k]["size"].GetInt();
      break;
    }
    case GET_DATA:
    {
      m_nodeStats->getDataSentBytes += m_countBytes + d["blocks"].Size()*m_inventorySizeBytes;
      break;
    }
  } 
}


void
BitcoinNode::SendMessage(enum Messages receivedMessage,  enum Messages responseMessage, std::string packet, Address &outgoingAddress)
{
  NS_LOG_FUNCTION (this);
  
  const uint8_t delimiter[] = "#";
  rapidjson::Document d;
  
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

  d.Parse(packet.c_str());  
  d["message"].SetInt(responseMessage);
  d.Accept(writer);
  NS_LOG_INFO ("Node " << GetNode ()->GetId () << " got a " 
               << getMessageName(receivedMessage) << " message" 
               << " and sent a " << getMessageName(responseMessage) 
			   << " message: " << buffer.GetString());
			
  Ipv4Address outgoingIpv4Address = InetSocketAddress::ConvertFrom(outgoingAddress).GetIpv4 ();
  std::map<Ipv4Address, Ptr<Socket>>::iterator it = m_peersSockets.find(outgoingIpv4Address);
  
  if (it == m_peersSockets.end()) //Create the socket if it doesn't exist
  {
    m_peersSockets[outgoingIpv4Address] = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());  
    m_peersSockets[outgoingIpv4Address]->Connect (InetSocketAddress (outgoingIpv4Address, m_bitcoinPort));
  }
  
  m_peersSockets[outgoingIpv4Address]->Send (reinterpret_cast<const uint8_t*>(buffer.GetString()), buffer.GetSize(), 0);
  m_peersSockets[outgoingIpv4Address]->Send (delimiter, 1, 0);	

  switch (d["message"].GetInt()) 
  {
    case INV:
    {
      m_nodeStats->invSentBytes += m_countBytes + d["inv"].Size()*m_inventorySizeBytes;;
      break;
    }	  
    case GET_HEADERS:
    {
      m_nodeStats->getHeadersSentBytes += m_getHeadersSizeBytes;
      break;
    }
    case HEADERS:
    {
      m_nodeStats->headersSentBytes += m_countBytes + d["blocks"].Size()*m_headersSizeBytes;
      break;
    }
    case BLOCK:
    {
	  for(int k = 0; k < d["blocks"].Size(); k++)
        m_nodeStats->blockSentBytes += m_blockHeadersSizeBytes + m_countBytes + d["blocks"][k]["size"].GetInt();
      break;
    }
    case GET_DATA:
    {
      m_nodeStats->getDataSentBytes += m_countBytes + d["blocks"].Size()*m_inventorySizeBytes;
      break;
    }
  } 
}


void 
BitcoinNode::PrintQueueInv()
{
  NS_LOG_FUNCTION (this);

  std::cout << "The queueINV is:\n";
  
  for(auto &elem : m_queueInv)
  {
    std::vector<Address>::iterator  block_it;
    std::cout << "  " << elem.first << ":";
	
	for (block_it = elem.second.begin();  block_it < elem.second.end(); block_it++)
    {
      std::cout << " " << InetSocketAddress::ConvertFrom(*block_it).GetIpv4 ();
    }
    std::cout << "\n";
  }
  std::cout << std::endl;
}


void 
BitcoinNode::PrintInvTimeouts()
{
  NS_LOG_FUNCTION (this);

  std::cout << "The m_invTimeouts is:\n";
  
  for(auto &elem : m_invTimeouts)
  {
    std::cout << "  " << elem.first << ":\n";
  }
  std::cout << std::endl;
}


void
BitcoinNode::InvTimeoutExpired(std::string blockHash)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_INFO ("Node " << GetNode ()->GetId () << ": At time "  << Simulator::Now ().GetSeconds ()
                << " the timeout for block " << blockHash << " expired");

  //PrintQueueInv();
  //PrintInvTimeouts();
  
  m_queueInv[blockHash].erase(m_queueInv[blockHash].begin());
  m_invTimeouts.erase(blockHash);
  
  //PrintQueueInv();
  //PrintInvTimeouts();
  
  if (!m_queueInv[blockHash].empty())
  {
    rapidjson::Document   d; 
	EventId               timeout;
    rapidjson::Value      value(INV);
    rapidjson::Value      array(rapidjson::kArrayType);
	
    d.SetObject();

    d.AddMember("message", value, d.GetAllocator());
  
    value.SetString("block");
    d.AddMember("type", value, d.GetAllocator());
	
    value.SetString(blockHash.c_str(), blockHash.size(), d.GetAllocator());
    array.PushBack(value, d.GetAllocator());
    d.AddMember("blocks", array, d.GetAllocator());
	
    SendMessage(INV, GET_HEADERS, d, *(m_queueInv[blockHash].begin()));				
    SendMessage(INV, GET_DATA, d, *(m_queueInv[blockHash].begin()));	
					
    timeout = Simulator::Schedule (m_invTimeoutMinutes, &BitcoinNode::InvTimeoutExpired, this, blockHash);
    m_invTimeouts[blockHash] = timeout;
  }
  
  if (m_queueInv[blockHash].size() == 0)
    m_queueInv.erase(blockHash);
    
  //PrintQueueInv();
  //PrintInvTimeouts();
}


bool 
BitcoinNode::ReceivedButNotValidated (std::string blockHash)
{
  NS_LOG_FUNCTION (this);
  
  if ( std::find(m_receivedNotValidated.begin(), m_receivedNotValidated.end(), blockHash) != m_receivedNotValidated.end() )
    return true;
  else
    return false;
}


bool 
BitcoinNode::RemoveReceivedButNotValidated (std::string blockHash)
{
  NS_LOG_FUNCTION (this);
  
  auto it = std::find(m_receivedNotValidated.begin(), m_receivedNotValidated.end(), blockHash);
  
  if ( it  != m_receivedNotValidated.end())
  {
    m_receivedNotValidated.erase(it);
  }
  else
  {
    NS_LOG_WARN (blockHash << " was not found in m_receivedNotValidated");
  }
}


void 
BitcoinNode::HandlePeerClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}
 
void BitcoinNode::HandlePeerError (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}
 

void 
BitcoinNode::HandleAccept (Ptr<Socket> s, const Address& from)
{
  NS_LOG_FUNCTION (this << s << from);
  s->SetRecvCallback (MakeCallback (&BitcoinNode::HandleRead, this));
}

} // Namespace ns3

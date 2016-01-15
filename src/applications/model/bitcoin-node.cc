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

BitcoinNode::BitcoinNode (void) : m_bitcoinPort (8333), m_secondsPerMin(60)
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

std::list<Ptr<Socket> >
BitcoinNode::GetAcceptedSockets (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socketList;
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
BitcoinNode::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_socketList.clear ();

  // chain up
  Application::DoDispose ();
}


// Application Methods
void 
BitcoinNode::StartApplication ()    // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);
  // Create the socket if not already
  NS_LOG_DEBUG ("Node " << GetNode()->GetId() << ": m_numberOfPeers = " << m_numberOfPeers);
  NS_LOG_DEBUG ("Node " << GetNode()->GetId() << ": m_invTimeoutMinutes = " << m_invTimeoutMinutes.GetMinutes() << "mins");
  NS_LOG_DEBUG ("Node " << GetNode()->GetId() << ": My peers are");
  
  for (auto it = m_peersAddresses.begin(); it != m_peersAddresses.end(); it++)
    NS_LOG_DEBUG("\t" << *it);


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
	
    for (std::vector<Ipv4Address>::const_iterator i = m_peersAddresses.begin(); i != m_peersAddresses.end(); ++i)
    {
      m_peersSockets[*i] = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
	  m_peersSockets[*i]->Connect (InetSocketAddress (*i, m_bitcoinPort));
	}
}

void 
BitcoinNode::StopApplication ()     // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);
  
  for (std::vector<Ipv4Address>::iterator i = m_peersAddresses.begin(); i != m_peersAddresses.end(); ++i) //close the outgoing sockets
  {
	m_peersSockets[*i]->Close ();
  }
  
  while(!m_socketList.empty ()) //these are accepted sockets, close them
    {
      Ptr<Socket> acceptedSocket = m_socketList.front ();
      m_socketList.pop_front ();
      acceptedSocket->Close ();
    }
  if (m_socket) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }

  NS_LOG_WARN("\n\nBITCOIN NODE " << GetNode ()->GetId () << ":");
  NS_LOG_WARN ("Current Top Block is:\n" << *(m_blockchain.GetCurrentTopBlock()));
  NS_LOG_DEBUG ("Current Blockchain is:\n" << m_blockchain);
  m_blockchain.PrintOrphans();
  PrintQueueInv();
  PrintInvTimeouts();
  NS_LOG_WARN("Mean Block Receive Time = " << m_meanBlockReceiveTime << " or " 
               << static_cast<int>(m_meanBlockReceiveTime) / m_secondsPerMin << "min and " 
			   << m_meanBlockReceiveTime - static_cast<int>(m_meanBlockReceiveTime) / m_secondsPerMin * m_secondsPerMin << "s");
  NS_LOG_WARN("Mean Block Propagation Time = " << m_meanBlockPropagationTime << "s");
  NS_LOG_WARN("Mean Block Size = " << m_meanBlockSize << " Bytes");
  NS_LOG_WARN("Total Blocks = " << m_blockchain.GetTotalBlocks());
  NS_LOG_WARN("Stale Blocks = " << m_blockchain.GetNoStaleBlocks() << " (" 
              << 100. * m_blockchain.GetNoStaleBlocks() / m_blockchain.GetTotalBlocks() << "%)");
  NS_LOG_WARN("receivedButNotValidated size = " << m_receivedNotValidated.size());

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
        NS_LOG_DEBUG("Node " << GetNode ()->GetId () << " Total Received Data: " << totalReceivedData);
		  
        while ((pos = totalReceivedData.find(delimiter)) != std::string::npos) 
        {
          parsedPacket = totalReceivedData.substr(0, pos);
          NS_LOG_DEBUG("Node " << GetNode ()->GetId () << " Parsed Packet: " << parsedPacket);
		  
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
		  
          NS_LOG_DEBUG ("At time "  << Simulator::Now ().GetSeconds ()
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
                  NS_LOG_DEBUG("INV: Bitcoin node " << GetNode ()->GetId () 
				  << " has already received the block with height = " 
				  << height << " and minerId = " << minerId);				  
                }
                else
                {
                  NS_LOG_DEBUG("INV: Bitcoin node " << GetNode ()->GetId () 
                  << " does not have the block with height = " 
                  << height << " and minerId = " << minerId);
				  
                  /**
                   * Check if we have already request the block
                   */
				   
                  if (m_queueInv.find(parsedInv) == m_queueInv.end())
                  {
                    NS_LOG_DEBUG("INV: Bitcoin node " << GetNode ()->GetId ()
                                 << " has not requested the block yet");
                    requestBlocks.push_back(parsedInv);
                    timeout = Simulator::Schedule (m_invTimeoutMinutes, &BitcoinNode::InvTimeoutExpired, this, parsedInv);
                    m_invTimeouts[parsedInv] = timeout;
                  }
                  else
                  {
                    NS_LOG_DEBUG("INV: Bitcoin node " << GetNode ()->GetId ()
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
			  
              for (j=0; j<d["blocks"].Size(); j++)
              {  
                std::string   invDelimiter = "/";
                std::string   parsedInv = d["blocks"][j].GetString();
                size_t        invPos = parsedInv.find(invDelimiter);
				  
                int height = atoi(parsedInv.substr(0, invPos).c_str());
                int minerId = atoi(parsedInv.substr(invPos+1, parsedInv.size()).c_str());
				
                if (m_blockchain.HasBlock(height, minerId) || m_blockchain.IsOrphan(height, minerId))
                {
                  NS_LOG_DEBUG("GET_HEADERS: Bitcoin node " << GetNode ()->GetId () 
                  << " has the block with height = " 
                  << height << " and minerId = " << minerId);
                  Block newBlock (m_blockchain.ReturnBlock (height, minerId));
                  requestBlocks.push_back(newBlock);
                }
                else
                {
                  NS_LOG_DEBUG("GET_HEADERS: Bitcoin node " << GetNode ()->GetId () 
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
                  NS_LOG_DEBUG ("In requestBlocks " << *block_it);
    
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
              int j;
              std::vector<Block>              requestBlocks;
              std::vector<Block>::iterator    block_it;
			  
              for (j=0; j<d["blocks"].Size(); j++)
              {  
                std::string    invDelimiter = "/";
                std::string    parsedInv = d["blocks"][j].GetString();
                size_t         invPos = parsedInv.find(invDelimiter);
				  
                int height = atoi(parsedInv.substr(0, invPos).c_str());
                int minerId = atoi(parsedInv.substr(invPos+1, parsedInv.size()).c_str());
				
                if (m_blockchain.HasBlock(height, minerId) || ReceivedButNotValidated(parsedInv))
                {
                  NS_LOG_DEBUG("GET_DATA: Bitcoin node " << GetNode ()->GetId () 
                  << " has already received the block with height = " 
                  << height << " and minerId = " << minerId);
                  Block newBlock (m_blockchain.ReturnBlock (height, minerId));
                  requestBlocks.push_back(newBlock);
                }
                else
                {
                  NS_LOG_DEBUG("GET_DATA: Bitcoin node " << GetNode ()->GetId () 
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
                  NS_LOG_DEBUG ("In requestBlocks " << *block_it);
    
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

                SendMessage(GET_DATA, BLOCK, d, from);	
              }
              break;
            }
            case HEADERS:
            {
              NS_LOG_DEBUG ("HEADERS");

              std::vector<std::string>              requestBlocks;
              std::vector<std::string>::iterator    block_it;
              int j;
			  
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
                  NS_LOG_DEBUG("The Block with height = " << d["blocks"][j]["height"].GetInt() 
                               << " and minerId = " << d["blocks"][j]["minerId"].GetInt() 
                               << " is an orphan\n");
				  
                  /**
                   * Acquire parent
                   */
	  
                  if (m_queueInv.find(blockHash) == m_queueInv.end())
                  {
                    NS_LOG_DEBUG("INV: Bitcoin node " << GetNode ()->GetId ()
                                 << " has not requested its parent block yet");
                    requestBlocks.push_back(blockHash.c_str());
                    timeout = Simulator::Schedule (m_invTimeoutMinutes, &BitcoinNode::InvTimeoutExpired, this, blockHash);
                    m_invTimeouts[blockHash] = timeout;
                  }
                  else
                  {
                    NS_LOG_DEBUG("INV: Bitcoin node " << GetNode ()->GetId ()
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
				  NS_LOG_DEBUG("The Block with height = " << d["blocks"][j]["height"].GetInt() 
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
              NS_LOG_DEBUG ("BLOCK");
              int j;
              double fullBlockReceiveTime = 0;
			  
              for (j=0; j<d["blocks"].Size(); j++)
              {  
                fullBlockReceiveTime = d["blocks"][j]["size"].GetInt() / static_cast<double>(1000000) + fullBlockReceiveTime; //FIX ME: constant MB/s
                Block newBlock (d["blocks"][j]["height"].GetInt(), d["blocks"][j]["minerId"].GetInt(), d["blocks"][j]["parentBlockMinerId"].GetInt(), 
                                d["blocks"][j]["size"].GetInt(), d["blocks"][j]["timeCreated"].GetDouble(), 
                                Simulator::Now ().GetSeconds () + fullBlockReceiveTime, InetSocketAddress::ConvertFrom(from).GetIpv4 ());

                Simulator::Schedule (Seconds(fullBlockReceiveTime), &BitcoinNode::ReceiveBlock, this, newBlock);
                NS_LOG_DEBUG("The full block " << newBlock << " will be received in " << fullBlockReceiveTime << "s");
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
  NS_LOG_DEBUG ("ReceiveBlock: At time " << Simulator::Now ().GetSeconds ()
                << "s bitcoin node " << GetNode ()->GetId () << " received " << newBlock);
				
  if (m_blockchain.HasBlock(newBlock))
  {
    NS_LOG_DEBUG("ReceiveBlock: Bitcoin node " << GetNode ()->GetId () << " has already added this block in the m_blockchain: " << newBlock);
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
BitcoinNode::ReceivedHigherBlock(const Block &newBlock) 
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG("ReceivedHigherBlock: Bitcoin node " << GetNode ()->GetId () << " added a new block in the m_blockchain with higher height: " << newBlock);
}


void 
BitcoinNode::ValidateBlock(const Block &newBlock) 
{
  NS_LOG_FUNCTION (this);
  
  const Block *parent = m_blockchain.GetParent(newBlock);
  
  if (parent == nullptr)
  {
    NS_LOG_DEBUG("ValidateBlock: Block " << newBlock << " is an orphan\n"); 
	 
	 m_blockchain.AddOrphan(newBlock);
	 //m_blockchain.PrintOrphans();
  }
  else 
  {
    NS_LOG_DEBUG("ValidateBlock: Block's " << newBlock << " parent is " << *parent << "\n");

	/**
	 * Block is not orphan, so we can go on validating
	 */	
	 
	const int averageBlockSizeBytes = 458263;
	const double averageValidationTimeSeconds = 0.174;
	double validationTime = averageValidationTimeSeconds * newBlock.GetBlockSizeBytes() / averageBlockSizeBytes;		
	
    Simulator::Schedule (Seconds(validationTime), &BitcoinNode::AfterBlockValidation, this, newBlock);
    NS_LOG_DEBUG ("ValidateBlock: The Block " << newBlock << " will be validated in " 
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
  
  NS_LOG_DEBUG ("AfterBlockValidation: At time " << Simulator::Now ().GetSeconds ()
               << "s bitcoin node " << GetNode ()->GetId () 
			   << " validated block " <<  newBlock);
			   
  if (newBlock.GetBlockHeight() > m_blockchain.GetBlockchainHeight())
    ReceivedHigherBlock(newBlock);

  if (m_blockchain.IsOrphan(newBlock))
  {
    NS_LOG_DEBUG ("AfterBlockValidation: Block " << newBlock << " was orphan");
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
    NS_LOG_DEBUG("ValidateOrphanChildren: Block " << newBlock << " has no orphan children\n");
  }
  else 
  {
    std::vector<const Block *>::iterator  block_it;
	NS_LOG_DEBUG("ValidateOrphanChildren: Block " << newBlock << " has orphan children:");
	
	for (block_it = children.begin();  block_it < children.end(); block_it++)
    {
       NS_LOG_DEBUG ("\t" << **block_it);
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
	  

      NS_LOG_DEBUG ("AdvertiseNewBlock: At time " << Simulator::Now ().GetSeconds ()
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
  NS_LOG_DEBUG ("Node " << GetNode ()->GetId () << " got a " 
               << getMessageName(receivedMessage) << " message" 
               << " and sent a " << getMessageName(responseMessage) 
			   << " message: " << buffer.GetString());

  outgoingSocket->Send (reinterpret_cast<const uint8_t*>(buffer.GetString()), buffer.GetSize(), 0);
  outgoingSocket->Send (delimiter, 1, 0);		
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
  NS_LOG_DEBUG ("Node " << GetNode ()->GetId () << " got a " 
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

  NS_LOG_DEBUG ("Node " << GetNode ()->GetId () << ": At time "  << Simulator::Now ().GetSeconds ()
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
  m_socketList.push_back (s);
}

} // Namespace ns3

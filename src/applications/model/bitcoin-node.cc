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
    .AddAttribute ("NumberOfPeers", 
				   "The number of peers for the node",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BitcoinNode::m_numberOfPeers),
                   MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("Rx",
                     "A packet has been received",
                     MakeTraceSourceAccessor (&BitcoinNode::m_rxTrace),
                     "ns3::Packet::AddressTracedCallback")
  ;
  return tid;
}

BitcoinNode::BitcoinNode (void) 
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_meanBlockReceiveTime = 0;
  m_previousBlockReceiveTime = 0;
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

}

void 
BitcoinNode::StopApplication ()     // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);
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

  NS_LOG_DEBUG("\n\nBITCOIN NODE " << GetNode ()->GetId () << ":");
  NS_LOG_DEBUG ("Current Top Block is:\n" << *(blockchain.GetCurrentTopBlock()));
  NS_LOG_DEBUG ("Current Blockchain is:\n" << blockchain);
  NS_LOG_DEBUG("Mean Block Receive Time = " << m_meanBlockReceiveTime);
  NS_LOG_DEBUG("Total Blocks = " << blockchain.GetTotalBlocks());
  NS_LOG_DEBUG("Stale Blocks = " << blockchain.GetNoStaleBlocks());
}

void 
BitcoinNode::SendPacket (void)
{
  NS_LOG_FUNCTION (this);
  
  Ptr<Packet> packet = Create<Packet> (1000);//Change this later
  
  for (std::vector<Address>::const_iterator i = m_peersAddresses.begin(); i != m_peersAddresses.end(); ++i)
  {
    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
    ns3TcpSocket->Connect(*i);
	ns3TcpSocket->Send (packet);
	ns3TcpSocket->Close();
  }
 
  NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
			   << "s bitcoin node " << GetNode ()->GetId () << " sent  a packet");
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

		  char *packetInfo = new char[packet->GetSize () + 1];
		  packet->CopyData (reinterpret_cast<uint8_t*>(packetInfo), packet->GetSize ());
		  packetInfo[packet->GetSize ()] = '\0'; // ensure that it is null terminated to avoid bugs
		  
		  rapidjson::Document d;
          d.Parse(packetInfo);
		  
		  rapidjson::StringBuffer buffer;
		  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
          d.Accept(writer);
		  
		  NS_LOG_DEBUG ("packet size: " << packet->GetSize () << ", packet info: " << buffer.GetString());
		  
          Block newBlock (d["height"].GetInt(), d["minerId"].GetInt(), d["parentBlockMinerId"].GetInt(),
						  d["size"].GetInt(), d["timeCreated"].GetDouble(), newBlockReceiveTime);	
						  
          NS_LOG_DEBUG ("At time "  << Simulator::Now ().GetSeconds ()
                       << "s bitcoin node " << GetNode ()->GetId () << " received "
                       <<  packet->GetSize () << " bytes from "
                       << InetSocketAddress::ConvertFrom(from).GetIpv4 ()
                       << " port " << InetSocketAddress::ConvertFrom (from).GetPort () 
					   << " with size = " << d["size"].GetInt() << " from miner " << d["minerId"].GetInt());
		  	  
		  if (blockchain.HasBlock(newBlock))
		  {
		    NS_LOG_DEBUG("Bitcoin node " << GetNode ()->GetId () << " has already added this block in the blockchain: " << newBlock);
		  }
		  else
		  {
			/**
			 * Update m_meanBlockReceiveTime with the timeReceived of the newly received block
			 */
		    m_meanBlockReceiveTime = (blockchain.GetTotalBlocks() - 1)/static_cast<double>(blockchain.GetTotalBlocks())*m_meanBlockReceiveTime + 
									 (newBlockReceiveTime - m_previousBlockReceiveTime)/(blockchain.GetTotalBlocks());
			m_previousBlockReceiveTime = newBlockReceiveTime;
			blockchain.AddBlock(newBlock);
		    NS_LOG_DEBUG("Bitcoin node " << GetNode ()->GetId () << " added a new block in the blockchain: " << newBlock);
		  }
		  
		  rapidjson::Value value(newBlockReceiveTime);
		  double fullBlockReceiveTime = d["size"].GetInt() / static_cast<double>(1000000) ; //FIX: constant MB/s
		  d.AddMember("timeReceived", value, d.GetAllocator());

		  Simulator::Schedule (Seconds(fullBlockReceiveTime), &BitcoinNode::ReceivePacket, this, *(blockchain.GetBlockPointer(newBlock)));
		  NS_LOG_DEBUG("The full block will be received in " << fullBlockReceiveTime << "s");
		  
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
BitcoinNode::ReceivePacket(Block newBlock)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_DEBUG ("ReceivePacket: At time " << Simulator::Now ().GetSeconds ()
                << "s bitcoin node " << GetNode ()->GetId () << " received " << newBlock);
}

void 
BitcoinNode::SetPeersAddresses (std::vector<Address> peers)
{
  NS_LOG_FUNCTION (this);
  m_peersAddresses = peers;
}
  
std::vector<Address> 
BitcoinNode::GetPeersAddresses (void) const
{
  NS_LOG_FUNCTION (this);
  return m_peersAddresses;
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

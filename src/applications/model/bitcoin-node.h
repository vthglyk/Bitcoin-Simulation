#ifndef BITCOIN_NODE_H
#define BITCOIN_NODE_H

#include <algorithm>
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/address.h"
#include "bitcoin.h"
#include "../../rapidjson/document.h"
#include "../../rapidjson/writer.h"
#include "../../rapidjson/stringbuffer.h"

namespace ns3 {

class Address;
class Socket;
class Packet;

/**
 * Based on packet-sink.h
 */
 
class BitcoinNode : public Application 
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  BitcoinNode (void);

  virtual ~BitcoinNode (void);

  /**
   * \return pointer to listening socket
   */
  Ptr<Socket> GetListeningSocket (void) const;

  /**
   * \return list of pointers to accepted sockets
   */
  std::list<Ptr<Socket> > GetAcceptedSockets (void) const;

  /**
   * \return a vector containing the addresses of peers
   */  
  std::vector<Ipv4Address> GetPeersAddresses (void) const;
  
  
  /**
   * set the address of peers
   */
  void SetPeersAddresses (const std::vector<Ipv4Address> &peers);
  
  /**
   * set the node statistics
   */
  void SetNodeStats (nodeStatistics *nodeStats);
  
protected:
  virtual void DoDispose (void);

  // inherited from Application base class.
  virtual void StartApplication (void);    // Called at time specified by Start
  virtual void StopApplication (void);     // Called at time specified by Stop

  /**
   * \brief Handle a packet received by the application
   * \param socket the receiving socket
   */
  void HandleRead (Ptr<Socket> socket);
  /**
   * \brief Handle an incoming connection
   * \param socket the incoming connection socket
   * \param from the address the connection is from
   */
  void HandleAccept (Ptr<Socket> socket, const Address& from);
  /**
   * \brief Handle an connection close
   * \param socket the connected socket
   */
  void HandlePeerClose (Ptr<Socket> socket);
  /**
   * \brief Handle an connection error
   * \param socket the connected socket
   */
  void HandlePeerError (Ptr<Socket> socket);

  void ReceiveBlock(const Block &newBlock);				    //Called for every new block
  virtual void ReceivedHigherBlock(const Block &newBlock);	//Called for blocks with better score(height)
  
  void ValidateBlock(const Block &newBlock);
  void AfterBlockValidation(const Block &newBlock);
  void ValidateOrphanChildren(const Block &newBlock);
  
  void AdvertiseNewBlock (const Block &newBlock);

  void SendMessage(enum Messages receivedMessage,  enum Messages responseMessage, rapidjson::Document &d, Ptr<Socket> outgoingSocket);
  void SendMessage(enum Messages receivedMessage,  enum Messages responseMessage, rapidjson::Document &d, Address &outgoingAddress);
  
  void PrintQueueInv();
  void PrintInvTimeouts();

  void InvTimeoutExpired (std::string blockHash);

  bool ReceivedButNotValidated (std::string blockHash);
  
  bool RemoveReceivedButNotValidated (std::string blockHash);
  
  
  // In the case of TCP, each socket accept returns a new socket, so the 
  // listening socket is stored separately from the accepted sockets
  Ptr<Socket>     m_socket;       //!< Listening socket
  Address         m_local;        //!< Local address to bind to
  TypeId          m_tid;          //!< Protocol TypeId
  int			  m_numberOfPeers; //!< Number of node's peers
  double		  m_meanBlockReceiveTime;
  double		  m_previousBlockReceiveTime;
  double		  m_meanBlockPropagationTime;
  double		  m_meanBlockSize;
  Blockchain 	  m_blockchain;
  Time            m_invTimeoutMinutes;
  
  std::list<Ptr<Socket> >                         m_socketList;            //!< the accepted sockets
  std::vector<Ipv4Address>		                  m_peersAddresses;        //!< The addresses of peers
  std::map<Ipv4Address, Ptr<Socket>>              m_peersSockets;          //!< The sockets of peers
  std::map<std::string, std::vector<Address>>     m_queueInv;              //!< map holding the addresses of nodes which sent an INV for a particular block
  std::map<std::string, EventId>                  m_invTimeouts;           //!< map holding the event timeouts of inv messages
  std::map<Address, std::string>                  m_bufferedData;          //!< map holding the buffered data from previous handleRead events
  std::vector<std::string>                        m_receivedNotValidated;  //!< map holding the received but not yet validated blocks
  nodeStatistics                                  *m_nodeStats;             //!< struct holding the node stats
  
  const int		  m_bitcoinPort;   //!< 8333
  const int       m_secondsPerMin; //!< 8333
  /// Traced Callback: received packets, source address.
  TracedCallback<Ptr<const Packet>, const Address &> m_rxTrace;

};

} // namespace ns3

#endif /* BITCOIN_NODE_H */


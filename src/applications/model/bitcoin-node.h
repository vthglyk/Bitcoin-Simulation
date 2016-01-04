#ifndef BITCOIN_NODE_H
#define BITCOIN_NODE_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/address.h"
#include "bitcoin.h"

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
   * set the address of peers
   */
  void SetPeersAddresses (std::vector<Address> peers);
  
  /**
   * \return a vector containing the addresses of peers
   */  
  std::vector<Address> GetPeersAddresses (void) const;

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

  virtual void SendPacket (void);

  // In the case of TCP, each socket accept returns a new socket, so the 
  // listening socket is stored separately from the accepted sockets
  Ptr<Socket>     m_socket;       //!< Listening socket
  std::list<Ptr<Socket> > m_socketList; //!< the accepted sockets

  Address         m_local;        //!< Local address to bind to
  TypeId          m_tid;          //!< Protocol TypeId
  int			  m_numberOfPeers; //!< Number of node's peers
  double		  m_meanBlockReceiveTime;
  double		  m_previousBlockReceiveTime;
  std::vector<Address>		  m_peersAddresses; //!< The addresses of peers
  Blockchain blockchain;
  
  /// Traced Callback: received packets, source address.
  TracedCallback<Ptr<const Packet>, const Address &> m_rxTrace;

};

} // namespace ns3

#endif /* BITCOIN_NODE_H */


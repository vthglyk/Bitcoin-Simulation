#ifndef BITCOIN_NODE_H
#define BITCOIN_NODE_H

#include <algorithm>
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/address.h"
#include "bitcoin.h"
#include "ns3/boolean.h"
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
   * \return a vector containing the addresses of peers
   */  
  std::vector<Ipv4Address> GetPeersAddresses (void) const;
  
  
  /**
   * set the address of peers
   */
  void SetPeersAddresses (const std::vector<Ipv4Address> &peers);
  
  /**
   * set the peersDownloadSpeeds of peers
   */
  void SetPeersDownloadSpeeds (const std::map<Ipv4Address, double> &peersDownloadSpeeds);
  
  /**
   * set the internet speeds of the node
   */
  void SetNodeInternetSpeeds (const nodeInternetSpeeds &internetSpeeds);
  
  /**
   * set the node statistics
   */
  void SetNodeStats (nodeStatistics *nodeStats);
  
  /**
   * set the protocol
   */
  void SetProtocolType (enum ProtocolType protocolType);

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

  void ReceivedBlockMessage(std::string &blockInfo, Address &from);			//Called for every BLOCK message
  void ReceivedChunkMessage(std::string &chunkInfo, Address &from);			//Called for every BLOCK message
  void ReceiveBlock(const Block &newBlock);				                    //Called for every new block
  void ReceivedLastChunk(const Block &newBlock);				            //Called when we receive the last chunk of the block

  void SendBlock(std::string packetInfo, Address &from);				   
  void SendChunk(std::string packetInfo, Address &from);				   

  virtual void ReceivedHigherBlock(const Block &newBlock);	//Called for blocks with better score(height)

  void ValidateBlock(const Block &newBlock);
  void AfterBlockValidation(const Block &newBlock);
  void ValidateOrphanChildren(const Block &newBlock);
  
  void AdvertiseNewBlock (const Block &newBlock);
  void AdvertiseFullBlock (const Block &newBlock);
  void AdvertiseFirstChunk (const Block &newBlock);

  void SendMessage(enum Messages receivedMessage,  enum Messages responseMessage, rapidjson::Document &d, Ptr<Socket> outgoingSocket);
  void SendMessage(enum Messages receivedMessage,  enum Messages responseMessage, rapidjson::Document &d, Address &outgoingAddress);
  void SendMessage(enum Messages receivedMessage,  enum Messages responseMessage, std::string packet, Address &outgoingAddress);

  void PrintQueueInv();
  void PrintInvTimeouts();
  void PrintChunkTimeouts();
  void PrintQueueChunks();
  void PrintQueueChunkPeers();
  void PrintReceivedChunks();
  void PrintOnlyHeadersReceived();
  
  void InvTimeoutExpired (std::string blockHash);
  void ChunkTimeoutExpired (std::string chunk);

  bool ReceivedButNotValidated (std::string blockHash);
  void RemoveReceivedButNotValidated (std::string blockHash);

  bool OnlyHeadersReceived (std::string blockHash);
  bool HasChunk (std::string blockHash, int chunk);
  
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
  bool            m_isMiner;
  double          m_downloadSpeed;
  double          m_uploadSpeed;
  double 		  m_averageTransactionSize;
  int             m_transactionIndexSize;         //!< The transaction index size in bytes.
  bool            m_blockTorrent;
  uint32_t        m_chunkSize;
  
  std::vector<Ipv4Address>		                      m_peersAddresses;                 //!< The addresses of peers
  std::map<Ipv4Address, double>                       m_peersDownloadSpeeds;            //!< The peersDownloadSpeeds of channels
  std::map<Ipv4Address, Ptr<Socket>>                  m_peersSockets;                   //!< The sockets of peers
  std::map<std::string, std::vector<Address>>         m_queueInv;                       //!< map holding the addresses of nodes which sent an INV for a particular block
  std::map<std::string, std::vector<Address>>         m_queueChunkPeers;                //!< map holding the addresses of nodes from which we are waiting for a CHUNK, key = block_hash
  std::map<std::string, std::vector<int>>             m_queueChunks;                    //!< map holding the chunks of the blocks which we have not requested yet, key = block_hash
  std::map<std::string, std::vector<int>>             m_receivedChunks;                 //!< map holding the chunks of the blocks which we are currently downloading, key = block_hash
  std::map<std::string, EventId>                      m_invTimeouts;                    //!< map holding the event timeouts of inv messages
  std::map<std::string, EventId>                      m_chunkTimeouts;                  //!< map holding the event timeouts of chunk messages
  std::map<Address, std::string>                      m_bufferedData;                   //!< map holding the buffered data from previous handleRead events
  std::map<std::string, Block>                        m_receivedNotValidated;           //!< vector holding the received but not yet validated blocks
  std::map<std::string, Block>                        m_onlyHeadersReceived;            //!< vector holding the blocks that we know but not received
  nodeStatistics                                     *m_nodeStats;                      //!< struct holding the node stats
  std::vector<double>                                 m_sendBlockTimes;                 //!< contains the times of the next sendBlock events
  std::vector<double>                                 m_receiveBlockTimes;              //!< contains the times of the next sendBlock events
  enum ProtocolType                                   m_protocolType;                   //!< protocol type

  const int		  m_bitcoinPort;   //!< 8333
  const int       m_secondsPerMin; //!< 8333
  const int       m_countBytes;    //!< The size of count variable in messages, 4 Bytes
  const int       m_bitcoinMessageHeader; //!< The size of the bitcoin Message Header
  const int       m_inventorySizeBytes; //!< The size of inventories in INV messages, 36 Bytes
  const int       m_getHeadersSizeBytes; //!< The size of the GET_HEADERS message, 72 Bytes
  const int       m_headersSizeBytes; //!< 80 Bytes
  const int       m_blockHeadersSizeBytes; //!< 80 Bytes
  
  /// Traced Callback: received packets, source address.
  TracedCallback<Ptr<const Packet>, const Address &> m_rxTrace;
  
};

} // namespace ns3

#endif /* BITCOIN_NODE_H */


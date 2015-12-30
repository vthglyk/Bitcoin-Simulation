#ifndef BITCOIN_MINER_H
#define BITCOIN_MINER_H

#include "bitcoin-node.h"
#include <random>

namespace ns3 {

class Address;
class Socket;
class Packet;

/**
 * \ingroup applications 
 * \defgroup packetsink PacketSink
 *
 * This application was written to complement OnOffApplication, but it
 * is more general so a PacketSink name was selected.  Functionally it is
 * important to use in multicast situations, so that reception of the layer-2
 * multicast frames of interest are enabled, but it is also useful for
 * unicast as an example of how you can write something simple to receive
 * packets at the application layer.  Also, if an IP stack generates 
 * ICMP Port Unreachable errors, receiving applications will be needed.
 */

/**
 * \ingroup packetsink
 *
 * \brief Receive and consume traffic generated to an IP address and port
 *
 * This application was written to complement OnOffApplication, but it
 * is more general so a PacketSink name was selected.  Functionally it is
 * important to use in multicast situations, so that reception of the layer-2
 * multicast frames of interest are enabled, but it is also useful for
 * unicast as an example of how you can write something simple to receive
 * packets at the application layer.  Also, if an IP stack generates 
 * ICMP Port Unreachable errors, receiving applications will be needed.
 *
 * The constructor specifies the Address (IP address and port) and the 
 * transport protocol to use.   A virtual Receive () method is installed 
 * as a callback on the receiving socket.  By default, when logging is
 * enabled, it prints out the size of packets and their address.
 * A tracing source to Receive() is also available.
 */
class BitcoinMiner : public BitcoinNode 
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  BitcoinMiner ();
  
  /**
   * \return fixed Block Time Generation
   */
  double GetFixedBlockTimeGeneration (void) const;

  /**
   * Set fixed Block Time Generation
   */
  void SetFixedBlockTimeGeneration (double fixedBlockTimeGeneration);

  /**
   * \return fixed Block Size
   */
  uint32_t GetFixedBlockSize(void) const;

  /**
   * Set fixed Block Size
   */
  void SetFixedBlockSize (uint32_t fixedBlockSize);
 
  /**
   * \return fixed Block Generation binSize
   */
  double GetBlockGenBinSize(void) const;

  /**
   * Set fixed Block Generation binSize
   */
  void SetBlockGenBinSize (double m_blockGenBinSize);
 
  /**
   * \return fixed Block Generation binSize
   */
  double GetBlockGenParameter(void) const;

  /**
   * Set fixed Block Generation binSize
   */
  void SetBlockGenParameter (double blockGenParameter); 

  /**
   * \return fixed hash rate
   */
  double GetHashRate(void) const;

  /**
   * Set fixed hash rate
   */
  void SetHashRate (double blockGenParameter);  
  
protected:
  // inherited from Application base class.
  virtual void StartApplication (void);    // Called at time specified by Start
  virtual void StopApplication (void);    // Called at time specified by Stop

  /**
   * Schedule next Mining Event
   */  
  void ScheduleNextMiningEvent (void);
  
  virtual void SendPacket (void);
  
  uint32_t			m_fixedBlockSize;  
  double 				m_fixedBlockTimeGeneration; 	//!< Fixed Block Time Generation
  EventId			m_nextMiningEvent; 				//!< Event to mine the next block
  std::default_random_engine m_generator;

  /** 
   * The m_blockGenBinSize states binSize of the block generation time.
   * In the paper "Misbehaviour In Bitcoin" is stated when the binSize is 2mins the parameter is 0.19.
   * According to that we calculate the the new parameter as 0.183*m_blockGenBinSize/2 to achieve better granularity
   */   
  double 			m_blockGenBinSize;	
  double			m_blockGenParameter; 			//!< The block generation distribution parameter
  double 			m_nextBlockTime;
  float				m_minerAverageBlockGenTime;
  int				m_minerGeneratedBlocks;
  double			m_hashRate;
  std::geometric_distribution<int> m_blockSizeDistribution;
  
};

} // namespace ns3

#endif /* BITCOIN_MINER_H */


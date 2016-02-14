#ifndef BITCOIN_H
#define BITCOIN_H

#include <vector>
#include <map>
#include "ns3/address.h"
#include <algorithm>

namespace ns3 {
	
enum Messages
{
  INV,				//0
  GET_HEADERS,		//1
  HEADERS,			//2
  GET_BLOCKS,		//3
  BLOCK,			//4
  GET_DATA,			//5
  NO_MESSAGE        //6
};

enum MinerType
{
  NORMAL_MINER,
  SIMPLE_ATTACKER,
  SELFISH_MINER
};

enum BlockBroadcastType
{
  STANDARD,
  UNSOLICITED,
  RELAY_NETWORK
};

enum BitcoinRegion
{
  NORTH_AMERICA,    //0
  EUROPE,           //1
  SOUTH_AMERICA,    //2
  ASIA_PACIFIC,     //3
  JAPAN,            //4
  AUSTRALIA,        //5
  OTHER             //6
};

typedef struct {
  int      nodeId;
  double   meanBlockReceiveTime;
  double   meanBlockPropagationTime;
  double   meanBlockSize;
  int      totalBlocks;
  int      staleBlocks;
  int      miner;	//0->node, 1->miner
  int      minerGeneratedBlocks;
  double   minerAverageBlockGenInterval;
  double   minerAverageBlockSize;
  double   hashRate;
  int      attackSuccess; //0->fail, 1->success
  long     invReceivedBytes;
  long     invSentBytes;
  long     getHeadersReceivedBytes;
  long     getHeadersSentBytes;
  long     headersReceivedBytes;
  long     headersSentBytes;
  long     getDataReceivedBytes;
  long     getDataSentBytes;
  long     blockReceivedBytes;
  long     blockSentBytes;
  int      longestFork;
  int      blocksInForks;
  int      connections;
} nodeStatistics;


typedef struct {
  double downloadSpeed;
  double uploadSpeed;
} nodeInternetSpeeds;


const char* getMessageName(enum Messages m);
const char* getMinerType(enum MinerType m);
const char* getBlockBroadcastType(enum BlockBroadcastType m);
const char* getBitcoinRegion(enum BitcoinRegion m);
enum BitcoinRegion getBitcoinEnum(uint32_t n);

class Block
{
public:
  Block (int blockHeight, int minerId, int parentBlockMinerId = 0, int blockSizeBytes = 0, 
         double timeCreated = 0, double timeReceived = 0, Ipv4Address receivedFromIpv4 = Ipv4Address("0.0.0.0"));
  Block (const Block &blockSource);  // Copy constructor
  virtual ~Block (void);
 
  int GetBlockHeight (void) const;
  void SetBlockHeight (int blockHeight);
  
  int GetMinerId (void) const;
  void SetMinerId (int minerId);
  
  int GetParentBlockMinerId (void) const;
  void SetParentBlockMinerId (int parentBlockMinerId);
  
  int GetBlockSizeBytes (void) const;
  void SetBlockSizeBytes (int blockSizeBytes);
  
  double GetTimeCreated (void) const;
  
  double GetTimeReceived (void) const;

  Ipv4Address GetReceivedFromIpv4 (void) const;
  void SetReceivedFromIpv4 (Ipv4Address receivedFromIpv4);
    
  bool IsParent (const Block &block) const; //check if it is the parent of block

  bool IsChild (const Block &block) const; //check if it is the child of block
  
  Block& operator= (const Block &blockSource);
  
  friend bool operator== (const Block &block1, const Block &block2);
  friend std::ostream& operator<< (std::ostream &out, const Block &block);
  
private:	
  int           m_blockHeight;
  int           m_minerId;
  int           m_parentBlockMinerId;
  int           m_blockSizeBytes;
  double        m_timeCreated;
  double        m_timeReceived;
  Ipv4Address   m_receivedFromIpv4;
  
};

class Blockchain
{
public:
  Blockchain(void);
  virtual ~Blockchain (void);

  int GetNoStaleBlocks (void) const;
  
  int GetNoOrphans (void) const;
  
  int GetTotalBlocks (void) const;
  
  int GetBlockchainHeight (void) const;

  bool HasBlock (const Block &newBlock) const;
  bool HasBlock (int height, int minerId) const;
  /**
   * Return the block with the specified height and minerId.
   * Should be called after HasBlock() to make sure that the block exists
   */
  Block ReturnBlock(int height, int minerId);  

  bool IsOrphan (const Block &newBlock) const;
  bool IsOrphan (int height, int minerId) const;
  
  const Block* GetBlockPointer (const Block &newBlock) const;
  
  const std::vector<const Block *> GetChildrenPointers (const Block &newBlock);  //Get the children of newBlock
  const std::vector<const Block *> GetOrphanChildrenPointers (const Block &newBlock);  //Get the orphan children of newBlock
  
  const Block* GetParent (const Block &newBlock);  //Get the parent of newBlock

  const Block* GetCurrentTopBlock (void) const;
  
  void AddBlock (const Block& newBlock);
  
  void AddOrphan (const Block& newBlock);
  void RemoveOrphan (const Block& newBlock);
  void PrintOrphans (void);
  
  int GetBlocksInForks (void);
  
  int GetLongestForkSize (void);

  friend std::ostream& operator<< (std::ostream &out, Blockchain &blockchain);

private:
  int                                m_noStaleBlocks;    //total number of stale blocks
  int                                m_totalBlocks;		 //total number of blocks including genesis block
  std::vector<std::vector<Block>>    m_blocks;	         //2d vector containing all the blocks of the blockchain. (row->blockHeight, col->sibling blocks)
  std::vector<Block>                 m_orphans;			 //vector containing the orphans


};


}// Namespace ns3

#endif /* BITCOIN_H */

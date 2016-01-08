#ifndef BITCOIN_H
#define BITCOIN_H

#include <vector>
#include "ns3/address.h"

namespace ns3 {
	
enum Messages
{
  INV,				//0
  GET_HEADERS,		//1
  HEADERS,			//2
  GET_BLOCKS,		//3
  BLOCK,			//4
  GET_DATA			//5
};

const char* getMessageName(enum Messages m);

class Block
{
public:
  Block (int blockHeight, int minerId, int parentBlockMinerId, int blockSizeBytes, 
         double timeCreated, double timeReceived, Ipv4Address receivedFromIpv4);
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
  
  bool IsParent(const Block &block) const; //check if it is the parent of block

  bool IsChild(const Block &block) const; //check if it is the child of block

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
  
  bool IsOrphan (const Block &newBlock) const;
  
  const Block* GetBlockPointer (const Block &newBlock) const;
  
  const std::vector<const Block *> GetChildrenPointers (const Block &newBlock);  //Get the children of newBlock
  const std::vector<const Block *> GetOrphanChildrenPointers (const Block &newBlock);  //Get the orphan children of newBlock
  
  const Block* GetParent (const Block &newBlock);  //Get the parent of newBlock

  const Block* GetCurrentTopBlock (void) const;
  
  void AddBlock (const Block& newBlock);
  
  void AddOrphan (const Block& newBlock);
  void RemoveOrphan (const Block& newBlock);
  void PrintOrphans (void);
  
  friend std::ostream& operator<< (std::ostream &out, Blockchain &blockchain);

private:
  int                                m_noStaleBlocks;    //total number of stale blocks
  int                                m_totalBlocks;		 //total number of blocks including genesis block
  std::vector<std::vector<Block>>    m_blocks;	         //2d vector containing all the blocks of the blockchain. (row->blockHeight, col->sibling blocks)
  std::vector<Block>                 m_orphans;			 //vector containing the orphans


};


}// Namespace ns3

#endif /* BITCOIN_H */

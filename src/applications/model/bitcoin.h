#ifndef BITCOIN_H
#define BITCOIN_H

#include <vector>

namespace ns3 {
	
enum Messages
{
  INV,
  GETHEADERS,
  GETBLOCK
};

class Block
{
public:
  Block (int blockHeight, int minerId, int parentBlockMinerId, int blockSizeBytes, int timeCreated, int timeReceived);
  virtual ~Block (void);
 
  int GetBlockHeight (void) const;
  void SetBlockHeight (int blockHeight);
  
  int GetMinerId (void) const;
  void SetMinerId (int minerId);
  
  int GetParentBlockMinerId (void) const;
  void SetParentBlockMinerId (int parentBlockMinerId);
  
  int GetBlockSizeBytes (void) const;
  void SetBlockSizeBytes (int blockSizeBytes);
  
  int GetTimeCreated (void) const;
  
  int GetTimeReceived (void) const;
  
  friend bool operator== (const Block &block1, const Block &block2);
 
private:	
  int m_blockHeight;
  int m_minerId;
  int m_parentBlockMinerId;
  int m_blockSizeBytes;
  int m_timeCreated;
  int m_timeReceived;
  
};

class Blockchain
{
public:
  Blockchain(void);
  virtual ~Blockchain (void);

  int GetNoStaleBlocks (void) const;
  
  bool HasBlock (Block newBlock);
  
  Block* GetCurrentTopBlock (void) const;
  void SetCurrentTopBlock (void);
  void SetCurrentTopBlock (Block *currentTopBlock);
  
  void AddBlock (Block newBlock);
  
private:
  int m_noStaleBlocks;						//total number of stale blocks
  std::vector<std::vector<Block>> m_blocks;	//2d vector containing all the blocks. (row->blockHeight, col->sibling blocks)
  Block *m_currentTopBlock;					//The top block in the Blockchain
};

//bool operator== (const Block &block1, const Block &block2);

}// Namespace ns3

#endif /* BITCOIN_H */

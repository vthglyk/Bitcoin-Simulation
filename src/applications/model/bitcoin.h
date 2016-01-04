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
  Block (int blockHeight, int minerId, int parentBlockMinerId, int blockSizeBytes, double timeCreated, double timeReceived);
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
  
  friend bool operator== (const Block &block1, const Block &block2);
  friend std::ostream& operator<< (std::ostream &out, Block &block);
  
private:	
  int m_blockHeight;
  int m_minerId;
  int m_parentBlockMinerId;
  int m_blockSizeBytes;
  double m_timeCreated;
  double m_timeReceived;
  
};

class Blockchain
{
public:
  Blockchain(void);
  virtual ~Blockchain (void);

  int GetNoStaleBlocks (void) const;
  
  int GetTotalBlocks (void) const;

  bool HasBlock (const Block &newBlock);
  
  Block* GetCurrentTopBlock (void);
  
  int GetBlockchainHeight (void);
  
  void AddBlock (Block& newBlock);
  
  friend std::ostream& operator<< (std::ostream &out, Blockchain &blockchain);

private:
  int m_noStaleBlocks;						//total number of stale blocks
  int m_totalBlocks;						//total number of blocks including genesis block
  std::vector<std::vector<Block>> m_blocks;	//2d vector containing all the blocks. (row->blockHeight, col->sibling blocks)

};

//bool operator== (const Block &block1, const Block &block2);

}// Namespace ns3

#endif /* BITCOIN_H */

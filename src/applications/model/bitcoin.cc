#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/address.h"
#include "ns3/log.h"
#include "bitcoin.h"

namespace ns3 {


/**
 *
 * Class Block functions
 *
 */

Block::Block(int blockHeight, int minerId, int parentBlockMinerId, int blockSizeBytes, double timeCreated, double timeReceived)
{  
  m_blockHeight = blockHeight;
  m_minerId = minerId;
  m_parentBlockMinerId = parentBlockMinerId;
  m_blockSizeBytes = blockSizeBytes;
  m_timeCreated = timeCreated;
  m_timeReceived = timeReceived;
}

Block::~Block (void)
{
}

int 
Block::GetBlockHeight (void) const
{
  return m_blockHeight;
}

void
Block::SetBlockHeight (int blockHeight)
{
  m_blockHeight = blockHeight;
}

int 
Block::GetMinerId (void) const
{
  return m_minerId;
}

void 
Block::SetMinerId (int minerId)
{
  m_minerId = minerId;
}

int 
Block::GetParentBlockMinerId (void) const
{
  return m_parentBlockMinerId;
}

void 
Block::SetParentBlockMinerId (int parentBlockMinerId)
{
  m_parentBlockMinerId = parentBlockMinerId;
}

int 
Block::GetBlockSizeBytes (void) const
{
  return m_blockSizeBytes;
}

void 
Block::SetBlockSizeBytes (int blockSizeBytes)
{
  m_blockSizeBytes = blockSizeBytes;
}

double 
Block::GetTimeCreated (void) const
{
  return m_timeCreated;
}
  
double 
Block::GetTimeReceived (void) const
{
  return m_timeReceived;
}

bool 
Block::IsParent(const Block &block) const
{
  if (GetBlockHeight() == block.GetBlockHeight() - 1 && GetMinerId() == block.GetParentBlockMinerId())
    return true;
  else
	return false;
}

bool 
Block::IsChild(const Block &block) const
{
  if (GetBlockHeight() == block.GetBlockHeight() + 1 && GetParentBlockMinerId() == block.GetMinerId())
    return true;
  else
	return false;
}


/**
 *
 * Class Blockchain functions
 *
 */
 
Blockchain::Blockchain(void)
{
  m_noStaleBlocks = 0;
  m_totalBlocks = 0;
  Block genesisBlock(0, -1, -1, 0, 0, 0);
  AddBlock(genesisBlock); 
}

Blockchain::~Blockchain (void)
{
}

int 
Blockchain::GetNoStaleBlocks (void) const
{
  return m_noStaleBlocks;
}

int 
Blockchain::GetTotalBlocks (void) const
{
  return m_totalBlocks;
}

Block* 
Blockchain::GetCurrentTopBlock (void)
{
  return &m_blocks[m_blocks.size() - 1][0];
}

int 
Blockchain::GetBlockchainHeight (void)
{
  return GetCurrentTopBlock()->GetBlockHeight();
}

bool 
Blockchain::HasBlock (const Block &newBlock)
{
  
  if (newBlock.GetBlockHeight() > GetCurrentTopBlock()->GetBlockHeight())		//The new block has a new blockHeight, so we haven't received it previously.
	return false;
  else														//The new block doesn't have a new blockHeight,
  {															//so we have to check it is new or if we have already received it.
    for (auto const &block: m_blocks[newBlock.GetBlockHeight()]) 
    {
      if (block == newBlock)
	  {
	    return true;
	  }
    }
  }
  return false;
}

const Block* 
Blockchain::GetBlockPointer (const Block &newBlock)
{
  
  for (auto const &block: m_blocks[newBlock.GetBlockHeight()]) 
  {
    if (block == newBlock)
    {
	  return &block;
	}
  }
  return nullptr;
}
 
std::vector<const Block *> 
Blockchain::GetChildrenPointers (const Block &newBlock)
{
  std::vector<const Block *> children;
  std::vector<Block>::iterator  block_it;
  
  if (newBlock.GetBlockHeight() + 1 > GetBlockchainHeight())
    return children;

  for (block_it = m_blocks[newBlock.GetBlockHeight() + 1].begin();  block_it < m_blocks[newBlock.GetBlockHeight() + 1].end(); block_it++)
  {
    if (newBlock.IsParent(*block_it))
    {
	  Block printBlock (newBlock.GetBlockHeight(), newBlock.GetMinerId(), newBlock.GetParentBlockMinerId(),
	                    newBlock.GetBlockSizeBytes(), newBlock.GetTimeCreated(), newBlock.GetTimeReceived());
	  
	  std::cout << "Block "  << printBlock << " is the parent of block " << *block_it << std::endl;
	  children.push_back(&(*block_it));
	}
  }
  return children;
}

Block* 
Blockchain::GetParent (const Block &newBlock)
{
  std::vector<Block>::iterator  block_it;
  
  for (block_it = m_blocks[newBlock.GetBlockHeight() - 1].begin();  block_it < m_blocks[newBlock.GetBlockHeight() - 1].end(); block_it++)  {
    if (newBlock.IsChild(*block_it))
    {
	  return &(*block_it);
	}
  }
  return nullptr;
}

void 
Blockchain::AddBlock (Block& newBlock)
{

  if (m_blocks.size() == 0)
  {
    std::vector<Block> newHeight(1, newBlock);
	m_blocks.push_back(newHeight);
  }	
  else if (newBlock.GetBlockHeight() > GetCurrentTopBlock()->GetBlockHeight())   		//The new block has a new blockHeight, so have to create a new vector (row)
  {
	/**
	 * If we receive an orphan block we have to create the dummy rows for the missing blocks as well
	 */
	int dummyRows = newBlock.GetBlockHeight() - GetCurrentTopBlock()->GetBlockHeight() - 1;
	
	for(int i = 0; i < dummyRows; i++)
	{  
	  std::vector<Block> newHeight; 
	  m_blocks.push_back(newHeight);
	}
	
    std::vector<Block> newHeight(1, newBlock);
	m_blocks.push_back(newHeight);
  }
  else														//The new block doesn't have a new blockHeight,
  {															//so we have to add it in an existing row
    if (m_blocks[newBlock.GetBlockHeight()].size() > 0)
	  m_noStaleBlocks++;									

    m_blocks[newBlock.GetBlockHeight()].push_back(newBlock);   
  }
  
  m_totalBlocks++;
}

bool operator== (const Block &block1, const Block &block2)
{
  if (block1.GetBlockHeight() == block2.GetBlockHeight() && block1.GetMinerId() == block2.GetMinerId() && block1.GetParentBlockMinerId() == block2.GetParentBlockMinerId())
    return true;
  else
	return false;
}

std::ostream& operator<< (std::ostream &out, const Block &block)
{

    out << "(m_blockHeight: " << block.GetBlockHeight() << ", " <<
        "m_minerId: " << block.GetMinerId() << ", " <<
        "m_parentBlockMinerId: " << block.GetParentBlockMinerId() << ", " <<
		"m_blockSizeBytes: " << block.GetBlockSizeBytes() << ", " <<
		"m_timeCreated: " << block.GetTimeCreated() << ", " <<
		"m_timeReceived: " << block.GetTimeReceived() <<
		")";
    return out;
}

std::ostream& operator<< (std::ostream &out, Blockchain &blockchain)
{
  
  std::vector< std::vector<Block>>::iterator blockHeight_it;
  std::vector<Block>::iterator  block_it;
  int i;
  
  for (blockHeight_it = blockchain.m_blocks.begin(), i = 0; blockHeight_it < blockchain.m_blocks.end(); blockHeight_it++, i++) 
  {
	out << "  BLOCK HEIGHT " << i << ":\n";
    for (block_it = blockHeight_it->begin();  block_it < blockHeight_it->end(); block_it++)
	{
	  out << *block_it << "\n";
    }
  }
  
  return out;
}

const char* getMessageName(enum Messages m) 
{
   switch (m) 
   {
      case INV: return "INV";
      case GET_HEADERS: return "GET_HEADERS";
      case HEADERS: return "HEADERS";
      case GET_BLOCKS: return "GET_BLOCKS";
      case BLOCK: return "BLOCK";
      case GET_DATA: return "GET_DATA";
   }
}

}// Namespace ns3

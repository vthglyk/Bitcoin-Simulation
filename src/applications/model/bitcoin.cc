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
  bool found = false;
  
  if (newBlock.GetBlockHeight() > GetCurrentTopBlock()->GetBlockHeight())		//The new block has a new blockHeight, so we haven't received it previously.
	return false;
  else														//The new block doesn't have a new blockHeight,
  {															//so we have to check it is new or if we have already received it.
    for (auto const &block: m_blocks[newBlock.GetBlockHeight()]) 
    {
      if (block == newBlock)
	  {
	    found = true;
        break;
	  }
    }
  }
  return found;
}

const Block* 
Blockchain::GetBlockPointer (const Block &newBlock)
{
  Block* pointer = nullptr;
  
  for (auto const &block: m_blocks[newBlock.GetBlockHeight()]) 
  {
    if (block == newBlock)
    {
	  return &block;
      break;
	}
  }
  
}
  
void 
Blockchain::AddBlock (Block& newBlock)
{

  if (m_blocks.size() == 0 || newBlock.GetBlockHeight() > GetCurrentTopBlock()->GetBlockHeight())			//The new block has a new blockHeight, so have to create a new vector (row)
  {
    std::vector<Block> newHeight(1, newBlock);
	m_blocks.push_back(newHeight);
  }
  else														//The new block doesn't have a new blockHeight,
  {															//so we have to add it in an existing row
    m_blocks[newBlock.GetBlockHeight()].push_back(newBlock);   
	m_noStaleBlocks++;
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

std::ostream& operator<< (std::ostream &out, Block &block)
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

  for (blockHeight_it = blockchain.m_blocks.begin(); blockHeight_it < blockchain.m_blocks.end(); blockHeight_it++) 
  {
    for (block_it = blockHeight_it->begin();  block_it < blockHeight_it->end(); block_it++)
	{
	  out << *block_it << std::endl;
    }
	out << std::endl;
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

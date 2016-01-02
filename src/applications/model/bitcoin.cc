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

Block::Block(int blockHeight, int minerId, int parentBlockMinerId, int blockSizeBytes, int timeCreated, int timeReceived)
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

int 
Block::GetTimeCreated (void) const
{
  return m_timeCreated;
}
  
int 
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
}

Blockchain::~Blockchain (void)
{
}

int 
Blockchain::GetNoStaleBlocks (void) const
{
  return m_noStaleBlocks;
}

bool 
Blockchain::HasBlock (const Block newBlock)
{
  bool found = false;
  
  if (newBlock.GetBlockHeight() > m_blocks.size())			//The new block has a new blockHeight, so we haven't received it previously.
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

Block* 
Blockchain::GetCurrentTopBlock (void) const
{
  return m_currentTopBlock;
}

void 
Blockchain::SetCurrentTopBlock (void)
{
  m_currentTopBlock = &m_blocks.back()[0];
}

void 
Blockchain::SetCurrentTopBlock (Block *currentTopBlock)
{
  m_currentTopBlock = currentTopBlock;
}

void 
Blockchain::AddBlock (Block newBlock)
{

  if (newBlock.GetBlockHeight() > m_blocks.size())			//The new block has a new blockHeight, so have to create a new vector (row)
  {
    std::vector<Block> newHeight(1, newBlock);
	m_blocks.push_back(newHeight);
  }
  else														//The new block doesn't have a new blockHeight,
  {															//so we have to add it in an existing row
    m_blocks[newBlock.GetBlockHeight()].push_back(newBlock);   
  }
}

bool operator== (const Block &block1, const Block &block2)
{
  if (block1.GetBlockHeight() == block2.GetBlockHeight() && block1.GetMinerId() == block2.GetMinerId() && block1.GetParentBlockMinerId() == block2.GetParentBlockMinerId())
    return true;
  else
	return false;
}

}// Namespace ns3

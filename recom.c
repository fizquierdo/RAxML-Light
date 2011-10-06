#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>

#include "axml.h"
boolean needsRecomp(tree *tr, nodeptr p)
{ 
  if(tr->useRecom)
  {
    if(!p->x || !isNodePinned(tr, p->number)) 
    {
      if(isNodeScheduled(tr, p->number))
        return FALSE;
      else
        return TRUE;
    }
    else      
    {       
      return FALSE;
    } 
  }
  else   
  {
    if(!p->x)
      return TRUE;
    else
      return FALSE;
  }
}
    
void allocRecompVectors(tree *tr, size_t width)
{
  recompVectors *v;
  int num_inner_nodes, num_vectors, i, j;
  num_inner_nodes = tr->mxtips - 2;
  printBothOpen("recom frac %f \n", tr->vectorRecomFraction);
  assert(tr->vectorRecomFraction > 0.0);
  assert(tr->vectorRecomFraction < 1.0);
  num_vectors = (int) (1 + tr->vectorRecomFraction * (float)num_inner_nodes); 
  printBothOpen("num_vecs %d , min %d\n", num_vectors, (int)log((double)tr->mxtips) + 2);
  assert(num_vectors > (int)log((double)tr->mxtips) + 2);
  assert(num_vectors < tr->mxtips);
  //if (num_vectors < 20) 
    //num_vectors = 20; // make my life easier
  v = (recompVectors *) malloc(sizeof(recompVectors));
  v->width = width;
  v->numVectors = num_vectors; // use minimum bound theoretical
  printBothOpen("Allocating space for %d inner vectors of width %d\n", v->numVectors, v->width);
  v->tmpvectors = (double **)malloc_aligned(num_vectors * sizeof(double *));
  for(i=0; i<num_vectors; i++)
  {
    v->tmpvectors[i] = (double *)malloc_aligned(width * sizeof(double));
    for(j=0; j< width; j++)
      v->tmpvectors[i][j] = INVALID_VALUE;
  }
  /* init vectors tracking */
  v->iVector = (int *) malloc((size_t)num_vectors * sizeof(int));
  v->unpinPrio = (int *) malloc((size_t)num_vectors * sizeof(int));
  v->nextPrio = 0;
  v->unpinnable = (boolean *) malloc((size_t)num_vectors * sizeof(boolean));
  for(i=0; i<num_vectors; i++)
  {
    v->iVector[i] = SLOT_UNUSED;
    v->unpinPrio[i] = SLOT_UNUSED;
    v->unpinnable[i] = FALSE;
  }
  v->usePrioList = FALSE;
  v->iNode = (int *) malloc((size_t)num_inner_nodes * sizeof(int));
  v->stlen = (int *) malloc((size_t)num_inner_nodes * sizeof(int));
  for(i=0; i<num_inner_nodes; i++)
  {
    v->iNode[i] = NODE_UNPINNED;
    v->stlen[i] = INNER_NODE_INIT_STLEN;
  }
  v->allSlotsBusy = FALSE;
  /* init nodes tracking */
  v->maxVectorsUsed = 0;
  tr->rvec = v;
}

void freeRecompVectors(recompVectors *v)
{
  int i;
  for(i=0; i<v->numVectors; i++)
    free(v->tmpvectors[i]);
  free(v->tmpvectors);
  free(v->iVector);
  free(v->iNode);
  free(v->stlen);
  free(v->unpinPrio);
  free(v);
}

int findFreeSlot(tree *tr)
{
  recompVectors *v = tr->rvec;
  assert(v->allSlotsBusy == FALSE);
  int slotno = -1, i;
  for(i=0; i < v->numVectors; i++)
  {
    if(v->iVector[i] == SLOT_UNUSED)
    {
      slotno = i;
      break;
    } 
  }
  if(slotno == -1)
  {
    v->allSlotsBusy = TRUE;
    slotno = findUnpinnableSlot(tr);
  }
  return slotno;
}
boolean isNodePinned(tree *tr, int nodenum)
{
  assert(nodenum > tr->mxtips);
  if (tr->rvec->iNode[nodenum-tr->mxtips-1] == NODE_UNPINNED)
    return FALSE;
  else
    return TRUE;
}
boolean isNodePinnedAndActive(tree *tr, int nodenum)
{
  if (isTip(nodenum, tr->mxtips))
    return TRUE;
  assert(nodenum > tr->mxtips);
  int slot;
  slot = tr->rvec->iNode[nodenum-tr->mxtips-1];
  if(slot == NODE_UNPINNED)
  {
    return FALSE; // because it is not pinned
  }
  else
  {
    if(tr->rvec->tmpvectors[slot][0] == INVALID_VALUE)
      return FALSE; // because it is not computed
    else
      return TRUE;
  }
}
void pinAtomicNode(tree *tr, int nodenum, int slot)
{
  recompVectors *v = tr->rvec;
  v->iVector[slot] = nodenum;
  v->iNode[nodenum - tr->mxtips - 1] = slot;
  v->unpinnable[slot] = FALSE;
}
int pinNode(tree *tr, int nodenum)
{
  int slot, i;
  assert(!isNodePinned(tr, nodenum));
  if(tr->rvec->allSlotsBusy)
    slot = findUnpinnableSlot(tr);
  else
    slot = findFreeSlot(tr);
  assert(slot >= 0);
  pinAtomicNode(tr, nodenum, slot);
  if(slot > tr->rvec->maxVectorsUsed)
    tr->rvec->maxVectorsUsed = slot;
  assert(slot == tr->rvec->iNode[nodenum - tr->mxtips - 1]);
  return slot;
}
void unpinNode(tree *tr, int nodenum)
{
  recompVectors *v = tr->rvec;
  if(nodenum <= tr->mxtips)
    return;
  int slot = -1, i;
  assert(nodenum > tr->mxtips);
  slot = tr->rvec->iNode[nodenum-tr->mxtips-1];
  assert(slot >= 0 && slot < v->numVectors);
  v->unpinnable[slot] = TRUE;
}
static void unpinAtomicSlot(tree *tr, int slot)
{
  int i, nodenum;
  recompVectors *v = tr->rvec;
  nodenum = v->iVector[slot];
  v->iVector[slot] = SLOT_UNUSED;
  if(nodenum != SLOT_UNUSED)
    v->iNode[nodenum - tr->mxtips - 1] = NODE_UNPINNED;
  else
    printBothOpen("WARNING unpinning a node that was not pinned\n");
  for(i=0; i < v->width; i++)
    v->tmpvectors[slot][i] = INVALID_VALUE;
}
void unpinAllSlots(tree *tr)
{
  int slot;
  for(slot=0; slot < tr->rvec->numVectors; slot++)
    unpinAtomicSlot(tr, slot);
}
int findUnpinnableSlotByCost(tree *tr)
{
  int i, slot, cheapest_slot, min_cost;
  recompVectors *v = tr->rvec;
  min_cost = tr->mxtips * 2; // more expensive than the most expensive
  cheapest_slot = -1;
  for(i=0; i< tr->mxtips - 2; i++)
  {
    slot = v->iNode[i];
    if(slot != NODE_UNPINNED)
    {
      assert(slot >=0 && slot < v->numVectors);
      if(v->unpinnable[slot])
      {
        assert(v->stlen[i] > 0);
        if (v->stlen[i] < min_cost)
        {
          min_cost = v->stlen[i];
          cheapest_slot = slot;
          /* if the slot costs 2 you can break cause there is nothing cheaper to recompute */
          if (min_cost == 2)
            break;
        }
      }
    }
  }
  assert(min_cost < tr->mxtips * 2 && min_cost >= 2);
  assert(cheapest_slot >= 0);
  return cheapest_slot;
}
/* priority list */
void traverseBackRec(tree *tr, nodeptr p)
{
  if(isTip(p->number, tr->mxtips))
    return;
  //printBothOpen(" %d", p->number);
  int slot;
  int pos = tr->rvec->nextPrio;
  slot = tr->rvec->iNode[p->number - tr->mxtips - 1];
  if (slot != NODE_UNPINNED)
  {
    assert(slot >= 0 && slot < tr->rvec->numVectors);
    if(tr->rvec->unpinnable[slot])
    {
      tr->rvec->unpinPrio[pos] = p->number;
      tr->rvec->nextPrio = pos + 1;
    }
    else
    {
      //printBothOpen(" W");
    }
  }
  traverseBackRec(tr, p->next->back);
  traverseBackRec(tr, p->next->next->back);
}
void resetUnpinPrio(recompVectors *v)
{
  int i;
  assert(v->nextPrio < v->numVectors);
  assert(v->nextPrio >= 0);
  for(i=0; i < v->nextPrio; i++)
    v->unpinPrio[i] = SLOT_UNUSED;
  v->nextPrio = 0;
}
void updatePrioNodes(tree *tr, nodeptr p)
{
  resetUnpinPrio(tr->rvec);
  assert(!isTip(p->back->number, tr->mxtips));
  traverseBackRec(tr, p->back);
}
void set_node_priority(tree *tr, nodeptr p)
{
  if(!isTip(p->back->number, tr->mxtips)) 
  {
    updatePrioNodes(tr, p);
    tr->rvec->usePrioList = TRUE;
  }
  else
    tr->rvec->usePrioList = FALSE;
}
/* end priority list */
void showUnpinnableNodes(tree *tr)
{
  int slot, count = 0;
  recompVectors *v = tr->rvec;
  printBothOpen(" unpinnable: ");
  for(slot=0; slot < v->numVectors; slot++)
  {
    if(v->unpinnable[slot])
    {
      printBothOpen(" %d", v->iVector[slot]);
      count++;
    }
  }
  printBothOpen(" [%d/%d]\n", count, v->numVectors);
}
int findUnpinnableSlot(tree *tr)
{
  int i, slot;
  int node_unpinned;
  recompVectors *v = tr->rvec;
  int slot_unpinned = -1;
  /* try using the prio list */
  if(v->usePrioList && v->nextPrio > 0)
  {
    int listnode;
    int pos = v->nextPrio - 1;
    while(pos >= 0)
    {
      listnode = v->unpinPrio[pos];
      slot_unpinned = v->iNode[listnode - tr->mxtips - 1];
      assert(v->iVector[slot_unpinned] == listnode);
      v->unpinPrio[pos] = SLOT_UNUSED;
      v->nextPrio = pos;
      if(v->unpinnable[slot_unpinned])
        break;
      else
      {
        pos--;
        slot_unpinned = -1;
      }
    }
  }
  /* uses minimum cost if prio list failed */
  if(slot_unpinned == -1)
    slot_unpinned = findUnpinnableSlotByCost(tr);

  assert(slot_unpinned >= 0);
  assert(v->unpinnable[slot_unpinned]);
  unpinAtomicSlot(tr, slot_unpinned);
  return slot_unpinned;
}

int countPinnedNodes(recompVectors *v)
{
  int count = 0;
  int i;
  for(i=0; i < v->numVectors; i++)
  {
    if(v->iVector[i] != SLOT_UNUSED)
      count++;
  }
  return count;
}


void getxVector(tree *tr, int nodenum, int *slot)
{
  double tstart = gettime();
  *slot = tr->rvec->iNode[nodenum - tr->mxtips - 1];
  if(*slot == NODE_UNPINNED)
    *slot = pinNode(tr, nodenum);
  tr->rvec->unpinnable[*slot] = FALSE;
  tr->rvec->pinTime += gettime() - tstart;
}
int tipsPartialCountStlen(int maxTips, nodeptr p, recompVectors *rvec)
{
  assert(rvec != NULL);
  int ntips1, ntips2, ntips;
  if(isTip(p->number, maxTips))
  {
    return 1;
  }
  else
  {
    nodeptr p1, p2; 
    p1 = p->next->back;
    ntips1 = rvec->stlen[p1->number - maxTips - 1]; 
    if(!p1->x || ntips1 == INNER_NODE_INIT_STLEN)
    {
      ntips1 = tipsPartialCountStlen(maxTips, p1, rvec);
    }
    assert(ntips1 >= 1 && ntips1 <= maxTips - 1);

    p2 = p->next->next->back;
    ntips2 = rvec->stlen[p2->number - maxTips - 1]; 
    if(!p2->x || ntips2 == INNER_NODE_INIT_STLEN)
    {
      ntips2 = tipsPartialCountStlen(maxTips, p2, rvec);
    }
    assert(ntips2 >= 1 && ntips2 <= maxTips - 1);
  }
  ntips = ntips1 + ntips2;
  assert(ntips >= 2 && ntips <= maxTips - 1);
  return ntips;
}

/*  
int traverseCountTipsPerSubtree(tree *tr, nodeptr p)
{
  int ntips1, ntips2;
  printBothOpen("traverse at p %d - %d\n", p->number, p->back->number);
  if(isTip(p->number, tr->mxtips))
  {
    return 1;
  }
  else
  {
    ntips1 = traverseCountTipsPerSubtree(tr, p->next->back);
    ntips2 = traverseCountTipsPerSubtree(tr, p->next->next->back);
    set_stlen(tr, p, ntips1 + ntips2);
  }
  return ntips1 + ntips2;
}
*/

boolean isNodeScheduled(tree *tr, int nodenum)
{
    //is the node shcheduled in the traversal?
    traversalInfo *ti   = tr->td[0].ti;
    int i; 
    for(i = 1; i < tr->td[0].count; i++)
    {
      traversalInfo *tInfo = &ti[i];
      if (tInfo->pNumber == nodenum)
        return TRUE;
    }
    return FALSE;
}
void protectNode(tree *tr, int nodenum)
{
  if (isTip(nodenum, tr->mxtips))
    return;
  int slot;
  if (isNodePinnedAndActive(tr, nodenum))
  {
    assert(nodenum > tr->mxtips);
    slot = tr->rvec->iNode[nodenum - tr->mxtips - 1];
    if (slot != NODE_UNPINNED)
    {
      assert(slot >= 0 && slot < tr->rvec->numVectors);
      if(tr->rvec->tmpvectors[slot][0] != INVALID_VALUE)
      {
        if(tr->rvec->unpinnable[slot])
        {
          //printBothOpen("WARNING: slot %d, node %d are unprotected, protecting...\n", slot, nodenum);
          tr->rvec->unpinnable[slot] = FALSE;
        }
      }
    }
  }
}
void protectNodesInTraversal(tree *tr)
{
    traversalInfo *ti   = tr->td[0].ti;
    int i, slot; 
    for(i = 1; i < tr->td[0].count; i++)
    {
      traversalInfo *tInfo = &ti[i];
      protectNode(tr, tInfo->qNumber);
      protectNode(tr, tInfo->rNumber);
    }
}
void reset_stlen(tree *tr)
{
  int num_inner_nodes = tr->mxtips - 2;
  int i;
  for(i=0; i< num_inner_nodes; i++)
    tr->rvec->stlen[i] = INNER_NODE_INIT_STLEN; 
}
void validate_stlen(nodeptr p, int maxTips, int value, int *stlen)
{
  int num_inner_nodes = maxTips - 2;
  int pnumber = p->number;
  int index = pnumber - maxTips - 1;

  boolean stlen_ok = TRUE;

  if(isTip(pnumber, maxTips)) 
  {
    printBothOpen("tips dont have stlen for n %d : %d\n", pnumber, value);
    stlen_ok = FALSE;
  }
  if(!(index >= 0 && index < num_inner_nodes))
  {
    printBothOpen("wrong index stlen for n %d : %d\n", pnumber, value);
    stlen_ok = FALSE;
  }
  if(!(value >= 2 && value <= maxTips - 1))
  {
    printBothOpen("wrong value: stlen for n %d : %d\n", pnumber, value);
    stlen_ok = FALSE;
  }
  if(!p->x)
  {
    printBothOpen("unexpected orientation p %d -b %d : l %d\n", pnumber, p->back->number, value);
    stlen_ok = FALSE;
  }
  assert(stlen_ok);
  printBothOpen("storing for p %d -b %d : l %d\n", pnumber, p->back->number, value);
  stlen[index] = value;
}
void set_stlen(tree *tr, nodeptr p, int value)
{
  validate_stlen(p, tr->mxtips, value, tr->rvec->stlen);
}
/* pre-compute the node stlens (this needs to be known prior to running the strategy) */
static void computeFullTraversalInfoStlen(nodeptr p, int maxTips, recompVectors *rvec) 
{
  int value;
  printBothOpen("Visited node %d\n", p->number);
  if(isTip(p->number, maxTips))
    return;

  nodeptr q = p->next->back;
  nodeptr r = p->next->next->back;
  if(isTip(r->number, maxTips) && isTip(q->number, maxTips))
  {
    printBothOpen("Tip %d - Tip %d\n", r->number, q->number);
    value = 2;
    printBothOpen("precomp for p %d -b %d : l %d\n", p->number, p->back->number, value);
  }
  else
  {
    if(isTip(r->number, maxTips) || isTip(q->number, maxTips))
    {
      // tip/vec
      nodeptr tmp;
      if(isTip(r->number, maxTips))
      {
        tmp = r;
        r = q;
        q = tmp;
      }
      printBothOpen("Tip %d - Vector %d\n", q->number, r->number);
      computeFullTraversalInfoStlen(r, maxTips, rvec);
    }
    else
    {
      // vec/vec
      printBothOpen("Vector %d - Vector %d\n", r->number, q->number);
      computeFullTraversalInfoStlen(r, maxTips, rvec);
      computeFullTraversalInfoStlen(q, maxTips, rvec); 
    }
  }
}
void determineFullTraversalStlen(nodeptr p, tree *tr)
{
  nodeptr q = p->back;
  assert(isTip(p->number, tr->mxtips));
  printBothOpen("Start stlen trav from %d\n", p->number);
  computeFullTraversalInfoStlen(p, tr->mxtips, tr->rvec); 
  printBothOpen("Start stlen trav from %d\n", q->number);
  computeFullTraversalInfoStlen(q, tr->mxtips, tr->rvec); 
}


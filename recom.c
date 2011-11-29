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
      //if(isNodeScheduled(tr, p->number))
      //  return FALSE;
      //else
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
  v->iVector_prev = (int *) malloc((size_t)num_vectors * sizeof(int));
  v->unpinPrio = (int *) malloc((size_t)num_vectors * sizeof(int));
  v->nextPrio = 0;
  v->unpinnable = (boolean *) malloc((size_t)num_vectors * sizeof(boolean));
  v->unpinnable_prev = (boolean *) malloc((size_t)num_vectors * sizeof(boolean));
  for(i=0; i<num_vectors; i++)
  {
    v->iVector[i] = SLOT_UNUSED;
    v->iVector_prev[i] = SLOT_UNUSED;
    v->unpinPrio[i] = SLOT_UNUSED;
    v->unpinnable[i] = FALSE;
    v->unpinnable_prev[i] = FALSE;
  }
  v->usePrioList = FALSE;
  v->iNode = (int *) malloc((size_t)num_inner_nodes * sizeof(int));
  v->iNode_prev = (int *) malloc((size_t)num_inner_nodes * sizeof(int));
  v->stlen = (int *) malloc((size_t)num_inner_nodes * sizeof(int));
  for(i=0; i<num_inner_nodes; i++)
  {
    v->iNode[i] = NODE_UNPINNED;
    v->iNode_prev[i] = NODE_UNPINNED;
    v->stlen[i] = INNER_NODE_INIT_STLEN;
  }
  v->allSlotsBusy = FALSE;
  v->allSlotsBusy_prev = FALSE;
  /* init nodes tracking */
  v->maxVectorsUsed = 0;
  tr->rvec = v;
}
/* running the strategy overwrites the current state*/
void save_strategy_state(tree *tr)
{
  if(!tr->useRecom)
    return;
  recompVectors *v = tr->rvec;
  int num_inner_nodes, i;
  num_inner_nodes = tr->mxtips - 2;
  for(i=0; i < v->numVectors; i++)
  {
    v->iVector_prev[i] = v->iVector[i];
    v->unpinnable_prev[i] = v->unpinnable[i];
  }
  for(i=0; i<num_inner_nodes; i++)
  {
    v->iNode_prev[i] = v->iNode[i];
  }
  v->allSlotsBusy_prev = v->allSlotsBusy;
}
void restore_strategy_state(tree *tr)
{
  if(!tr->useRecom)
    return;
  recompVectors *v = tr->rvec;
  int num_inner_nodes, i;
  num_inner_nodes = tr->mxtips - 2;
  for(i=0; i < v->numVectors; i++)
  {
    v->iVector[i] = v->iVector_prev[i];
    v->unpinnable[i] = v->unpinnable_prev[i];
  }
  for(i=0; i<num_inner_nodes; i++)
  {
    v->iNode[i] = v->iNode_prev[i];
  }
  v->allSlotsBusy = v->allSlotsBusy_prev;
}

void freeRecompVectors(recompVectors *v)
{
  int i;
  for(i=0; i<v->numVectors; i++)
    free(v->tmpvectors[i]);
  free(v->tmpvectors);
  free(v->iVector);
  free(v->iVector_prev);
  free(v->iNode);
  free(v->iNode_prev);
  free(v->stlen);
  free(v->unpinPrio);
  free(v->unpinnable);
  free(v->unpinnable_prev);
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
  /*
  int prev_nodenum;
  prev_node =  v->iVector[slot];
  */
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
  //assert(slot >= 0 && slot < v->numVectors);
  /* does this make sense? */
  if(slot >= 0 && slot < v->numVectors)
    v->unpinnable[slot] = TRUE;
}
void unpinAtomicSlot(tree *tr, int slot)
{
  int i, nodenum;
  recompVectors *v = tr->rvec;
  nodenum = v->iVector[slot];
  v->iVector[slot] = SLOT_UNUSED;
  if(nodenum != SLOT_UNUSED)
  {
    v->iNode[nodenum - tr->mxtips - 1] = NODE_UNPINNED;
  }
  else
  {
    //printBothOpen("WARNING unpinning a node that was not pinned\n");
  }

  /*
  for(i=0; i < v->width; i++)
    v->tmpvectors[slot][i] = INVALID_VALUE;
    */
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
void showTreeNodes(tree *tr)
{
  if(!tr->useRecom) 
    return;
  int i;
  nodeptr p, q;
  printBothOpen("tree map\n");
  for(i = tr->mxtips + 1; i <= tr->mxtips + tr->mxtips - 2; i++)
  {
    q = tr->nodep[i];
    p = q; 
    while(!p->x)
      p = p->next;
    printBothOpen("node %d b %d, nb %d nnb %d, stlen %d\n",  
        p->number, 
        p->back->number,  
        p->next->back->number,  
        p->next->next->back->number,
        tr->rvec->stlen[p->number - tr->mxtips - 1]);
  }
  printBothOpen("\n");
}
void showUnpinnableNodes(tree *tr)
{
  int slot, nodenum, count = 0;
  if (!tr->useRecom)
    return;
  recompVectors *v = tr->rvec;
  printBothOpen("Nodes in slots, unpinnable u: ");
  for(slot=0; slot < v->numVectors; slot++)
  {
    nodenum = v->iVector[slot];
    if(nodenum == SLOT_UNUSED)
    {
      printBothOpen(" -");
    }
    else
    {
      printBothOpen(" %d", nodenum);
      assert(v->iNode[nodenum - tr->mxtips - 1] == slot);
    }
    if(v->unpinnable[slot])
    {
      printBothOpen("u");
      count++;
    }
  }
  printBothOpen(" [%d/%d]\n", count, v->numVectors);
  int i, num_inner_nodes = tr->mxtips - 2;
  printBothOpen("Nodes id (len): ");
  for(i=0; i<num_inner_nodes; i++)
  {
    printBothOpen("%d(%d) ", i + tr->mxtips + 1, v->stlen[i]);
  }
  printBothOpen("\n");
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
  assert(*slot >= 0 && *slot < tr->rvec->numVectors);
  tr->rvec->unpinnable[*slot] = FALSE;
  tr->rvec->pinTime += gettime() - tstart;
}
void getxVectorReport(tree *tr, int nodenum, int *slot, boolean *slotNeedsRecomp)
{
  double tstart = gettime();
  assert(*slotNeedsRecomp == FALSE);
  *slot = tr->rvec->iNode[nodenum - tr->mxtips - 1];
  if(*slot == NODE_UNPINNED)
  {
    *slot = pinNode(tr, nodenum);
    *slotNeedsRecomp = TRUE;
  }
  assert(*slot >= 0 && *slot < tr->rvec->numVectors);
  tr->rvec->unpinnable[*slot] = FALSE;
  tr->rvec->pinTime += gettime() - tstart;
}

int tipsPartialCountStlen(int maxTips, nodeptr p, recompVectors *rvec)
{
  assert(rvec != NULL);
  int ntips1, ntips2, ntips, index;
  if(isTip(p->number, maxTips))
  {
    return 1;
  }
  else
  {
    nodeptr p1, p2; 
    p1 = p->next->back;
    if(isTip(p1->number, maxTips))
    {
      ntips1 = 1; 
    }
    else
    {
      index = p1->number - maxTips - 1; 
      assert(index >= 0 && index < maxTips - 2);
      ntips1 = rvec->stlen[index]; 
      if(!p1->x_stlen || ntips1 == INNER_NODE_INIT_STLEN)
        ntips1 = tipsPartialCountStlen(maxTips, p1, rvec);
    }
    assert(ntips1 >= 1 && ntips1 <= maxTips - 1);

    p2 = p->next->next->back;
    if(isTip(p2->number, maxTips))
    {
      ntips2 = 1; 
    }
    else
    {
      index = p2->number - maxTips - 1; 
      assert(index >= 0 && index < maxTips - 2);
      ntips2 = rvec->stlen[index]; 
      if(!p2->x_stlen || ntips2 == INNER_NODE_INIT_STLEN)
      {
        ntips2 = tipsPartialCountStlen(maxTips, p2, rvec);
      }
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
  if(!p->x_stlen)
  {
    printBothOpen("unexpected orientation p %d -b %d : l %d\n", pnumber, p->back->number, value);
    stlen_ok = FALSE;
  }
  assert(stlen_ok);
  //printBothOpen("storing for p %d -b %d : l %d\n", pnumber, p->back->number, value);
  stlen[index] = value;
}
void set_stlen(tree *tr, nodeptr p, int value)
{
  validate_stlen(p, tr->mxtips, value, tr->rvec->stlen);
}
void computeTraversalInfoStlen(nodeptr p, int maxTips, recompVectors *rvec, int *count) 
{
  int value;
  //printBothOpen("Visited node %d \n", p->number);
  if(isTip(p->number, maxTips) || p->x_stlen)
    return;

  nodeptr q = p->next->back;
  nodeptr r = p->next->next->back;

  *count += 1;
  /* set xnode info at this point */
  p->x_stlen = 1;
  p->next->x_stlen = 0;
  p->next->next->x_stlen = 0;     

  if(isTip(r->number, maxTips) && isTip(q->number, maxTips))
  {
    //printBothOpen("Tip %d - Tip %d\n", r->number, q->number);
    validate_stlen(p, maxTips, 2, rvec->stlen);
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
      computeTraversalInfoStlen(r, maxTips, rvec, count);

      int r_stlen = rvec->stlen[r->number - maxTips - 1];
      assert(r_stlen != INNER_NODE_INIT_STLEN);
      assert(r_stlen >= 2 && r_stlen <= maxTips - 1);
      validate_stlen(p, maxTips, r_stlen + 1, rvec->stlen);
     // printBothOpen("Tip %d - Vector %d : len %d\n", q->number, r->number, r_stlen + 1);
    }
    else
    {
      computeTraversalInfoStlen(r, maxTips, rvec, count);
      computeTraversalInfoStlen(q, maxTips, rvec, count); 
      int val = rvec->stlen[q->number - maxTips - 1] + rvec->stlen[r->number - maxTips - 1];
      validate_stlen(p, maxTips, val, rvec->stlen);
    }
  }
}
/* pre-compute the node stlens (this needs to be known prior to running the strategy) */
static void computeFullTraversalInfoStlen(nodeptr p, int maxTips, recompVectors *rvec, int *count) 
{
  int value;
  //printBothOpen("Visited node %d \n", p->number);
  if(isTip(p->number, maxTips))
    return;

  nodeptr q = p->next->back;
  nodeptr r = p->next->next->back;

  *count += 1;
  /* set xnode info at this point */
  p->x_stlen = 1;
  p->next->x_stlen = 0;
  p->next->next->x_stlen = 0;     

  if(isTip(r->number, maxTips) && isTip(q->number, maxTips))
  {
    //printBothOpen("Tip %d - Tip %d\n", r->number, q->number);
    validate_stlen(p, maxTips, 2, rvec->stlen);
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
      computeFullTraversalInfoStlen(r, maxTips, rvec, count);

      int r_stlen = rvec->stlen[r->number - maxTips - 1];
      assert(r_stlen != INNER_NODE_INIT_STLEN);
      assert(r_stlen >= 2 && r_stlen <= maxTips - 1);
      validate_stlen(p, maxTips, r_stlen + 1, rvec->stlen);
     // printBothOpen("Tip %d - Vector %d : len %d\n", q->number, r->number, r_stlen + 1);
    }
    else
    {
      // vec/vec
      /*
      int q_stlen, r_stlen;
      r_stlen = rvec->stlen[r->number - maxTips - 1];
      q_stlen = rvec->stlen[q->number - maxTips - 1];
      if(r_stlen == INNER_NODE_INIT_STLEN || !r->x_stlen)
        r_stlen = tipsPartialCountStlen(maxTips, r, rvec);
      if(q_stlen == INNER_NODE_INIT_STLEN || !q->x_stlen)
        q_stlen = tipsPartialCountStlen(maxTips, q, rvec);
        */

      computeFullTraversalInfoStlen(r, maxTips, rvec, count);
      computeFullTraversalInfoStlen(q, maxTips, rvec, count); 

      int val = rvec->stlen[q->number - maxTips - 1] + rvec->stlen[r->number - maxTips - 1];
      //printBothOpen("Vector %d - Vector %d , stlen %d\n", r->number, q->number, val);
      validate_stlen(p, maxTips, val, rvec->stlen);
    }
  }
}
void determineFullTraversalStlen(nodeptr p, tree *tr)
{
  double travTime = gettime();
  int count = 0;
  nodeptr q = p->back;
  //assert(isTip(p->number, tr->mxtips));
  //printBothOpen("Start stlen trav from %d\n", p->number);
  computeFullTraversalInfoStlen(p, tr->mxtips, tr->rvec, &count); 
  //printBothOpen("Start stlen trav from %d\n", q->number);
  computeFullTraversalInfoStlen(q, tr->mxtips, tr->rvec, &count); 
  tr->stlenTime += (gettime() - travTime);
  //printBothOpen("full trav: %d/%d visits\n", count, tr->mxtips - 2);
}
void printVector(double *vector, int pnumber)
{
  int j;
  printBothOpen("vector for node %d \n", pnumber);
  if (vector == NULL)
  {
    printBothOpen(" is NULL\n");
    return;
  }
  for(j=0; j<5; j++)
    printBothOpen("%.5f ", vector[j]);
  printBothOpen("\n");
}

void printTraversal(tree *tr)
{
  int i;
  traversalInfo *ti   = tr->td[0].ti;

  printBothOpen("Traversal: ");
  for(i = 1; i < tr->td[0].count; i++)
  {
    traversalInfo *tInfo = &ti[i];
    if(tr->useRecom)
      printBothOpen("%d->S%d ", tInfo->pNumber, tInfo->slot_p);
    else
      printBothOpen("%d ", tInfo->pNumber);
  }
  printBothOpen("\n");
}

traverseTree(tree *tr, nodeptr p, int *counter)
{
  double t;
  if (isTip(p->number,tr->mxtips))
    return;
  *counter += 1;

  printBothOpen("\nEvaluating branch p %d b%d \n", p->number, p->back->number);
  t = gettime();
  evaluateGeneric(tr, p);
  printBothOpen(" evalTime %f, LH %f \n", gettime() - t, tr->likelihood);

  //t = gettime();
  //evaluateGeneric(tr, p->back);
  //fprintf(stderr, " evalTime %f, LH %f \n", gettime() - t, tr->likelihood);

  traverseTree(tr, p->next->back, counter);
  traverseTree(tr, p->next->next->back, counter);
}


static char *Tree2StringRecomREC(char *treestr, tree *tr, nodeptr q, boolean printBranchLengths)
{
  char  *nameptr;            
  double z;
  nodeptr p = q;
  int slot;

  if(isTip(p->number, tr->rdta->numsp)) 
  {	       	  
    nameptr = tr->nameList[p->number];     
    sprintf(treestr, "%s", nameptr);
    while (*treestr) treestr++;
  }
  else 
  {                 	 
    while(!p->x)
      p = p->next;
    *treestr++ = '(';
    treestr = Tree2StringRecomREC(treestr, tr, q->next->back, printBranchLengths);
    *treestr++ = ',';
    treestr = Tree2StringRecomREC(treestr, tr, q->next->next->back, printBranchLengths);
    if(q == tr->start->back) 
    {
      *treestr++ = ',';
      treestr = Tree2StringRecomREC(treestr, tr, q->back, printBranchLengths);
    }
    *treestr++ = ')';                    
    // write innernode as nodenum_b_nodenumback
    sprintf(treestr, "%d", q->number);
    while (*treestr) treestr++;
    *treestr++ = 'b';                    
    sprintf(treestr, "%d", p->back->number);
    while (*treestr) treestr++;
    if(tr->useRecom)
    {
      *treestr++ = '_';                    
      //assert(p->x_stlen);
      if(!p->x_stlen)
        printf("WARN: x_stlen not oriented for %d\n", p->number);
      *treestr++ = 'l';                    
      sprintf(treestr, "%d", tr->rvec->stlen[p->number - tr->mxtips - 1]);
      while (*treestr) treestr++;
      *treestr++ = '_';                    
      *treestr++ = 'S';                    
      slot = tr->rvec->iNode[p->number - tr->mxtips - 1];
      if(slot == NODE_UNPINNED)
      {
        *treestr++ = '-';                    
      }
      else
      {
        //assert(tr->rvec->iVector[slot] == p->number);
        if(tr->rvec->iVector[slot] != p->number)
          printBothOpen("WARN: slot %d contains nodenum %d, displaying node %d\n", 
                        slot, tr->rvec->iVector[slot], p->number);
        sprintf(treestr, "%d", slot);
        while (*treestr) treestr++;
        if(tr->rvec->unpinnable[slot])
        {
          *treestr++ = 'u';                    
        }
        else
        {
          *treestr++ = 'p';                    
        }
      }
    }
  }

  if(q == tr->start->back) 
  {	      	 
    if(printBranchLengths)
      sprintf(treestr, ":0.0;\n");
    else
      sprintf(treestr, ";\n");	 	  	
  }
  else 
  {                   
    if(printBranchLengths)	    
    {
      //sprintf(treestr, ":%8.20f", getBranchLength(tr, SUMMARIZE_LH, p));	      	   
      assert(tr->fracchange != -1.0);
      z = q->z[0];
      if (z < zmin) 
        z = zmin;      	 
      sprintf(treestr, ":%8.20f", -log(z) * tr->fracchange);	      	   
    }
    else	    
      sprintf(treestr, "%s", "\0");	    
  }

  while (*treestr) treestr++;
  return  treestr;
}

void printRecomTree(tree *tr, boolean printBranchLengths, char *title)
{
  if(!tr->verbose)
    return;
  FILE *nwfile;
  nwfile = myfopen("tmp.nw", "w+");
  Tree2StringRecomREC(tr->tree_string, tr, tr->start->back, printBranchLengths);
  //fprintf(stderr,"%s\n", tr->tree_string);
  fprintf(nwfile,"%s\n", tr->tree_string);
  fclose(nwfile);
  if(title)
    printBothOpen("%s\n", title);
  printBothOpen("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
  system("bin/nw_display tmp.nw");
}

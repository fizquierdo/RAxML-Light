#!/bin/bash

DATADIR=data
NAME=TEST

#SET=50
#TREE=RAxML_parsimonyTree.50sim.0
SET=7
TREE=RAxML_parsimonyTree.7_s12345.0

FACTOR=0.60

# just clean dir
if [ $1 = clean ] ; then
  rm *.o
  rm *${NAME}*
  exit
fi

# compile
if [ $1 = cmp ] ; then
  rm *.o
  rm raxmlLight
  make -f Makefile.SSE3.gcc
fi

#run
rm *${NAME}*
if [ $1 = pro ] ; then
  valgrind ./raxmlLight -r $FACTOR -m GTRCAT -n ${NAME} -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE}
else
  echo "*** run recom"
  ./raxmlLight -r $FACTOR -m GTRCAT -n ${NAME} -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE}
  #FACTOR=1.1
  #./raxmlLight -r $FACTOR -m GTRCAT -n ${NAME}_hf -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE}
  echo "*** run std"
  ./raxmlLight -m GTRCAT -n ${NAME}_std -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE} > /dev/null
  #echo "diff"
  #diff RAxML_result.${NAME} RAxML_result.${NAME}_std
  echo "diff of LHs"
  grep "Likelihood" RAxML_info.${NAME}_std
  grep "Likelihood" RAxML_info.${NAME}
fi

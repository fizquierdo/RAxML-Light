#!/bin/bash



DATADIR=../RAxML-Light-1.0.4/data
NAME=TEST

SET=50
TREE=RAxML_parsimonyTree.50sim.0
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
  ./raxmlLight -r $FACTOR -m GTRCAT -n ${NAME} -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE}
  FACTOR=1.1
  ./raxmlLight -r $FACTOR -m GTRCAT -n ${NAME}_hf -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE}
  ./raxmlLight -m GTRCAT -n ${NAME}_std -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE}
fi

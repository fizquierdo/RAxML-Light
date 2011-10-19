#!/bin/bash

DATADIR=data
NAME=TEST

if [ $1 = 50 ] ; then
 SET=50
 TREE=RAxML_parsimonyTree.50sim.0
elif [ $1 = 150 ] ; then
 SET=150
 TREE=intree150
elif [ $1 = 1288 ] ; then
 SET=1288
 TREE=intree1288
else
 SET=7
 TREE=RAxML_parsimonyTree.7_s12345.0
fi

FACTOR=0.90

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
  #rm *.o
  #make -f Makefile.SSE3.PTHREADS.gcc
fi

#run
rm *${NAME}*
if [ $1 = pro ] ; then
  valgrind ./raxmlLight -r $FACTOR -m GTRCAT -n ${NAME} -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE}
  #valgrind ./raxmlLight -m GTRCAT -n ${NAME}_std -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE} > /dev/null
  #valgrind ./raxmlLight-PTHREADS -r $FACTOR -T 2 -m GTRCAT -n ${NAME}_T2 -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE}
else
  echo "*** run recom"
  ./raxmlLight -r $FACTOR -m GTRCAT -n ${NAME} -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE} 2> err
  #./raxmlLight-PTHREADS -r $FACTOR -T 2 -m GTRCAT -n ${NAME}_T2 -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE}
  #FACTOR=1.1
  #./raxmlLight -r $FACTOR -m GTRCAT -n ${NAME}_hf -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE}
  echo "*** run std"
  (./raxmlLight -m GTRCAT -n ${NAME}_std -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE} 2> err_std) > /dev/null
  echo "diff of LHs std. recomp"
  grep "Likelihood" RAxML_info.${NAME}_std
  grep "Likelihood" RAxML_info.${NAME}
  #grep "Likelihood" RAxML_info.${NAME}_T2
fi


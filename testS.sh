#!/bin/bash

DATADIR=datar
MODEL=GTRCAT
NAME=TEST
PARTITION_CALL=""

if [ $1 = 50 ] ; then
 SET=50
 TREE=RAxML_parsimonyTree.50sim.0
elif [ $1 = 20 ] ; then
 SET=20
 TREE=intree20
 #PARTITION_CALL="-q ${DATADIR}/20.model "
 #PARTITION_CALL="-q ${DATADIR}/20.model -M "
elif [ $1 = 10 ] ; then
 SET=10
 TREE=intree10
elif [ $1 = 8 ] ; then
 SET=8
 TREE=intree8
elif [ $1 = 150 ] ; then
 SET=150
 TREE=intree150
 #PARTITION_CALL="-q ${DATADIR}/20.model"
elif [ $1 = 1288 ] ; then
 SET=1288
 TREE=intree1288
else
 SET=20
 TREE=intree20
 PARTITION_CALL="-q ${DATADIR}/20.model -M "
 #PARTITION_CALL="-q ${DATADIR}/20.model "

 #SET=50
 #TREE=RAxML_parsimonyTree.50sim.0
fi

FACTOR=0.55
NUM_THREADS=4

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
  #rm raxmlLight-PTHREADS
  #make -f Makefile.SSE3.PTHREADS.gcc
  #rm *.o
  #rm raxmlLight-MPI
  #make -f Makefile.SSE3.MPI
fi

#run
rm *${NAME}*
  RECOM=" -r 0.8 "
  #RECOM=""
  echo "*** run recom"
  #valgrind ./raxmlLight $RECOM -m $MODEL -n ${NAME}_std -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE}
  #./raxmlLight $RECOM -m $MODEL -n ${NAME}_std -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE} 
  echo "*** run S -----------------------------------"
  valgrind ./raxmlLight $RECOM -m $MODEL -n ${NAME}_stdS -s ${DATADIR}/${SET} -S -t ${DATADIR}/${TREE}
  #./raxmlLight $RECOM -m $MODEL -n ${NAME}_stdS -s ${DATADIR}/${SET} -S -t ${DATADIR}/${TREE}
  #echo "*** run pthreads"
  #./raxmlLight-PTHREADS -T $NUM_THREADS -m $MODEL -n ${NAME}_T${NUM_THREADS} -s ${DATADIR}/${SET} $PARTITION_CALL -t ${DATADIR}/${TREE} 
  #./raxmlLight-PTHREADS -T $NUM_THREADS -m $MODEL -n ${NAME}_T${NUM_THREADS}_norec -s ${DATADIR}/${SET} $PARTITION_CALL -t ${DATADIR}/${TREE} 
  #cp RAxML_info.${NAME}_T${NUM_THREADS} brm_treads
  #echo "*** run mpi"
  #mpirun.openmpi -np 4 ./raxmlLight-MPI -m $MODEL -n ${NAME}_mpi -s ${DATADIR}/${SET} $PARTITION_CALL -t ${DATADIR}/${TREE} 
  #cp RAxML_info.${NAME}_mpi brm_mpi
  #tail brm*

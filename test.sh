#!/bin/bash

DATADIR=data
NAME=TEST
PARTITION_CALL=""

if [ $1 = 50 ] ; then
 SET=50
 TREE=RAxML_parsimonyTree.50sim.0
elif [ $1 = 20 ] ; then
 SET=20
 TREE=intree20
 PARTITION_CALL="-q ${DATADIR}/20.model "
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
elif [ $1 = 1288 ] ; then
 SET=1288
 TREE=intree1288
else
 SET=7
 TREE=RAxML_parsimonyTree.7_s12345.0

 SET=8
 TREE=intree8

 #SET=50
 #TREE=RAxML_parsimonyTree.50sim.0
fi

FACTOR=0.70
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
  rm *.o
  rm raxmlLight-PTHREADS
  make -f Makefile.SSE3.PTHREADS.gcc
  #rm *.o
  #rm raxmlLight-MPI
  #make -f Makefile.SSE3.MPI
fi

#run
rm *${NAME}*
if [ $1 = pro ] ; then
  #valgrind ./raxmlLight -r $FACTOR -m GTRCAT -n ${NAME} -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE}
  #valgrind ./raxmlLight -m GTRCAT -n ${NAME}_std -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE} > /dev/null
  #valgrind ./raxmlLight-PTHREADS -r $FACTOR -T $NUM_THREADS -m GTRCAT -n ${NAME}_T2 -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE}
  valgrind ./raxmlLight-PTHREADS -T $NUM_THREADS -m GTRCAT -n ${NAME}_T${NUM_THREADS} -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE}
else
  echo "*** run recom"
  ./raxmlLight -r $FACTOR -m GTRCAT -n ${NAME} -s ${DATADIR}/${SET} $PARTITION_CALL -t ${DATADIR}/${TREE} 
  cp RAxML_info.${NAME} brm_recom
  echo "*** run std"
  ./raxmlLight -m GTRCAT -n ${NAME}_std -s ${DATADIR}/${SET} $PARTITION_CALL -t ${DATADIR}/${TREE} > /dev/null
  cp RAxML_info.${NAME}_std brm_std
  echo "*** run pthreads"
  ./raxmlLight-PTHREADS -r $FACTOR -T $NUM_THREADS -m GTRCAT -n ${NAME}_T${NUM_THREADS} -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE} 
  cp RAxML_info.${NAME}_T${NUM_THREADS} brm_treads
  #echo "*** run mpi"
  #mpirun.openmpi -np 4 ./raxmlLight-MPI -r $FACTOR -m GTRCAT -n ${NAME}_mpi -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE} 
  #mpirun.openmpi -np 2 ./raxmlLight-MPI -m GTRCAT -n ${NAME}_mpi -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE} 
  #cp RAxML_info.${NAME}_T${NUM_THREADS} brm_mpi
  tail brm*
fi


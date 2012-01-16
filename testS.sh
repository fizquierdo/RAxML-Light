#!/bin/bash

DATADIR=datar
MODEL=GTRCAT
NAME=TEST

if [ $1 = 150 ] ; then
  SET=150
  TREE=intree150
else
  SET=20
  TREE=intree20
fi


# compile
if [ $1 = cmp ] ; then
  rm *.o
  rm raxmlLight
  make -f Makefile.SSE3.gcc
  rm *.o
  rm raxmlLight-PTHREADS
  make -f Makefile.SSE3.PTHREADS.gcc
  rm *.o
  rm raxmlLight-MPI
  make -f Makefile.SSE3.MPI
fi

#run
NUM_THREADS=4
PARTITION_CALL="-q ${DATADIR}/20.model -M -S"
RECOM=" -r 0.6 "
echo $PARTITION_CALL

echo "*** run serial -----------------------------------"
./raxmlLight $RECOM -m $MODEL -n ${NAME}_serial -s ${DATADIR}/${SET} -t ${DATADIR}/${TREE} $PARTITION_CALL
echo "*** run pthreads"
./raxmlLight-PTHREADS $RECOM -T $NUM_THREADS -m $MODEL -n ${NAME}_T${NUM_THREADS} -s ${DATADIR}/${SET} $PARTITION_CALL -t ${DATADIR}/${TREE} 
echo "*** run mpi"
mpirun.openmpi -np 4 ./raxmlLight-MPI $RECOM -m $MODEL -n ${NAME}_mpi -s ${DATADIR}/${SET} $PARTITION_CALL -t ${DATADIR}/${TREE} 

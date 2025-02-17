// Distributed under MIT licence. See https://github.com/QuEST-Kit/QuEST/blob/master/LICENCE.txt for details

/** @file
 * The core of the CPU backend functionality. The CPU/MPI implementations of the pure state functions in
 * ../QuEST_ops_pure.h are in QuEST_cpu_local.c and QuEST_cpu_distributed.c which mostly wrap the core
 * functions defined here. Some additional hardware-agnostic functions are defined here
 *
 * @author Ania Brown
 * @author Tyson Jones
 * @author Balint Koczor
 */

# include "QuEST.h"
# include "QuEST_internal.h"
# include "QuEST_precision.h"
# include "mt19937ar.h"

# include "QuEST_cpu_internal.h"

# include <math.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>
# include <assert.h>
# include <immintrin.h>

# ifdef _OPENMP
# include <omp.h>
# endif



/*
 * overloads for consistent API with GPU
 */

void copyStateToGPU(Qureg qureg) {
}

void copyStateFromGPU(Qureg qureg) {
}



/*
 * state vector and density matrix operations
 */

void densmatr_oneQubitDegradeOffDiagonal(Qureg qureg, const int targetQubit, qreal retain){
    const long long int numTasks = qureg.numAmpsPerChunk;
    long long int innerMask = 1LL << targetQubit;
    long long int outerMask = 1LL << (targetQubit + (qureg.numQubitsRepresented));

    long long int thisTask;
    long long int thisPattern;
    long long int totMask = innerMask|outerMask;

 # ifdef _OPENMP
# pragma omp parallel \
    shared   (innerMask,outerMask,totMask,qureg,retain) \
    private  (thisTask,thisPattern)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++){
            thisPattern = (thisTask+qureg.numAmpsPerChunk*qureg.chunkId)&totMask;
            if ((thisPattern==innerMask) || (thisPattern==outerMask)){
                // do dephase
                // the lines below will degrade the off-diagonal terms |..0..><..1..| and |..1..><..0..|
                qureg.stateVec.real[thisTask] = retain*qureg.stateVec.real[thisTask];
                qureg.stateVec.imag[thisTask] = retain*qureg.stateVec.imag[thisTask];
            }
        }
    }
}

void densmatr_mixDephasing(Qureg qureg, const int targetQubit, qreal dephase) {
    qreal retain=1-dephase;
    densmatr_oneQubitDegradeOffDiagonal(qureg, targetQubit, retain);
}

void densmatr_mixDampingDephase(Qureg qureg, const int targetQubit, qreal dephase) {
    qreal retain=sqrt(1-dephase);
    densmatr_oneQubitDegradeOffDiagonal(qureg, targetQubit, retain);
}

void densmatr_mixTwoQubitDephasing(Qureg qureg, const int qubit1, const int qubit2, qreal dephase) {
    qreal retain=1-dephase;

    const long long int numTasks = qureg.numAmpsPerChunk;
    long long int innerMaskQubit1 = 1LL << qubit1;
    long long int outerMaskQubit1 = 1LL << (qubit1 + (qureg.numQubitsRepresented));
    long long int innerMaskQubit2 = 1LL << qubit2;
    long long int outerMaskQubit2 = 1LL << (qubit2 + (qureg.numQubitsRepresented));
    long long int totMaskQubit1 = innerMaskQubit1|outerMaskQubit1;
    long long int totMaskQubit2 = innerMaskQubit2|outerMaskQubit2;

    long long int thisTask;
    long long int thisPatternQubit1, thisPatternQubit2;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (innerMaskQubit1,outerMaskQubit1,totMaskQubit1,innerMaskQubit2,outerMaskQubit2, \
                totMaskQubit2,qureg,retain) \
    private  (thisTask,thisPatternQubit1,thisPatternQubit2)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++){
            thisPatternQubit1 = (thisTask+qureg.numAmpsPerChunk*qureg.chunkId)&totMaskQubit1;
            thisPatternQubit2 = (thisTask+qureg.numAmpsPerChunk*qureg.chunkId)&totMaskQubit2;

            // any mismatch |...0...><...1...| etc
            if ( (thisPatternQubit1==innerMaskQubit1) || (thisPatternQubit1==outerMaskQubit1) ||
                    (thisPatternQubit2==innerMaskQubit2) || (thisPatternQubit2==outerMaskQubit2) ){
                // do dephase
                // the lines below will degrade the off-diagonal terms |..0..><..1..| and |..1..><..0..|
                qureg.stateVec.real[thisTask] = retain*qureg.stateVec.real[thisTask];
                qureg.stateVec.imag[thisTask] = retain*qureg.stateVec.imag[thisTask];
            }
        }
    }
}

void densmatr_mixDepolarisingLocal(Qureg qureg, const int targetQubit, qreal depolLevel) {
    qreal retain=1-depolLevel;

    const long long int numTasks = qureg.numAmpsPerChunk;
    long long int innerMask = 1LL << targetQubit;
    long long int outerMask = 1LL << (targetQubit + (qureg.numQubitsRepresented));
    long long int totMask = innerMask|outerMask;

    long long int thisTask;
    long long int partner;
    long long int thisPattern;

    qreal realAv, imagAv;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (innerMask,outerMask,totMask,qureg,retain,depolLevel) \
    private  (thisTask,partner,thisPattern,realAv,imagAv)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++){
            thisPattern = (thisTask+qureg.numAmpsPerChunk*qureg.chunkId)&totMask;
            if ((thisPattern==innerMask) || (thisPattern==outerMask)){
                // do dephase
                // the lines below will degrade the off-diagonal terms |..0..><..1..| and |..1..><..0..|
                qureg.stateVec.real[thisTask] = retain*qureg.stateVec.real[thisTask];
                qureg.stateVec.imag[thisTask] = retain*qureg.stateVec.imag[thisTask];
            } else {
                if ((thisTask&totMask)==0){ //this element relates to targetQubit in state 0
                    // do depolarise
                    partner = thisTask | totMask;
                    realAv =  (qureg.stateVec.real[thisTask] + qureg.stateVec.real[partner]) /2 ;
                    imagAv =  (qureg.stateVec.imag[thisTask] + qureg.stateVec.imag[partner]) /2 ;

                    qureg.stateVec.real[thisTask] = retain*qureg.stateVec.real[thisTask] + depolLevel*realAv;
                    qureg.stateVec.imag[thisTask] = retain*qureg.stateVec.imag[thisTask] + depolLevel*imagAv;

                    qureg.stateVec.real[partner] = retain*qureg.stateVec.real[partner] + depolLevel*realAv;
                    qureg.stateVec.imag[partner] = retain*qureg.stateVec.imag[partner] + depolLevel*imagAv;
                }
            }
        }
    }
}

void densmatr_mixDampingLocal(Qureg qureg, const int targetQubit, qreal damping) {
    qreal retain=1-damping;
    qreal dephase=sqrt(retain);

    const long long int numTasks = qureg.numAmpsPerChunk;
    long long int innerMask = 1LL << targetQubit;
    long long int outerMask = 1LL << (targetQubit + (qureg.numQubitsRepresented));
    long long int totMask = innerMask|outerMask;

    long long int thisTask;
    long long int partner;
    long long int thisPattern;

    //qreal realAv, imagAv;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (innerMask,outerMask,totMask,qureg,retain,damping,dephase) \
    private  (thisTask,partner,thisPattern)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++){
            thisPattern = (thisTask+qureg.numAmpsPerChunk*qureg.chunkId)&totMask;
            if ((thisPattern==innerMask) || (thisPattern==outerMask)){
                // do dephase
                // the lines below will degrade the off-diagonal terms |..0..><..1..| and |..1..><..0..|
                qureg.stateVec.real[thisTask] = dephase*qureg.stateVec.real[thisTask];
                qureg.stateVec.imag[thisTask] = dephase*qureg.stateVec.imag[thisTask];
            } else {
                if ((thisTask&totMask)==0){ //this element relates to targetQubit in state 0
                    // do depolarise
                    partner = thisTask | totMask;
                    //realAv =  (qureg.stateVec.real[thisTask] + qureg.stateVec.real[partner]) /2 ;
                    //imagAv =  (qureg.stateVec.imag[thisTask] + qureg.stateVec.imag[partner]) /2 ;

                    qureg.stateVec.real[thisTask] = qureg.stateVec.real[thisTask] + damping*qureg.stateVec.real[partner];
                    qureg.stateVec.imag[thisTask] = qureg.stateVec.imag[thisTask] + damping*qureg.stateVec.imag[partner];

                    qureg.stateVec.real[partner] = retain*qureg.stateVec.real[partner];
                    qureg.stateVec.imag[partner] = retain*qureg.stateVec.imag[partner];
                }
            }
        }
    }
}

void densmatr_mixDepolarisingDistributed(Qureg qureg, const int targetQubit, qreal depolLevel) {

    // first do dephase part.
    // TODO -- this might be more efficient to do at the same time as the depolarise if we move to
    // iterating over all elements in the state vector for the purpose of vectorisation
    // TODO -- if we keep this split, move this function to densmatr_mixDepolarising()
    densmatr_mixDephasing(qureg, targetQubit, depolLevel);

    long long int sizeInnerBlock, sizeInnerHalfBlock;
    long long int sizeOuterColumn, sizeOuterHalfColumn;
    long long int thisInnerBlock, // current block
         thisOuterColumn, // current column in density matrix
         thisIndex,    // current index in (density matrix representation) state vector
         thisIndexInOuterColumn,
         thisIndexInInnerBlock;
    int outerBit;

    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk>>1;

    // set dimensions
    sizeInnerHalfBlock = 1LL << targetQubit;
    sizeInnerBlock = 2LL * sizeInnerHalfBlock;
    sizeOuterColumn = 1LL << qureg.numQubitsRepresented;
    sizeOuterHalfColumn = sizeOuterColumn >> 1;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (sizeInnerBlock,sizeInnerHalfBlock,sizeOuterColumn,sizeOuterHalfColumn,qureg,depolLevel) \
    private  (thisTask,thisInnerBlock,thisOuterColumn,thisIndex,thisIndexInOuterColumn, \
                thisIndexInInnerBlock,outerBit)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        // thisTask iterates over half the elements in this process' chunk of the density matrix
        // treat this as iterating over all columns, then iterating over half the values
        // within one column.
        // If this function has been called, this process' chunk contains half an
        // outer block or less
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            // we want to process all columns in the density matrix,
            // updating the values for half of each column (one half of each inner block)
            thisOuterColumn = thisTask / sizeOuterHalfColumn;
            thisIndexInOuterColumn = thisTask&(sizeOuterHalfColumn-1); // thisTask % sizeOuterHalfColumn
            thisInnerBlock = thisIndexInOuterColumn/sizeInnerHalfBlock;
            // get index in state vector corresponding to upper inner block
            thisIndexInInnerBlock = thisTask&(sizeInnerHalfBlock-1); // thisTask % sizeInnerHalfBlock
            thisIndex = thisOuterColumn*sizeOuterColumn + thisInnerBlock*sizeInnerBlock
                + thisIndexInInnerBlock;
            // check if we are in the upper or lower half of an outer block
            outerBit = extractBit(targetQubit, (thisIndex+qureg.numAmpsPerChunk*qureg.chunkId)>>qureg.numQubitsRepresented);
            // if we are in the lower half of an outer block, shift to be in the lower half
            // of the inner block as well (we want to dephase |0><0| and |1><1| only)
            thisIndex += outerBit*(sizeInnerHalfBlock);

            // NOTE: at this point thisIndex should be the index of the element we want to
            // dephase in the chunk of the state vector on this process, in the
            // density matrix representation.
            // thisTask is the index of the pair element in pairStateVec


            // state[thisIndex] = (1-depolLevel)*state[thisIndex] + depolLevel*(state[thisIndex]
            //      + pair[thisTask])/2
            qureg.stateVec.real[thisIndex] = (1-depolLevel)*qureg.stateVec.real[thisIndex] +
                    depolLevel*(qureg.stateVec.real[thisIndex] + qureg.pairStateVec.real[thisTask])/2;

            qureg.stateVec.imag[thisIndex] = (1-depolLevel)*qureg.stateVec.imag[thisIndex] +
                    depolLevel*(qureg.stateVec.imag[thisIndex] + qureg.pairStateVec.imag[thisTask])/2;
        }
    }
}

void densmatr_mixDampingDistributed(Qureg qureg, const int targetQubit, qreal damping) {
    qreal retain=1-damping;
    qreal dephase=sqrt(1-damping);
    // first do dephase part.
    // TODO -- this might be more efficient to do at the same time as the depolarise if we move to
    // iterating over all elements in the state vector for the purpose of vectorisation
    // TODO -- if we keep this split, move this function to densmatr_mixDepolarising()
    densmatr_mixDampingDephase(qureg, targetQubit, dephase);

    long long int sizeInnerBlock, sizeInnerHalfBlock;
    long long int sizeOuterColumn, sizeOuterHalfColumn;
    long long int thisInnerBlock, // current block
         thisOuterColumn, // current column in density matrix
         thisIndex,    // current index in (density matrix representation) state vector
         thisIndexInOuterColumn,
         thisIndexInInnerBlock;
    int outerBit;
    int stateBit;

    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk>>1;

    // set dimensions
    sizeInnerHalfBlock = 1LL << targetQubit;
    sizeInnerBlock = 2LL * sizeInnerHalfBlock;
    sizeOuterColumn = 1LL << qureg.numQubitsRepresented;
    sizeOuterHalfColumn = sizeOuterColumn >> 1;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (sizeInnerBlock,sizeInnerHalfBlock,sizeOuterColumn,sizeOuterHalfColumn,qureg,damping, retain, dephase) \
    private  (thisTask,thisInnerBlock,thisOuterColumn,thisIndex,thisIndexInOuterColumn, \
                thisIndexInInnerBlock,outerBit, stateBit)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        // thisTask iterates over half the elements in this process' chunk of the density matrix
        // treat this as iterating over all columns, then iterating over half the values
        // within one column.
        // If this function has been called, this process' chunk contains half an
        // outer block or less
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            // we want to process all columns in the density matrix,
            // updating the values for half of each column (one half of each inner block)
            thisOuterColumn = thisTask / sizeOuterHalfColumn;
            thisIndexInOuterColumn = thisTask&(sizeOuterHalfColumn-1); // thisTask % sizeOuterHalfColumn
            thisInnerBlock = thisIndexInOuterColumn/sizeInnerHalfBlock;
            // get index in state vector corresponding to upper inner block
            thisIndexInInnerBlock = thisTask&(sizeInnerHalfBlock-1); // thisTask % sizeInnerHalfBlock
            thisIndex = thisOuterColumn*sizeOuterColumn + thisInnerBlock*sizeInnerBlock
                + thisIndexInInnerBlock;
            // check if we are in the upper or lower half of an outer block
            outerBit = extractBit(targetQubit, (thisIndex+qureg.numAmpsPerChunk*qureg.chunkId)>>qureg.numQubitsRepresented);
            // if we are in the lower half of an outer block, shift to be in the lower half
            // of the inner block as well (we want to dephase |0><0| and |1><1| only)
            thisIndex += outerBit*(sizeInnerHalfBlock);

            // NOTE: at this point thisIndex should be the index of the element we want to
            // dephase in the chunk of the state vector on this process, in the
            // density matrix representation.
            // thisTask is the index of the pair element in pairStateVec

            // Extract state bit, is 0 if thisIndex corresponds to a state with 0 in the target qubit
            // and is 1 if thisIndex corresponds to a state with 1 in the target qubit
            stateBit = extractBit(targetQubit, (thisIndex+qureg.numAmpsPerChunk*qureg.chunkId));

            // state[thisIndex] = (1-depolLevel)*state[thisIndex] + depolLevel*(state[thisIndex]
            //      + pair[thisTask])/2
            if(stateBit == 0){
                qureg.stateVec.real[thisIndex] = qureg.stateVec.real[thisIndex] +
                    damping*( qureg.pairStateVec.real[thisTask]);

                qureg.stateVec.imag[thisIndex] = qureg.stateVec.imag[thisIndex] +
                    damping*( qureg.pairStateVec.imag[thisTask]);
            } else{
                qureg.stateVec.real[thisIndex] = retain*qureg.stateVec.real[thisIndex];

                qureg.stateVec.imag[thisIndex] = retain*qureg.stateVec.imag[thisIndex];
            }
        }
    }
}

// @TODO
void densmatr_mixTwoQubitDepolarisingLocal(Qureg qureg, int qubit1, int qubit2, qreal delta, qreal gamma) {
    const long long int numTasks = qureg.numAmpsPerChunk;
    long long int innerMaskQubit1 = 1LL << qubit1;
    long long int outerMaskQubit1= 1LL << (qubit1 + qureg.numQubitsRepresented);
    long long int totMaskQubit1 = innerMaskQubit1 | outerMaskQubit1;
    long long int innerMaskQubit2 = 1LL << qubit2;
    long long int outerMaskQubit2 = 1LL << (qubit2 + qureg.numQubitsRepresented);
    long long int totMaskQubit2 = innerMaskQubit2 | outerMaskQubit2;

    long long int thisTask;
    long long int partner;
    long long int thisPatternQubit1, thisPatternQubit2;

    qreal real00, imag00;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (totMaskQubit1,totMaskQubit2,qureg,delta,gamma) \
    private  (thisTask,partner,thisPatternQubit1,thisPatternQubit2,real00,imag00)
# endif
    {

# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        //--------------------------------------- STEP ONE ---------------------
        for (thisTask=0; thisTask<numTasks; thisTask++){
            thisPatternQubit1 = (thisTask+qureg.numAmpsPerChunk*qureg.chunkId)&totMaskQubit1;
            thisPatternQubit2 = (thisTask+qureg.numAmpsPerChunk*qureg.chunkId)&totMaskQubit2;
            if ((thisPatternQubit1==0) && ((thisPatternQubit2==0)
                        || (thisPatternQubit2==totMaskQubit2))){
                //this element of form |...X...0...><...X...0...|  for X either 0 or 1.
                partner = thisTask | totMaskQubit1;
                real00 =  qureg.stateVec.real[thisTask];
                imag00 =  qureg.stateVec.imag[thisTask];

                qureg.stateVec.real[thisTask] = qureg.stateVec.real[thisTask]
                    + delta*qureg.stateVec.real[partner];
                qureg.stateVec.imag[thisTask] = qureg.stateVec.imag[thisTask]
                    + delta*qureg.stateVec.imag[partner];

                qureg.stateVec.real[partner] = qureg.stateVec.real[partner] + delta*real00;
                qureg.stateVec.imag[partner] = qureg.stateVec.imag[partner] + delta*imag00;

            }
        }
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        //--------------------------------------- STEP TWO ---------------------
        for (thisTask=0; thisTask<numTasks; thisTask++){
            thisPatternQubit1 = (thisTask+qureg.numAmpsPerChunk*qureg.chunkId)&totMaskQubit1;
            thisPatternQubit2 = (thisTask+qureg.numAmpsPerChunk*qureg.chunkId)&totMaskQubit2;
            if ((thisPatternQubit2==0) && ((thisPatternQubit1==0)
                        || (thisPatternQubit1==totMaskQubit1))){
                //this element of form |...0...X...><...0...X...|  for X either 0 or 1.
                partner = thisTask | totMaskQubit2;
                real00 =  qureg.stateVec.real[thisTask];
                imag00 =  qureg.stateVec.imag[thisTask];

                qureg.stateVec.real[thisTask] = qureg.stateVec.real[thisTask]
                    + delta*qureg.stateVec.real[partner];
                qureg.stateVec.imag[thisTask] = qureg.stateVec.imag[thisTask]
                    + delta*qureg.stateVec.imag[partner];

                qureg.stateVec.real[partner] = qureg.stateVec.real[partner] + delta*real00;
                qureg.stateVec.imag[partner] = qureg.stateVec.imag[partner] + delta*imag00;

            }
        }

# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        //--------------------------------------- STEP THREE ---------------------
        for (thisTask=0; thisTask<numTasks; thisTask++){
            thisPatternQubit1 = (thisTask+qureg.numAmpsPerChunk*qureg.chunkId)&totMaskQubit1;
            thisPatternQubit2 = (thisTask+qureg.numAmpsPerChunk*qureg.chunkId)&totMaskQubit2;
            if ((thisPatternQubit2==0) && ((thisPatternQubit1==0)
                        || (thisPatternQubit1==totMaskQubit1))){
                //this element of form |...0...X...><...0...X...|  for X either 0 or 1.
                partner = thisTask | totMaskQubit2;
                partner = partner ^ totMaskQubit1;
                real00 =  qureg.stateVec.real[thisTask];
                imag00 =  qureg.stateVec.imag[thisTask];

                qureg.stateVec.real[thisTask] = gamma * (qureg.stateVec.real[thisTask]
                        + delta*qureg.stateVec.real[partner]);
                qureg.stateVec.imag[thisTask] = gamma * (qureg.stateVec.imag[thisTask]
                        + delta*qureg.stateVec.imag[partner]);

                qureg.stateVec.real[partner] = gamma * (qureg.stateVec.real[partner]
                        + delta*real00);
                qureg.stateVec.imag[partner] = gamma * (qureg.stateVec.imag[partner]
                        + delta*imag00);

            }
        }
    }
}

void densmatr_mixTwoQubitDepolarisingLocalPart1(Qureg qureg, int qubit1, int qubit2, qreal delta) {
    const long long int numTasks = qureg.numAmpsPerChunk;
    long long int innerMaskQubit1 = 1LL << qubit1;
    long long int outerMaskQubit1= 1LL << (qubit1 + qureg.numQubitsRepresented);
    long long int totMaskQubit1 = innerMaskQubit1 | outerMaskQubit1;
    long long int innerMaskQubit2 = 1LL << qubit2;
    long long int outerMaskQubit2 = 1LL << (qubit2 + qureg.numQubitsRepresented);
    long long int totMaskQubit2 = innerMaskQubit2 | outerMaskQubit2;
    // correct for being in a particular chunk
    //totMaskQubit2 = totMaskQubit2&(qureg.numAmpsPerChunk-1); // totMaskQubit2 % numAmpsPerChunk


    long long int thisTask;
    long long int partner;
    long long int thisPatternQubit1, thisPatternQubit2;

    qreal real00, imag00;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (totMaskQubit1,totMaskQubit2,qureg,delta) \
    private  (thisTask,partner,thisPatternQubit1,thisPatternQubit2,real00,imag00)
# endif
    {

# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        //--------------------------------------- STEP ONE ---------------------
        for (thisTask=0; thisTask<numTasks; thisTask ++){
            thisPatternQubit1 = (thisTask+qureg.numAmpsPerChunk*qureg.chunkId)&totMaskQubit1;
            thisPatternQubit2 = (thisTask+qureg.numAmpsPerChunk*qureg.chunkId)&totMaskQubit2;
            if ((thisPatternQubit1==0) && ((thisPatternQubit2==0)
                        || (thisPatternQubit2==totMaskQubit2))){
                //this element of form |...X...0...><...X...0...|  for X either 0 or 1.
                partner = thisTask | totMaskQubit1;
                real00 =  qureg.stateVec.real[thisTask];
                imag00 =  qureg.stateVec.imag[thisTask];

                qureg.stateVec.real[thisTask] = qureg.stateVec.real[thisTask]
                    + delta*qureg.stateVec.real[partner];
                qureg.stateVec.imag[thisTask] = qureg.stateVec.imag[thisTask]
                    + delta*qureg.stateVec.imag[partner];

                qureg.stateVec.real[partner] = qureg.stateVec.real[partner] + delta*real00;
                qureg.stateVec.imag[partner] = qureg.stateVec.imag[partner] + delta*imag00;

            }
        }
    }
}

void densmatr_mixTwoQubitDepolarisingDistributed(Qureg qureg, const int targetQubit,
        const int qubit2, qreal delta, qreal gamma) {

    long long int sizeInnerBlockQ1, sizeInnerHalfBlockQ1;
    long long int sizeInnerBlockQ2, sizeInnerHalfBlockQ2, sizeInnerQuarterBlockQ2;
    long long int sizeOuterColumn, sizeOuterQuarterColumn;
    long long int thisInnerBlockQ2,
         thisOuterColumn, // current column in density matrix
         thisIndex,    // current index in (density matrix representation) state vector
         thisIndexInOuterColumn,
         thisIndexInInnerBlockQ1,
         thisIndexInInnerBlockQ2,
         thisInnerBlockQ1InInnerBlockQ2;
    int outerBitQ1, outerBitQ2;

    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk>>2;

    // set dimensions
    sizeInnerHalfBlockQ1 = 1LL << targetQubit;
    sizeInnerHalfBlockQ2 = 1LL << qubit2;
    sizeInnerQuarterBlockQ2 = sizeInnerHalfBlockQ2 >> 1;
    sizeInnerBlockQ2 = sizeInnerHalfBlockQ2 << 1;
    sizeInnerBlockQ1 = 2LL * sizeInnerHalfBlockQ1;
    sizeOuterColumn = 1LL << qureg.numQubitsRepresented;
    sizeOuterQuarterColumn = sizeOuterColumn >> 2;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (sizeInnerBlockQ1,sizeInnerHalfBlockQ1,sizeInnerBlockQ2,sizeInnerHalfBlockQ2,sizeInnerQuarterBlockQ2,\
                sizeOuterColumn,sizeOuterQuarterColumn,qureg,delta,gamma) \
    private  (thisTask,thisInnerBlockQ2,thisInnerBlockQ1InInnerBlockQ2, \
                thisOuterColumn,thisIndex,thisIndexInOuterColumn, \
                thisIndexInInnerBlockQ1,thisIndexInInnerBlockQ2,outerBitQ1,outerBitQ2)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        // thisTask iterates over half the elements in this process' chunk of the density matrix
        // treat this as iterating over all columns, then iterating over half the values
        // within one column.
        // If this function has been called, this process' chunk contains half an
        // outer block or less
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            // we want to process all columns in the density matrix,
            // updating the values for half of each column (one half of each inner block)
            thisOuterColumn = thisTask / sizeOuterQuarterColumn;
            // thisTask % sizeOuterQuarterColumn
            thisIndexInOuterColumn = thisTask&(sizeOuterQuarterColumn-1);
            thisInnerBlockQ2 = thisIndexInOuterColumn / sizeInnerQuarterBlockQ2;
            // thisTask % sizeInnerQuarterBlockQ2;
            thisIndexInInnerBlockQ2 = thisTask&(sizeInnerQuarterBlockQ2-1);
            thisInnerBlockQ1InInnerBlockQ2 = thisIndexInInnerBlockQ2 / sizeInnerHalfBlockQ1;
            // thisTask % sizeInnerHalfBlockQ1;
            thisIndexInInnerBlockQ1 = thisTask&(sizeInnerHalfBlockQ1-1);

            // get index in state vector corresponding to upper inner block
            thisIndex = thisOuterColumn*sizeOuterColumn + thisInnerBlockQ2*sizeInnerBlockQ2
                + thisInnerBlockQ1InInnerBlockQ2*sizeInnerBlockQ1 + thisIndexInInnerBlockQ1;

            // check if we are in the upper or lower half of an outer block for Q1
            outerBitQ1 = extractBit(targetQubit, (thisIndex+qureg.numAmpsPerChunk*qureg.chunkId)>>qureg.numQubitsRepresented);
            // if we are in the lower half of an outer block, shift to be in the lower half
            // of the inner block as well (we want to dephase |0><0| and |1><1| only)
            thisIndex += outerBitQ1*(sizeInnerHalfBlockQ1);

            // check if we are in the upper or lower half of an outer block for Q2
            outerBitQ2 = extractBit(qubit2, (thisIndex+qureg.numAmpsPerChunk*qureg.chunkId)>>qureg.numQubitsRepresented);
            // if we are in the lower half of an outer block, shift to be in the lower half
            // of the inner block as well (we want to dephase |0><0| and |1><1| only)
            thisIndex += outerBitQ2*(sizeInnerQuarterBlockQ2<<1);

            // NOTE: at this point thisIndex should be the index of the element we want to
            // dephase in the chunk of the state vector on this process, in the
            // density matrix representation.
            // thisTask is the index of the pair element in pairStateVec


            // state[thisIndex] = (1-depolLevel)*state[thisIndex] + depolLevel*(state[thisIndex]
            //      + pair[thisTask])/2
            // NOTE: must set gamma=1 if using this function for steps 1 or 2
            qureg.stateVec.real[thisIndex] = gamma*(qureg.stateVec.real[thisIndex] +
                    delta*qureg.pairStateVec.real[thisTask]);
            qureg.stateVec.imag[thisIndex] = gamma*(qureg.stateVec.imag[thisIndex] +
                    delta*qureg.pairStateVec.imag[thisTask]);
        }
    }
}

void densmatr_mixTwoQubitDepolarisingQ1LocalQ2DistributedPart3(Qureg qureg, const int targetQubit,
        const int qubit2, qreal delta, qreal gamma) {

    long long int sizeInnerBlockQ1, sizeInnerHalfBlockQ1;
    long long int sizeInnerBlockQ2, sizeInnerHalfBlockQ2, sizeInnerQuarterBlockQ2;
    long long int sizeOuterColumn, sizeOuterQuarterColumn;
    long long int thisInnerBlockQ2,
         thisOuterColumn, // current column in density matrix
         thisIndex,    // current index in (density matrix representation) state vector
         thisIndexInPairVector,
         thisIndexInOuterColumn,
         thisIndexInInnerBlockQ1,
         thisIndexInInnerBlockQ2,
         thisInnerBlockQ1InInnerBlockQ2;
    int outerBitQ1, outerBitQ2;

    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk>>2;

    // set dimensions
    sizeInnerHalfBlockQ1 = 1LL << targetQubit;
    sizeInnerHalfBlockQ2 = 1LL << qubit2;
    sizeInnerQuarterBlockQ2 = sizeInnerHalfBlockQ2 >> 1;
    sizeInnerBlockQ2 = sizeInnerHalfBlockQ2 << 1;
    sizeInnerBlockQ1 = 2LL * sizeInnerHalfBlockQ1;
    sizeOuterColumn = 1LL << qureg.numQubitsRepresented;
    sizeOuterQuarterColumn = sizeOuterColumn >> 2;

//# if 0
# ifdef _OPENMP
# pragma omp parallel \
    shared   (sizeInnerBlockQ1,sizeInnerHalfBlockQ1,sizeInnerBlockQ2,sizeInnerHalfBlockQ2,sizeInnerQuarterBlockQ2,\
                sizeOuterColumn,sizeOuterQuarterColumn,qureg,delta,gamma) \
    private  (thisTask,thisInnerBlockQ2,thisInnerBlockQ1InInnerBlockQ2, \
                thisOuterColumn,thisIndex,thisIndexInPairVector,thisIndexInOuterColumn, \
                thisIndexInInnerBlockQ1,thisIndexInInnerBlockQ2,outerBitQ1,outerBitQ2)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
//# endif
        // thisTask iterates over half the elements in this process' chunk of the density matrix
        // treat this as iterating over all columns, then iterating over half the values
        // within one column.
        // If this function has been called, this process' chunk contains half an
        // outer block or less
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            // we want to process all columns in the density matrix,
            // updating the values for half of each column (one half of each inner block)
            thisOuterColumn = thisTask / sizeOuterQuarterColumn;
            // thisTask % sizeOuterQuarterColumn
            thisIndexInOuterColumn = thisTask&(sizeOuterQuarterColumn-1);
            thisInnerBlockQ2 = thisIndexInOuterColumn / sizeInnerQuarterBlockQ2;
            // thisTask % sizeInnerQuarterBlockQ2;
            thisIndexInInnerBlockQ2 = thisTask&(sizeInnerQuarterBlockQ2-1);
            thisInnerBlockQ1InInnerBlockQ2 = thisIndexInInnerBlockQ2 / sizeInnerHalfBlockQ1;
            // thisTask % sizeInnerHalfBlockQ1;
            thisIndexInInnerBlockQ1 = thisTask&(sizeInnerHalfBlockQ1-1);

            // get index in state vector corresponding to upper inner block
            thisIndex = thisOuterColumn*sizeOuterColumn + thisInnerBlockQ2*sizeInnerBlockQ2
                + thisInnerBlockQ1InInnerBlockQ2*sizeInnerBlockQ1 + thisIndexInInnerBlockQ1;

            // check if we are in the upper or lower half of an outer block for Q1
            outerBitQ1 = extractBit(targetQubit, (thisIndex+qureg.numAmpsPerChunk*qureg.chunkId)>>qureg.numQubitsRepresented);
            // if we are in the lower half of an outer block, shift to be in the lower half
            // of the inner block as well (we want to dephase |0><0| and |1><1| only)
            thisIndex += outerBitQ1*(sizeInnerHalfBlockQ1);

            // For part 3 we need to match elements such that (my Q1 != pair Q1) AND (my Q2 != pair Q2)
            // Find correct index in pairStateVector
            thisIndexInPairVector = thisTask + (1-outerBitQ1)*sizeInnerHalfBlockQ1*sizeOuterQuarterColumn -
                outerBitQ1*sizeInnerHalfBlockQ1*sizeOuterQuarterColumn;

            // check if we are in the upper or lower half of an outer block for Q2
            outerBitQ2 = extractBit(qubit2, (thisIndex+qureg.numAmpsPerChunk*qureg.chunkId)>>qureg.numQubitsRepresented);
            // if we are in the lower half of an outer block, shift to be in the lower half
            // of the inner block as well (we want to dephase |0><0| and |1><1| only)
            thisIndex += outerBitQ2*(sizeInnerQuarterBlockQ2<<1);


            // NOTE: at this point thisIndex should be the index of the element we want to
            // dephase in the chunk of the state vector on this process, in the
            // density matrix representation.


            // state[thisIndex] = (1-depolLevel)*state[thisIndex] + depolLevel*(state[thisIndex]
            //      + pair[thisIndexInPairVector])/2
            qureg.stateVec.real[thisIndex] = gamma*(qureg.stateVec.real[thisIndex] +
                    delta*qureg.pairStateVec.real[thisIndexInPairVector]);

            qureg.stateVec.imag[thisIndex] = gamma*(qureg.stateVec.imag[thisIndex] +
                    delta*qureg.pairStateVec.imag[thisIndexInPairVector]);
        }
    }

}


/* Without nested parallelisation, only the outer most loops which call below are parallelised */
void zeroSomeAmps(Qureg qureg, long long int startInd, long long int numAmps) {
    long long int i;
# ifdef _OPENMP
# pragma omp parallel for schedule (static)
# endif
    for (i=startInd; i < startInd+numAmps; i++) {
        qureg.stateVec.real[i] = 0;
        qureg.stateVec.imag[i] = 0;
    }
}
void normaliseSomeAmps(Qureg qureg, qreal norm, long long int startInd, long long int numAmps) {
    long long int i;
# ifdef _OPENMP
# pragma omp parallel for schedule (static)
# endif
    for (i=startInd; i < startInd+numAmps; i++) {
        qureg.stateVec.real[i] /= norm;
        qureg.stateVec.imag[i] /= norm;
    }
}
void alternateNormZeroingSomeAmpBlocks(
    Qureg qureg, qreal norm, int normFirst,
    long long int startAmpInd, long long int numAmps, long long int blockSize
) {
    long long int numDubBlocks = numAmps / (2*blockSize);
    long long int blockStartInd;

    if (normFirst) {
        long long int dubBlockInd;
# ifdef _OPENMP
# pragma omp parallel for schedule (static) private (blockStartInd)
# endif
        for (dubBlockInd=0; dubBlockInd < numDubBlocks; dubBlockInd++) {
            blockStartInd = startAmpInd + dubBlockInd*2*blockSize;
            normaliseSomeAmps(qureg, norm, blockStartInd,             blockSize); // |0><0|
            zeroSomeAmps(     qureg,       blockStartInd + blockSize, blockSize);
        }
    } else {
        long long int dubBlockInd;
# ifdef _OPENMP
# pragma omp parallel for schedule (static) private (blockStartInd)
# endif
        for (dubBlockInd=0; dubBlockInd < numDubBlocks; dubBlockInd++) {
            blockStartInd = startAmpInd + dubBlockInd*2*blockSize;
            zeroSomeAmps(     qureg,       blockStartInd,             blockSize);
            normaliseSomeAmps(qureg, norm, blockStartInd + blockSize, blockSize); // |1><1|
        }
    }
}

/** Renorms (/prob) every | * outcome * >< * outcome * | state, setting all others to zero */
void densmatr_collapseToKnownProbOutcome(Qureg qureg, const int measureQubit, int outcome, qreal totalStateProb) {

	// only (global) indices (as bit sequence): '* outcome *(n+q) outcome *q are spared
    // where n = measureQubit, q = qureg.numQubitsRepresented.
    // We can thus step in blocks of 2^q+n, killing every second, and inside the others,
    //  stepping in sub-blocks of 2^q, killing every second.
    // When outcome=1, we offset the start of these blocks by their size.
    long long int innerBlockSize = (1LL << measureQubit);
    long long int outerBlockSize = (1LL << (measureQubit + qureg.numQubitsRepresented));

    // Because there are 2^a number of nodes(/chunks), each node will contain 2^b number of blocks,
    // or each block will span 2^c number of nodes. Similarly for the innerblocks.
    long long int locNumAmps = qureg.numAmpsPerChunk;
    long long int globalStartInd = qureg.chunkId * locNumAmps;
    int innerBit = extractBit(measureQubit, globalStartInd);
    int outerBit = extractBit(measureQubit + qureg.numQubitsRepresented, globalStartInd);

    // If this chunk's amps are entirely inside an outer block
    if (locNumAmps <= outerBlockSize) {

        // if this is an undesired outer block, kill all elems
        if (outerBit != outcome)
            return zeroSomeAmps(qureg, 0, qureg.numAmpsPerChunk);

        // othwerwise, if this is a desired outer block, and also entirely an inner block
        if (locNumAmps <= innerBlockSize) {

            // and that inner block is undesired, kill all elems
            if (innerBit != outcome)
                return zeroSomeAmps(qureg, 0, qureg.numAmpsPerChunk);
            // otherwise normalise all elems
            else
                return normaliseSomeAmps(qureg, totalStateProb, 0, qureg.numAmpsPerChunk);
        }

        // otherwise this is a desired outer block which contains 2^a inner blocks; kill/renorm every second inner block
        return alternateNormZeroingSomeAmpBlocks(
            qureg, totalStateProb, innerBit==outcome, 0, qureg.numAmpsPerChunk, innerBlockSize);
    }

    // Otherwise, this chunk's amps contain multiple outer blocks (and hence multiple inner blocks)
    long long int numOuterDoubleBlocks = locNumAmps / (2*outerBlockSize);
    long long int firstBlockInd;

    // alternate norming* and zeroing the outer blocks (with order based on the desired outcome)
    // These loops aren't parallelised, since they could have 1 or 2 iterations and will prevent
    // inner parallelisation
    if (outerBit == outcome) {

        for (long long int outerDubBlockInd = 0; outerDubBlockInd < numOuterDoubleBlocks; outerDubBlockInd++) {
            firstBlockInd = outerDubBlockInd*2*outerBlockSize;

            // *norm only the desired inner blocks in the desired outer block
            alternateNormZeroingSomeAmpBlocks(
                qureg, totalStateProb, innerBit==outcome,
                firstBlockInd, outerBlockSize, innerBlockSize);

            // zero the undesired outer block
            zeroSomeAmps(qureg, firstBlockInd + outerBlockSize, outerBlockSize);
        }

    } else {

        for (long long int outerDubBlockInd = 0; outerDubBlockInd < numOuterDoubleBlocks; outerDubBlockInd++) {
            firstBlockInd = outerDubBlockInd*2*outerBlockSize;

            // same thing but undesired outer blocks come first
            zeroSomeAmps(qureg, firstBlockInd, outerBlockSize);
            alternateNormZeroingSomeAmpBlocks(
                qureg, totalStateProb, innerBit==outcome,
                firstBlockInd + outerBlockSize, outerBlockSize, innerBlockSize);
        }
    }

}

qreal densmatr_calcPurityLocal(Qureg qureg) {

    /* sum of qureg^2, which is sum_i |qureg[i]|^2 */
    long long int index;
    long long int numAmps = qureg.numAmpsPerChunk;

    qreal trace = 0;
    qreal *vecRe = qureg.stateVec.real;
    qreal *vecIm = qureg.stateVec.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared    (vecRe, vecIm, numAmps) \
    private   (index) \
    reduction ( +:trace )
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule  (static)
# endif
        for (index=0LL; index<numAmps; index++) {

            trace += vecRe[index]*vecRe[index] + vecIm[index]*vecIm[index];
        }
    }

    return trace;
}

void densmatr_mixDensityMatrix(Qureg combineQureg, qreal otherProb, Qureg otherQureg) {

    /* corresponding amplitudes live on the same node (same dimensions) */

    // unpack vars for OpenMP
    qreal* combineVecRe = combineQureg.stateVec.real;
    qreal* combineVecIm = combineQureg.stateVec.imag;
    qreal* otherVecRe = otherQureg.stateVec.real;
    qreal* otherVecIm = otherQureg.stateVec.imag;
    long long int numAmps = combineQureg.numAmpsPerChunk;
    long long int index;

# ifdef _OPENMP
# pragma omp parallel \
    default (none) \
    shared  (combineVecRe,combineVecIm,otherVecRe,otherVecIm, otherProb, numAmps) \
    private (index)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule  (static)
# endif
        for (index=0; index < numAmps; index++) {
            combineVecRe[index] *= 1-otherProb;
            combineVecIm[index] *= 1-otherProb;

            combineVecRe[index] += otherProb * otherVecRe[index];
            combineVecIm[index] += otherProb * otherVecIm[index];
        }
    }
}

/** computes Tr((a-b) conjTrans(a-b)) = sum of abs values of (a-b) */
qreal densmatr_calcHilbertSchmidtDistanceSquaredLocal(Qureg a, Qureg b) {

    long long int index;
    long long int numAmps = a.numAmpsPerChunk;

    qreal *aRe = a.stateVec.real;
    qreal *aIm = a.stateVec.imag;
    qreal *bRe = b.stateVec.real;
    qreal *bIm = b.stateVec.imag;

    qreal trace = 0;
    qreal difRe, difIm;

# ifdef _OPENMP
# pragma omp parallel \
    shared    (aRe,aIm, bRe,bIm, numAmps) \
    private   (index,difRe,difIm) \
    reduction ( +:trace )
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule  (static)
# endif
        for (index=0LL; index<numAmps; index++) {

            difRe = aRe[index] - bRe[index];
            difIm = aIm[index] - bIm[index];
            trace += difRe*difRe + difIm*difIm;
        }
    }

    return trace;
}

/** computes Tr(conjTrans(a) b) = sum of (a_ij^* b_ij) */
qreal densmatr_calcInnerProductLocal(Qureg a, Qureg b) {

    long long int index;
    long long int numAmps = a.numAmpsPerChunk;

    qreal *aRe = a.stateVec.real;
    qreal *aIm = a.stateVec.imag;
    qreal *bRe = b.stateVec.real;
    qreal *bIm = b.stateVec.imag;

    qreal trace = 0;

# ifdef _OPENMP
# pragma omp parallel \
    shared    (aRe,aIm, bRe,bIm, numAmps) \
    private   (index) \
    reduction ( +:trace )
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule  (static)
# endif
        for (index=0LL; index<numAmps; index++) {
            trace += aRe[index]*bRe[index] + aIm[index]*bIm[index];
        }
    }

    return trace;
}


/** computes a few dens-columns-worth of (vec^*T) dens * vec */
qreal densmatr_calcFidelityLocal(Qureg qureg, Qureg pureState) {

    /* Here, elements of pureState are not accessed (instead grabbed from qureg.pair).
     * We only consult the attributes.
     *
     * qureg is a density matrix, and pureState is a statevector.
     * Every node contains as many columns of qureg as amps by pureState.
     * (each node contains an integer, exponent-of-2 number of whole columns of qureg)
     * Ergo, this node contains columns:
     * qureg.chunkID * pureState.numAmpsPerChunk  to
     * (qureg.chunkID + 1) * pureState.numAmpsPerChunk
     *
     * The first pureState.numAmpsTotal elements of qureg.pairStateVec are the
     * entire pureState state-vector
     */

    // unpack everything for OPENMP
    qreal* vecRe  = qureg.pairStateVec.real;
    qreal* vecIm  = qureg.pairStateVec.imag;
    qreal* densRe = qureg.stateVec.real;
    qreal* densIm = qureg.stateVec.imag;

    int row, col;
    int dim = (int) pureState.numAmpsTotal;
    int colsPerNode = (int) pureState.numAmpsPerChunk;
    // using only int, because density matrix has squared as many amps so its
    // iteration would be impossible if the pureStates numAmpsTotal didn't fit into int

    // starting GLOBAL column index of the qureg columns on this node
    int startCol = (int) (qureg.chunkId * pureState.numAmpsPerChunk);

    qreal densElemRe, densElemIm;
    qreal prefacRe, prefacIm;
    qreal rowSumRe, rowSumIm;
    qreal vecElemRe, vecElemIm;

    // quantity computed by this node
    qreal globalSumRe = 0;   // imag-component is assumed zero

# ifdef _OPENMP
# pragma omp parallel \
    shared    (vecRe,vecIm,densRe,densIm, dim,colsPerNode,startCol) \
    private   (row,col, prefacRe,prefacIm, rowSumRe,rowSumIm, densElemRe,densElemIm, vecElemRe,vecElemIm) \
    reduction ( +:globalSumRe )
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule  (static)
# endif
        // indices of my GLOBAL row
        for (row=0; row < dim; row++) {

            // single element of conj(pureState)
            prefacRe =   vecRe[row];
            prefacIm = - vecIm[row];

            rowSumRe = 0;
            rowSumIm = 0;

            // indices of my LOCAL column
            for (col=0; col < colsPerNode; col++) {

                // my local density element
                densElemRe = densRe[row + dim*col];
                densElemIm = densIm[row + dim*col];

                // state-vector element
                vecElemRe = vecRe[startCol + col];
                vecElemIm = vecIm[startCol + col];

                rowSumRe += densElemRe*vecElemRe - densElemIm*vecElemIm;
                rowSumIm += densElemRe*vecElemIm + densElemIm*vecElemRe;
            }

            globalSumRe += rowSumRe*prefacRe - rowSumIm*prefacIm;
        }
    }

    return globalSumRe;
}

Complex statevec_calcInnerProductLocal(Qureg bra, Qureg ket) {

    qreal innerProdReal = 0;
    qreal innerProdImag = 0;

    long long int index;
    long long int numAmps = bra.numAmpsPerChunk;
    qreal *braVecReal = bra.stateVec.real;
    qreal *braVecImag = bra.stateVec.imag;
    qreal *ketVecReal = ket.stateVec.real;
    qreal *ketVecImag = ket.stateVec.imag;

    qreal braRe, braIm, ketRe, ketIm;

# ifdef _OPENMP
# pragma omp parallel \
    shared    (braVecReal, braVecImag, ketVecReal, ketVecImag, numAmps) \
    private   (index, braRe, braIm, ketRe, ketIm) \
    reduction ( +:innerProdReal, innerProdImag )
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule  (static)
# endif
        for (index=0; index < numAmps; index++) {
            braRe = braVecReal[index];
            braIm = braVecImag[index];
            ketRe = ketVecReal[index];
            ketIm = ketVecImag[index];

            // conj(bra_i) * ket_i
            innerProdReal += braRe*ketRe + braIm*ketIm;
            innerProdImag += braRe*ketIm - braIm*ketRe;
        }
    }

    Complex innerProd;
    innerProd.real = innerProdReal;
    innerProd.imag = innerProdImag;
    return innerProd;
}



void densmatr_initClassicalState (Qureg qureg, long long int stateInd)
{
    // dimension of the state vector
    long long int densityNumElems = qureg.numAmpsPerChunk;

    // Can't use qureg->stateVec as a private OMP var
    qreal *densityReal = qureg.stateVec.real;
    qreal *densityImag = qureg.stateVec.imag;

    // initialise the state to all zeros
    long long int index;
# ifdef _OPENMP
# pragma omp parallel \
    shared   (densityNumElems, densityReal, densityImag) \
    private  (index)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (index=0; index<densityNumElems; index++) {
            densityReal[index] = 0.0;
            densityImag[index] = 0.0;
        }
    }

    // index of the single density matrix elem to set non-zero
    long long int densityDim = 1LL << qureg.numQubitsRepresented;
    long long int densityInd = (densityDim + 1)*stateInd;

    // give the specified classical state prob 1
    if (qureg.chunkId == densityInd / densityNumElems){
        densityReal[densityInd % densityNumElems] = 1.0;
        densityImag[densityInd % densityNumElems] = 0.0;
    }
}


void densmatr_initPlusState (Qureg qureg)
{
    // |+><+| = sum_i 1/sqrt(2^N) |i> 1/sqrt(2^N) <j| = sum_ij 1/2^N |i><j|
    long long int dim = (1LL << qureg.numQubitsRepresented);
    qreal probFactor = 1.0/((qreal) dim);

    // Can't use qureg->stateVec as a private OMP var
    qreal *densityReal = qureg.stateVec.real;
    qreal *densityImag = qureg.stateVec.imag;

    long long int index;
    long long int chunkSize = qureg.numAmpsPerChunk;
    // initialise the state to |+++..+++> = 1/normFactor {1, 1, 1, ...}
# ifdef _OPENMP
# pragma omp parallel \
    shared   (chunkSize, densityReal, densityImag, probFactor) \
    private  (index)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (index=0; index<chunkSize; index++) {
            densityReal[index] = probFactor;
            densityImag[index] = 0.0;
        }
    }
}

void densmatr_initPureStateLocal(Qureg targetQureg, Qureg copyQureg) {

    /* copyQureg amps aren't explicitly used - they're accessed through targetQureg.pair,
     * which contains the full pure statevector.
     * targetQureg has as many columns on node as copyQureg has amps
     */

    long long int colOffset = targetQureg.chunkId * copyQureg.numAmpsPerChunk;
    long long int colsPerNode = copyQureg.numAmpsPerChunk;
    long long int rowsPerNode = copyQureg.numAmpsTotal;

    // unpack vars for OpenMP
    qreal* vecRe = targetQureg.pairStateVec.real;
    qreal* vecIm = targetQureg.pairStateVec.imag;
    qreal* densRe = targetQureg.stateVec.real;
    qreal* densIm = targetQureg.stateVec.imag;

    long long int col, row, index;

    // a_i conj(a_j) |i><j|
    qreal ketRe, ketIm, braRe, braIm;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (colOffset, colsPerNode,rowsPerNode, vecRe,vecIm,densRe,densIm) \
    private  (col,row, ketRe,ketIm,braRe,braIm, index)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        // local column
        for (col=0; col < colsPerNode; col++) {

            // global row
            for (row=0; row < rowsPerNode; row++) {

                // get pure state amps
                ketRe = vecRe[row];
                ketIm = vecIm[row];
                braRe =   vecRe[col + colOffset];
                braIm = - vecIm[col + colOffset]; // minus for conjugation

                // update density matrix
                index = row + col*rowsPerNode; // local ind
                densRe[index] = ketRe*braRe - ketIm*braIm;
                densIm[index] = ketRe*braIm + ketIm*braRe;
            }
        }
    }
}

void statevec_setAmps(Qureg qureg, long long int startInd, qreal* reals, qreal* imags, long long int numAmps) {

    /* this is actually distributed, since the user's code runs on every node */

    // local start/end indices of the given amplitudes, assuming they fit in this chunk
    // these may be negative or above qureg.numAmpsPerChunk
    long long int localStartInd = startInd - qureg.chunkId*qureg.numAmpsPerChunk;
    long long int localEndInd = localStartInd + numAmps; // exclusive

    // add this to a local index to get corresponding elem in reals & imags
    long long int offset = qureg.chunkId*qureg.numAmpsPerChunk - startInd;

    // restrict these indices to fit into this chunk
    if (localStartInd < 0)
        localStartInd = 0;
    if (localEndInd > qureg.numAmpsPerChunk)
        localEndInd = qureg.numAmpsPerChunk;
    // they may now be out of order = no iterations

    // unpacking OpenMP vars
    long long int index;
    qreal* vecRe = qureg.stateVec.real;
    qreal* vecIm = qureg.stateVec.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (localStartInd,localEndInd, vecRe,vecIm, reals,imags, offset) \
    private  (index)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        // iterate these local inds - this might involve no iterations
        for (index=localStartInd; index < localEndInd; index++) {
            vecRe[index] = reals[index + offset];
            vecIm[index] = imags[index + offset];
        }
    }
}

void statevec_createQureg(Qureg *qureg, int numQubits, QuESTEnv env)
{
    long long int numAmps = 1LL << numQubits;
    long long int numAmpsPerRank = numAmps/env.numRanks;

    if (numAmpsPerRank > SIZE_MAX) {
        printf("Could not allocate memory (cannot fit numAmps into size_t)!");
        exit (EXIT_FAILURE);
    }

    size_t arrSize = (size_t) (numAmpsPerRank * sizeof(*(qureg->stateVec.real)));
    qureg->stateVec.real = malloc(arrSize);
    qureg->stateVec.imag = malloc(arrSize);
    if (env.numRanks>1){
        qureg->pairStateVec.real = malloc(arrSize);
        qureg->pairStateVec.imag = malloc(arrSize);
    }

    if ( (!(qureg->stateVec.real) || !(qureg->stateVec.imag))
            && numAmpsPerRank ) {
        printf("Could not allocate memory!");
        exit (EXIT_FAILURE);
    }

    if ( env.numRanks>1 && (!(qureg->pairStateVec.real) || !(qureg->pairStateVec.imag))
            && numAmpsPerRank ) {
        printf("Could not allocate memory!");
        exit (EXIT_FAILURE);
    }

    qureg->numQubitsInStateVec = numQubits;
    qureg->numAmpsTotal = numAmps;
    qureg->numAmpsPerChunk = numAmpsPerRank;
    qureg->chunkId = env.rank;
    qureg->numChunks = env.numRanks;
    qureg->isDensityMatrix = 0;
}

void statevec_destroyQureg(Qureg qureg, QuESTEnv env){

    qureg.numQubitsInStateVec = 0;
    qureg.numAmpsTotal = 0;
    qureg.numAmpsPerChunk = 0;

    free(qureg.stateVec.real);
    free(qureg.stateVec.imag);
    if (env.numRanks>1){
        free(qureg.pairStateVec.real);
        free(qureg.pairStateVec.imag);
    }
    qureg.stateVec.real = NULL;
    qureg.stateVec.imag = NULL;
    qureg.pairStateVec.real = NULL;
    qureg.pairStateVec.imag = NULL;
}

void statevec_reportStateToScreen(Qureg qureg, QuESTEnv env, int reportRank){
    long long int index;
    int rank;
    if (qureg.numQubitsInStateVec<=5){
        for (rank=0; rank<qureg.numChunks; rank++){
            if (qureg.chunkId==rank){
                if (reportRank) {
                    printf("Reporting state from rank %d [\n", qureg.chunkId);
                    printf("real, imag\n");
                } else if (rank==0) {
                    printf("Reporting state [\n");
                    printf("real, imag\n");
                }

                for(index=0; index<qureg.numAmpsPerChunk; index++){
                    //printf(REAL_STRING_FORMAT ", " REAL_STRING_FORMAT "\n", qureg.pairStateVec.real[index], qureg.pairStateVec.imag[index]);
                    printf(REAL_STRING_FORMAT ", " REAL_STRING_FORMAT "\n", qureg.stateVec.real[index], qureg.stateVec.imag[index]);
                }
                if (reportRank || rank==qureg.numChunks-1) printf("]\n");
            }
            syncQuESTEnv(env);
        }
    } else printf("Error: reportStateToScreen will not print output for systems of more than 5 qubits.\n");
}
void statevec_getEnvironmentString(QuESTEnv env, Qureg qureg, char str[200]){
    int numThreads=1;
# ifdef _OPENMP
    numThreads=omp_get_max_threads();
# endif
    sprintf(str, "%dqubits_CPU_%dranksx%dthreads", qureg.numQubitsInStateVec, env.numRanks, numThreads);
}

void statevec_initBlankState (Qureg qureg)
{
    long long int stateVecSize;
    long long int index;

    // dimension of the state vector
    stateVecSize = qureg.numAmpsPerChunk;

    // Can't use qureg->stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    // initialise the state-vector to all-zeroes
# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecSize, stateVecReal, stateVecImag) \
    private  (index)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (index=0; index<stateVecSize; index++) {
            stateVecReal[index] = 0.0;
            stateVecImag[index] = 0.0;
        }
    }
}

void statevec_initZeroState (Qureg qureg)
{
    statevec_initBlankState(qureg);
    if (qureg.chunkId==0){
        // zero state |0000..0000> has probability 1
        qureg.stateVec.real[0] = 1.0;
        qureg.stateVec.imag[0] = 0.0;
    }
}

void statevec_initPlusState (Qureg qureg)
{
    long long int chunkSize, stateVecSize;
    long long int index;

    // dimension of the state vector
    chunkSize = qureg.numAmpsPerChunk;
    stateVecSize = chunkSize*qureg.numChunks;
    qreal normFactor = 1.0/sqrt((qreal)stateVecSize);

    // Can't use qureg->stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    // initialise the state to |+++..+++> = 1/normFactor {1, 1, 1, ...}
# ifdef _OPENMP
# pragma omp parallel \
    shared   (chunkSize, stateVecReal, stateVecImag, normFactor) \
    private  (index)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (index=0; index<chunkSize; index++) {
            stateVecReal[index] = normFactor;
            stateVecImag[index] = 0.0;
        }
    }
}

void statevec_initClassicalState (Qureg qureg, long long int stateInd)
{
    long long int stateVecSize;
    long long int index;

    // dimension of the state vector
    stateVecSize = qureg.numAmpsPerChunk;

    // Can't use qureg->stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    // initialise the state to vector to all zeros
# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecSize, stateVecReal, stateVecImag) \
    private  (index)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (index=0; index<stateVecSize; index++) {
            stateVecReal[index] = 0.0;
            stateVecImag[index] = 0.0;
        }
    }

    // give the specified classical state prob 1
    if (qureg.chunkId == stateInd/stateVecSize){
        stateVecReal[stateInd % stateVecSize] = 1.0;
        stateVecImag[stateInd % stateVecSize] = 0.0;
    }
}

void statevec_cloneQureg(Qureg targetQureg, Qureg copyQureg) {

    // registers are equal sized, so nodes hold the same state-vector partitions
    long long int stateVecSize;
    long long int index;

    // dimension of the state vector
    stateVecSize = targetQureg.numAmpsPerChunk;

    // Can't use qureg->stateVec as a private OMP var
    qreal *targetStateVecReal = targetQureg.stateVec.real;
    qreal *targetStateVecImag = targetQureg.stateVec.imag;
    qreal *copyStateVecReal = copyQureg.stateVec.real;
    qreal *copyStateVecImag = copyQureg.stateVec.imag;

    // initialise the state to |0000..0000>
# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecSize, targetStateVecReal, targetStateVecImag, copyStateVecReal, copyStateVecImag) \
    private  (index)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (index=0; index<stateVecSize; index++) {
            targetStateVecReal[index] = copyStateVecReal[index];
            targetStateVecImag[index] = copyStateVecImag[index];
        }
    }
}

/**
 * Initialise the state vector of probability amplitudes such that one qubit is set to 'outcome' and all other qubits are in an equal superposition of zero and one.
 * @param[in,out] qureg object representing the set of qubits to be initialised
 * @param[in] qubitId id of qubit to set to state 'outcome'
 * @param[in] value of qubit 'qubitId'
 */
void statevec_initStateOfSingleQubit(Qureg *qureg, int qubitId, int outcome)
{
    long long int chunkSize, stateVecSize;
    long long int index;
    int bit;
    const long long int chunkId=qureg->chunkId;

    // dimension of the state vector
    chunkSize = qureg->numAmpsPerChunk;
    stateVecSize = chunkSize*qureg->numChunks;
    qreal normFactor = 1.0/sqrt((qreal)stateVecSize/2.0);

    // Can't use qureg->stateVec as a private OMP var
    qreal *stateVecReal = qureg->stateVec.real;
    qreal *stateVecImag = qureg->stateVec.imag;

    // initialise the state to |0000..0000>
# ifdef _OPENMP
# pragma omp parallel \
    shared   (chunkSize, stateVecReal, stateVecImag, normFactor, qubitId, outcome) \
    private  (index, bit)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (index=0; index<chunkSize; index++) {
            bit = extractBit(qubitId, index+chunkId*chunkSize);
            if (bit==outcome) {
                stateVecReal[index] = normFactor;
                stateVecImag[index] = 0.0;
            } else {
                stateVecReal[index] = 0.0;
                stateVecImag[index] = 0.0;
            }
        }
    }
}


/**
 * Initialise the state vector of probability amplitudes to an (unphysical) state with
 * each component of each probability amplitude a unique floating point value. For debugging processes
 * @param[in,out] qureg object representing the set of qubits to be initialised
 */
void statevec_initDebugState (Qureg qureg)
{
    long long int chunkSize;
    long long int index;
    long long int indexOffset;

    // dimension of the state vector
    chunkSize = qureg.numAmpsPerChunk;

    // Can't use qureg->stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    indexOffset = chunkSize * qureg.chunkId;

    // initialise the state to |0000..0000>
# ifdef _OPENMP
# pragma omp parallel \
    shared   (chunkSize, stateVecReal, stateVecImag, indexOffset) \
    private  (index)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (index=0; index<chunkSize; index++) {
            stateVecReal[index] = ((indexOffset + index)*2.0)/10.0;
            stateVecImag[index] = ((indexOffset + index)*2.0+1.0)/10.0;
        }
    }
}

// returns 1 if successful, else 0
int statevec_initStateFromSingleFile(Qureg *qureg, char filename[200], QuESTEnv env){
    long long int chunkSize, stateVecSize;
    long long int indexInChunk, totalIndex;

    chunkSize = qureg->numAmpsPerChunk;
    stateVecSize = chunkSize*qureg->numChunks;

    qreal *stateVecReal = qureg->stateVec.real;
    qreal *stateVecImag = qureg->stateVec.imag;

    FILE *fp;
    char line[200];

    for (int rank=0; rank<(qureg->numChunks); rank++){
        if (rank==qureg->chunkId){
            fp = fopen(filename, "r");

            // indicate file open failure
            if (fp == NULL)
                return 0;

            indexInChunk = 0; totalIndex = 0;
            while (fgets(line, sizeof(char)*200, fp) != NULL && totalIndex<stateVecSize){
                if (line[0]!='#'){
                    int chunkId = (int) (totalIndex/chunkSize);
                    if (chunkId==qureg->chunkId){
                        # if QuEST_PREC==1
                        sscanf(line, "%f, %f", &(stateVecReal[indexInChunk]),
                                &(stateVecImag[indexInChunk]));
                        # elif QuEST_PREC==2
                        sscanf(line, "%lf, %lf", &(stateVecReal[indexInChunk]),
                                &(stateVecImag[indexInChunk]));
                        # elif QuEST_PREC==4
                        sscanf(line, "%Lf, %Lf", &(stateVecReal[indexInChunk]),
                                &(stateVecImag[indexInChunk]));
                        # endif
                        indexInChunk += 1;
                    }
                    totalIndex += 1;
                }
            }
            fclose(fp);
        }
        syncQuESTEnv(env);
    }

    // indicate success
    return 1;
}

int statevec_compareStates(Qureg mq1, Qureg mq2, qreal precision){
    qreal diff;
    long long int chunkSize = mq1.numAmpsPerChunk;

    for (long long int i=0; i<chunkSize; i++){
        diff = absReal(mq1.stateVec.real[i] - mq2.stateVec.real[i]);
        if (diff>precision) return 0;
        diff = absReal(mq1.stateVec.imag[i] - mq2.stateVec.imag[i]);
        if (diff>precision) return 0;
    }
    return 1;
}

void statevec_compactUnitaryLocalSmall (Qureg qureg, const int targetQubit, Complex alpha, Complex beta)
{
    long long int indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateRealLo,stateImagUp,stateImagLo;

    const long long int sizeTask = (1LL << targetQubit);
    if(sizeTask >= 4){
        statevec_compactUnitaryLocalSIMD(qureg,targetQubit,alpha,beta);
        //printf("simd");
        return;
    }

    const long long int numTasks = (qureg.numAmpsPerChunk>>(1 + targetQubit)) ;
    long long thisTask;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;
    qreal alphaImag=alpha.imag, alphaReal=alpha.real;
    qreal betaImag=beta.imag, betaReal=beta.real;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecReal,stateVecImag, alphaReal,alphaImag, betaReal,betaImag) \
    private  (thisTask,indexUp,indexLo, stateRealUp,stateImagUp,stateRealLo,stateImagLo)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask = 0; thisTask < numTasks; ++thisTask)
        for (indexUp = thisTask * sizeTask * 2; indexUp < thisTask * sizeTask * 2 + sizeTask; ++ indexUp) {

            indexLo     = indexUp + sizeTask;

            // store current state vector values in temp variables
            stateRealUp = stateVecReal[indexUp];
            stateImagUp = stateVecImag[indexUp];

            stateRealLo = stateVecReal[indexLo];
            stateImagLo = stateVecImag[indexLo];

            // state[indexUp] = alpha * state[indexUp] - conj(beta)  * state[indexLo]
            stateVecReal[indexUp] = alphaReal*stateRealUp - alphaImag*stateImagUp
                - betaReal*stateRealLo - betaImag*stateImagLo;
            stateVecImag[indexUp] = alphaReal*stateImagUp + alphaImag*stateRealUp
                - betaReal*stateImagLo + betaImag*stateRealLo;

            // state[indexLo] = beta  * state[indexUp] + conj(alpha) * state[indexLo]
            stateVecReal[indexLo] = betaReal*stateRealUp - betaImag*stateImagUp
                + alphaReal*stateRealLo + alphaImag*stateImagLo;
            stateVecImag[indexLo] = betaReal*stateImagUp + betaImag*stateRealUp
                + alphaReal*stateImagLo - alphaImag*stateRealLo;
        }
    }

}

void statevec_compactUnitaryLocalSIMD (Qureg qureg, const int targetQubit, Complex alpha, Complex beta)
{
    long long int indexUp,indexLo;    // current index and corresponding index in lower half block


    const long long int numTasks = (qureg.numAmpsPerChunk>>(1 + targetQubit)) ;
    const long long int sizeTask = (1LL << targetQubit);
    long long thisTask;


    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;
    qreal alphaImag=alpha.imag, alphaReal=alpha.real;
    qreal betaImag=beta.imag, betaReal=beta.real;


    __m256d stateRealUpSIMD,stateImagUpSIMD,stateRealLoSIMD,stateImagLoSIMD;
    register const __m256d alphaRealSIMD = _mm256_set1_pd(alphaReal);
    register const __m256d alphaImagSIMD = _mm256_set1_pd(alphaImag);
    register const __m256d betaRealSIMD = _mm256_set1_pd(betaReal);
    register const __m256d betaImagSIMD = _mm256_set1_pd(betaImag);


# ifdef _OPENMP
    if(numTasks >= omp_get_num_threads()){
# pragma omp parallel \
    shared   (stateVecReal,stateVecImag, alphaRealSIMD,alphaImagSIMD, betaRealSIMD,betaImagSIMD) \
    private  (thisTask,indexUp,indexLo, stateRealUpSIMD,stateImagUpSIMD,stateRealLoSIMD,stateImagLoSIMD)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask = 0; thisTask < numTasks; ++thisTask)
        for (indexUp = thisTask * sizeTask * 2; indexUp < thisTask * sizeTask * 2 + sizeTask; indexUp+=4) {

            indexLo     = indexUp + sizeTask;

            // store current state vector values in temp variables
            stateRealUpSIMD = _mm256_loadu_pd(stateVecReal+indexUp);
            stateImagUpSIMD = _mm256_loadu_pd(stateVecImag+indexUp);
            stateRealLoSIMD = _mm256_loadu_pd(stateVecReal+indexLo);
            stateImagLoSIMD = _mm256_loadu_pd(stateVecImag+indexLo);


            // state[indexUp] = alpha * state[indexUp] - conj(beta)  * state[indexLo]
            //stateVecReal[indexUp] = alphaReal*stateRealUp - alphaImag*stateImagUp
            //    - betaReal*stateRealLo - betaImag*stateImagLo;
            __m256d res1 =  _mm256_mul_pd(alphaRealSIMD,stateRealUpSIMD);
            res1 = _mm256_sub_pd(res1,_mm256_mul_pd(alphaImagSIMD,stateImagUpSIMD));
            res1 = _mm256_sub_pd(res1,_mm256_mul_pd(betaRealSIMD,stateRealLoSIMD));
            res1 = _mm256_sub_pd(res1,_mm256_mul_pd(betaImagSIMD,stateImagLoSIMD));


            //stateVecImag[indexUp] = alphaReal*stateImagUp + alphaImag*stateRealUp
            //    - betaReal*stateImagLo + betaImag*stateRealLo;
            __m256d res2 =  _mm256_mul_pd(alphaRealSIMD,stateImagUpSIMD);
            res2 = _mm256_add_pd(res2,_mm256_mul_pd(alphaImagSIMD,stateRealUpSIMD));
            res2 = _mm256_sub_pd(res2,_mm256_mul_pd(betaRealSIMD,stateImagLoSIMD));
            res2 = _mm256_add_pd(res2,_mm256_mul_pd(betaImagSIMD,stateRealLoSIMD));

            // state[indexLo] = beta  * state[indexUp] + conj(alpha) * state[indexLo]
            //stateVecReal[indexLo] = betaReal*stateRealUp - betaImag*stateImagUp
            //    + alphaReal*stateRealLo + alphaImag*stateImagLo;
            __m256d res3 = _mm256_mul_pd(betaRealSIMD,stateRealUpSIMD);
            res3 = _mm256_sub_pd(res3,_mm256_mul_pd(betaImagSIMD,stateImagUpSIMD));
            res3 = _mm256_add_pd(res3,_mm256_mul_pd(alphaRealSIMD,stateRealLoSIMD));
            res3 = _mm256_add_pd(res3,_mm256_mul_pd(alphaImagSIMD,stateImagLoSIMD));

            //stateVecImag[indexLo] = betaReal*stateImagUp + betaImag*stateRealUp
            //    + alphaReal*stateImagLo - alphaImag*stateRealLo;
            __m256d res4 = _mm256_mul_pd(betaRealSIMD,stateImagUpSIMD);
            res4 = _mm256_add_pd(res4,_mm256_mul_pd(betaImagSIMD,stateRealUpSIMD));
            res4 = _mm256_add_pd(res4,_mm256_mul_pd(alphaRealSIMD,stateImagLoSIMD));
            res4 = _mm256_sub_pd(res4,_mm256_mul_pd(alphaImagSIMD,stateRealLoSIMD));

            _mm256_storeu_pd(stateVecReal+indexUp,res1);
            _mm256_storeu_pd(stateVecImag+indexUp,res2);
            _mm256_storeu_pd(stateVecReal+indexLo,res3);
            _mm256_storeu_pd(stateVecImag+indexLo,res4);
        }
    }
# ifdef _OPENMP
    } else { 
        for (thisTask = 0; thisTask < numTasks; ++thisTask)
# pragma omp parallel \
    shared   (thisTask,stateVecReal,stateVecImag, alphaRealSIMD,alphaImagSIMD, betaRealSIMD,betaImagSIMD) \
    private  (indexUp,indexLo, stateRealUpSIMD,stateImagUpSIMD,stateRealLoSIMD,stateImagLoSIMD)
    {
# pragma omp for schedule (static)
        for (indexUp = thisTask * sizeTask * 2; indexUp < thisTask * sizeTask * 2 + sizeTask; indexUp+=4) {

            indexLo     = indexUp + sizeTask;

            // store current state vector values in temp variables
            stateRealUpSIMD = _mm256_loadu_pd(stateVecReal+indexUp);
            stateImagUpSIMD = _mm256_loadu_pd(stateVecImag+indexUp);
            stateRealLoSIMD = _mm256_loadu_pd(stateVecReal+indexLo);
            stateImagLoSIMD = _mm256_loadu_pd(stateVecImag+indexLo);


            // state[indexUp] = alpha * state[indexUp] - conj(beta)  * state[indexLo]
            //stateVecReal[indexUp] = alphaReal*stateRealUp - alphaImag*stateImagUp
            //    - betaReal*stateRealLo - betaImag*stateImagLo;
            __m256d res1 =  _mm256_mul_pd(alphaRealSIMD,stateRealUpSIMD);
            res1 = _mm256_sub_pd(res1,_mm256_mul_pd(alphaImagSIMD,stateImagUpSIMD));
            res1 = _mm256_sub_pd(res1,_mm256_mul_pd(betaRealSIMD,stateRealLoSIMD));
            res1 = _mm256_sub_pd(res1,_mm256_mul_pd(betaImagSIMD,stateImagLoSIMD));


            //stateVecImag[indexUp] = alphaReal*stateImagUp + alphaImag*stateRealUp
            //    - betaReal*stateImagLo + betaImag*stateRealLo;
            __m256d res2 =  _mm256_mul_pd(alphaRealSIMD,stateImagUpSIMD);
            res2 = _mm256_add_pd(res2,_mm256_mul_pd(alphaImagSIMD,stateRealUpSIMD));
            res2 = _mm256_sub_pd(res2,_mm256_mul_pd(betaRealSIMD,stateImagLoSIMD));
            res2 = _mm256_add_pd(res2,_mm256_mul_pd(betaImagSIMD,stateRealLoSIMD));

            // state[indexLo] = beta  * state[indexUp] + conj(alpha) * state[indexLo]
            //stateVecReal[indexLo] = betaReal*stateRealUp - betaImag*stateImagUp
            //    + alphaReal*stateRealLo + alphaImag*stateImagLo;
            __m256d res3 = _mm256_mul_pd(betaRealSIMD,stateRealUpSIMD);
            res3 = _mm256_sub_pd(res3,_mm256_mul_pd(betaImagSIMD,stateImagUpSIMD));
            res3 = _mm256_add_pd(res3,_mm256_mul_pd(alphaRealSIMD,stateRealLoSIMD));
            res3 = _mm256_add_pd(res3,_mm256_mul_pd(alphaImagSIMD,stateImagLoSIMD));

            //stateVecImag[indexLo] = betaReal*stateImagUp + betaImag*stateRealUp
            //    + alphaReal*stateImagLo - alphaImag*stateRealLo;
            __m256d res4 = _mm256_mul_pd(betaRealSIMD,stateImagUpSIMD);
            res4 = _mm256_add_pd(res4,_mm256_mul_pd(betaImagSIMD,stateRealUpSIMD));
            res4 = _mm256_add_pd(res4,_mm256_mul_pd(alphaRealSIMD,stateImagLoSIMD));
            res4 = _mm256_sub_pd(res4,_mm256_mul_pd(alphaImagSIMD,stateRealLoSIMD));

            _mm256_storeu_pd(stateVecReal+indexUp,res1);
            _mm256_storeu_pd(stateVecImag+indexUp,res2);
            _mm256_storeu_pd(stateVecReal+indexLo,res3);
            _mm256_storeu_pd(stateVecImag+indexLo,res4);
        }
    }
    }
# endif


}

void statevec_compactUnitaryLocal (Qureg qureg, const int targetQubit, Complex alpha, Complex beta)
{
    long long int sizeBlock, sizeHalfBlock;
    long long int thisBlock, // current block
         indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateRealLo,stateImagUp,stateImagLo;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk>>1;

    // set dimensions
    sizeHalfBlock = 1LL << targetQubit;
    sizeBlock     = 2LL * sizeHalfBlock;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;
    qreal alphaImag=alpha.imag, alphaReal=alpha.real;
    qreal betaImag=beta.imag, betaReal=beta.real;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag, alphaReal,alphaImag, betaReal,betaImag) \
    private  (thisTask,thisBlock ,indexUp,indexLo, stateRealUp,stateImagUp,stateRealLo,stateImagLo)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {

            thisBlock   = thisTask / sizeHalfBlock;
            indexUp     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
            indexLo     = indexUp + sizeHalfBlock;

            // store current state vector values in temp variables
            stateRealUp = stateVecReal[indexUp];
            stateImagUp = stateVecImag[indexUp];

            stateRealLo = stateVecReal[indexLo];
            stateImagLo = stateVecImag[indexLo];

            // state[indexUp] = alpha * state[indexUp] - conj(beta)  * state[indexLo]
            stateVecReal[indexUp] = alphaReal*stateRealUp - alphaImag*stateImagUp
                - betaReal*stateRealLo - betaImag*stateImagLo;
            stateVecImag[indexUp] = alphaReal*stateImagUp + alphaImag*stateRealUp
                - betaReal*stateImagLo + betaImag*stateRealLo;

            // state[indexLo] = beta  * state[indexUp] + conj(alpha) * state[indexLo]
            stateVecReal[indexLo] = betaReal*stateRealUp - betaImag*stateImagUp
                + alphaReal*stateRealLo + alphaImag*stateImagLo;
            stateVecImag[indexLo] = betaReal*stateImagUp + betaImag*stateRealUp
                + alphaReal*stateImagLo - alphaImag*stateRealLo;
        }
    }

}

void statevec_multiControlledTwoQubitUnitaryLocal(Qureg qureg, long long int ctrlMask, const int q1, const int q2, ComplexMatrix4 u) {

    // can't use qureg.stateVec as a private OMP var
    qreal *reVec = qureg.stateVec.real;
    qreal *imVec = qureg.stateVec.imag;

    // the global (between all nodes) index of this node's start index
    long long int globalIndStart = qureg.chunkId*qureg.numAmpsPerChunk;

    long long int numTasks = qureg.numAmpsPerChunk >> 2; // each iteration updates 4 amplitudes
    long long int thisTask;
    long long int thisGlobalInd00;
    long long int ind00, ind01, ind10, ind11;
    qreal re00, re01, re10, re11;
    qreal im00, im01, im10, im11;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (reVec,imVec,globalIndStart,numTasks,ctrlMask,u) \
    private  (thisTask, thisGlobalInd00, ind00,ind01,ind10,ind11, re00,re01,re10,re11, im00,im01,im10,im11)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {

            // determine ind00 of |..0..0..>
            ind00 = insertTwoZeroBits(thisTask, q1, q2);

            // skip amplitude if controls aren't in 1 state (overloaded for speed)
            thisGlobalInd00 = ind00 + globalIndStart;
            if (ctrlMask && ((ctrlMask & thisGlobalInd00) != ctrlMask))
                continue;

            // inds of |..0..1..>, |..1..0..> and |..1..1..>
            ind01 = flipBit(ind00, q1);
            ind10 = flipBit(ind00, q2);
            ind11 = flipBit(ind01, q2);

            // extract statevec amplitudes
            re00 = reVec[ind00]; im00 = imVec[ind00];
            re01 = reVec[ind01]; im01 = imVec[ind01];
            re10 = reVec[ind10]; im10 = imVec[ind10];
            re11 = reVec[ind11]; im11 = imVec[ind11];

            // apply u * {amp00, amp01, amp10, amp11}
            reVec[ind00] =
                u.real[0][0]*re00 - u.imag[0][0]*im00 +
                u.real[0][1]*re01 - u.imag[0][1]*im01 +
                u.real[0][2]*re10 - u.imag[0][2]*im10 +
                u.real[0][3]*re11 - u.imag[0][3]*im11;
            imVec[ind00] =
                u.imag[0][0]*re00 + u.real[0][0]*im00 +
                u.imag[0][1]*re01 + u.real[0][1]*im01 +
                u.imag[0][2]*re10 + u.real[0][2]*im10 +
                u.imag[0][3]*re11 + u.real[0][3]*im11;

            reVec[ind01] =
                u.real[1][0]*re00 - u.imag[1][0]*im00 +
                u.real[1][1]*re01 - u.imag[1][1]*im01 +
                u.real[1][2]*re10 - u.imag[1][2]*im10 +
                u.real[1][3]*re11 - u.imag[1][3]*im11;
            imVec[ind01] =
                u.imag[1][0]*re00 + u.real[1][0]*im00 +
                u.imag[1][1]*re01 + u.real[1][1]*im01 +
                u.imag[1][2]*re10 + u.real[1][2]*im10 +
                u.imag[1][3]*re11 + u.real[1][3]*im11;

            reVec[ind10] =
                u.real[2][0]*re00 - u.imag[2][0]*im00 +
                u.real[2][1]*re01 - u.imag[2][1]*im01 +
                u.real[2][2]*re10 - u.imag[2][2]*im10 +
                u.real[2][3]*re11 - u.imag[2][3]*im11;
            imVec[ind10] =
                u.imag[2][0]*re00 + u.real[2][0]*im00 +
                u.imag[2][1]*re01 + u.real[2][1]*im01 +
                u.imag[2][2]*re10 + u.real[2][2]*im10 +
                u.imag[2][3]*re11 + u.real[2][3]*im11;

            reVec[ind11] =
                u.real[3][0]*re00 - u.imag[3][0]*im00 +
                u.real[3][1]*re01 - u.imag[3][1]*im01 +
                u.real[3][2]*re10 - u.imag[3][2]*im10 +
                u.real[3][3]*re11 - u.imag[3][3]*im11;
            imVec[ind11] =
                u.imag[3][0]*re00 + u.real[3][0]*im00 +
                u.imag[3][1]*re01 + u.real[3][1]*im01 +
                u.imag[3][2]*re10 + u.real[3][2]*im10 +
                u.imag[3][3]*re11 + u.real[3][3]*im11;
        }
    }
}

int qsortComp(const void *a, const void *b) {
    return *(int*)a - *(int*)b;
}

void statevec_multiControlledMultiQubitUnitaryLocal(Qureg qureg, long long int ctrlMask, int* targs, const int numTargs, ComplexMatrixN u)
{
    // can't use qureg.stateVec as a private OMP var
    qreal *reVec = qureg.stateVec.real;
    qreal *imVec = qureg.stateVec.imag;

    long long int numTasks = qureg.numAmpsPerChunk >> numTargs;  // kernel called on every 1 in 2^numTargs amplitudes
    long long int numTargAmps = 1 << u.numQubits;  // num amps to be modified by each task

    // the global (between all nodes) index of this node's start index
    long long int globalIndStart = qureg.chunkId*qureg.numAmpsPerChunk;

    long long int thisTask;
    long long int thisInd00; // this thread's index of |..0..0..> (target qubits = 0)
    long long int thisGlobalInd00; // the global (between all nodes) index of this thread's |..0..0..> state
    long long int ind;   // each thread's iteration of amplitudes to modify
    int i, t, r, c;  // each thread's iteration of amps and targets
    qreal reElem, imElem;  // each thread's iteration of u elements

    // each thread/task will record and modify numTargAmps amplitudes, privately
    // (of course, tasks eliminated by the ctrlMask won't edit their allocation)
    long long int ampInds[numTargAmps];
    qreal reAmps[numTargAmps];
    qreal imAmps[numTargAmps];

    // we need a sorted targets list to find thisInd00 for each task.
    // we can't modify targets, because the user-ordering of targets matters in u
    int sortedTargs[numTargs];
    for (int t=0; t < numTargs; t++)
        sortedTargs[t] = targs[t];
    qsort(sortedTargs, numTargs, sizeof(int), qsortComp);

# ifdef _OPENMP
# pragma omp parallel \
    shared   (reVec,imVec, numTasks,numTargAmps,globalIndStart, ctrlMask,targs,sortedTargs,u) \
    private  (thisTask,thisInd00,thisGlobalInd00,ind,i,t,r,c,reElem,imElem,  ampInds,reAmps,imAmps)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {

            // find this task's start index (where all targs are 0)
            thisInd00 = thisTask;
            for (t=0; t < numTargs; t++)
                thisInd00 = insertZeroBit(thisInd00, sortedTargs[t]);

            // this task only modifies amplitudes if control qubits are 1 for this state
            thisGlobalInd00 = thisInd00 + globalIndStart;
            if (ctrlMask && ((ctrlMask & thisGlobalInd00) != ctrlMask))
                continue;

            // determine the indices and record values of this tasks's target amps
            for (i=0; i < numTargAmps; i++) {

                // get statevec index of current target qubit assignment
                ind = thisInd00;
                for (t=0; t < numTargs; t++)
                    if (extractBit(t, i))
                        ind = flipBit(ind, targs[t]);

                // update this tasks's private arrays
                ampInds[i] = ind;
                reAmps [i] = reVec[ind];
                imAmps [i] = imVec[ind];
            }

            // modify this tasks's target amplitudes
            for (r=0; r < numTargAmps; r++) {
                ind = ampInds[r];
                reVec[ind] = 0;
                imVec[ind] = 0;

                for (c=0; c < numTargAmps; c++) {
                    reElem = u.real[r][c];
                    imElem = u.imag[r][c];
                    reVec[ind] += reAmps[c]*reElem - imAmps[c]*imElem;
                    imVec[ind] += reAmps[c]*imElem + imAmps[c]*reElem;
                }
            }
        }
    }
}

void statevec_unitaryLocal(Qureg qureg, const int targetQubit, ComplexMatrix2 u)
{
    long long int sizeBlock, sizeHalfBlock;
    long long int thisBlock, // current block
         indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateRealLo,stateImagUp,stateImagLo;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk>>1;

    // set dimensions
    sizeHalfBlock = 1LL << targetQubit;
    sizeBlock     = 2LL * sizeHalfBlock;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag, u) \
    private  (thisTask,thisBlock ,indexUp,indexLo, stateRealUp,stateImagUp,stateRealLo,stateImagLo)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {

            thisBlock   = thisTask / sizeHalfBlock;
            indexUp     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
            indexLo     = indexUp + sizeHalfBlock;

            // store current state vector values in temp variables
            stateRealUp = stateVecReal[indexUp];
            stateImagUp = stateVecImag[indexUp];

            stateRealLo = stateVecReal[indexLo];
            stateImagLo = stateVecImag[indexLo];


            // state[indexUp] = u00 * state[indexUp] + u01 * state[indexLo]
            stateVecReal[indexUp] = u.real[0][0]*stateRealUp - u.imag[0][0]*stateImagUp
                + u.real[0][1]*stateRealLo - u.imag[0][1]*stateImagLo;
            stateVecImag[indexUp] = u.real[0][0]*stateImagUp + u.imag[0][0]*stateRealUp
                + u.real[0][1]*stateImagLo + u.imag[0][1]*stateRealLo;

            // state[indexLo] = u10  * state[indexUp] + u11 * state[indexLo]
            stateVecReal[indexLo] = u.real[1][0]*stateRealUp  - u.imag[1][0]*stateImagUp
                + u.real[1][1]*stateRealLo  -  u.imag[1][1]*stateImagLo;
            stateVecImag[indexLo] = u.real[1][0]*stateImagUp + u.imag[1][0]*stateRealUp
                + u.real[1][1]*stateImagLo + u.imag[1][1]*stateRealLo;

        }
    }
}

/** Rotate a single qubit in the state vector of probability amplitudes,
 * given two complex numbers alpha and beta,
 * and a subset of the state vector with upper and lower block values stored seperately.
 *
 *  @param[in,out] qureg object representing the set of qubits
 *  @param[in] rot1 rotation angle
 *  @param[in] rot2 rotation angle
 *  @param[in] stateVecUp probability amplitudes in upper half of a block
 *  @param[in] stateVecLo probability amplitudes in lower half of a block
 *  @param[out] stateVecOut array section to update (will correspond to either the lower or upper half of a block)
 */
void statevec_compactUnitaryDistributed (Qureg qureg,
        Complex rot1, Complex rot2,
        ComplexArray stateVecUp,
        ComplexArray stateVecLo,
        ComplexArray stateVecOut)
{

    qreal   stateRealUp,stateRealLo,stateImagUp,stateImagLo;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk;
    if(numTasks >= 4){
        statevec_compactUnitaryDistributedSIMD(qureg,rot1,rot2,stateVecUp,stateVecLo,stateVecOut);
        return ;
    }

    qreal rot1Real=rot1.real, rot1Imag=rot1.imag;
    qreal rot2Real=rot2.real, rot2Imag=rot2.imag;
    qreal *stateVecRealUp=stateVecUp.real, *stateVecImagUp=stateVecUp.imag;
    qreal *stateVecRealLo=stateVecLo.real, *stateVecImagLo=stateVecLo.imag;
    qreal *stateVecRealOut=stateVecOut.real, *stateVecImagOut=stateVecOut.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecRealUp,stateVecImagUp,stateVecRealLo,stateVecImagLo,stateVecRealOut,stateVecImagOut, \
            rot1Real,rot1Imag, rot2Real,rot2Imag) \
    private  (thisTask,stateRealUp,stateImagUp,stateRealLo,stateImagLo)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            // store current state vector values in temp variables
            stateRealUp = stateVecRealUp[thisTask];
            stateImagUp = stateVecImagUp[thisTask];

            stateRealLo = stateVecRealLo[thisTask];
            stateImagLo = stateVecImagLo[thisTask];

            // state[indexUp] = alpha * state[indexUp] - conj(beta)  * state[indexLo]
            stateVecRealOut[thisTask] = rot1Real*stateRealUp - rot1Imag*stateImagUp + rot2Real*stateRealLo + rot2Imag*stateImagLo;
            stateVecImagOut[thisTask] = rot1Real*stateImagUp + rot1Imag*stateRealUp + rot2Real*stateImagLo - rot2Imag*stateRealLo;
        }
    }
}

void statevec_compactUnitaryDistributedSIMD (Qureg qureg,
        Complex rot1, Complex rot2,
        ComplexArray stateVecUp,
        ComplexArray stateVecLo,
        ComplexArray stateVecOut)
{

    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk;

    qreal rot1Real=rot1.real, rot1Imag=rot1.imag;
    qreal rot2Real=rot2.real, rot2Imag=rot2.imag;
    qreal *stateVecRealUp=stateVecUp.real, *stateVecImagUp=stateVecUp.imag;
    qreal *stateVecRealLo=stateVecLo.real, *stateVecImagLo=stateVecLo.imag;
    qreal *stateVecRealOut=stateVecOut.real, *stateVecImagOut=stateVecOut.imag;

    __m256d stateRealUpSIMD,stateRealLoSIMD,stateImagUpSIMD,stateImagLoSIMD;
    register const __m256d rot1RealSIMD = _mm256_set1_pd(rot1Real);
    register const __m256d rot1ImagSIMD = _mm256_set1_pd(rot1Imag);
    register const __m256d rot2RealSIMD = _mm256_set1_pd(rot2Real);
    register const __m256d rot2ImagSIMD = _mm256_set1_pd(rot2Imag);

# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecRealUp,stateVecImagUp,stateVecRealLo,stateVecImagLo,stateVecRealOut,stateVecImagOut, \
            rot1RealSIMD,rot1ImagSIMD, rot2RealSIMD,rot2ImagSIMD) \
    private  (thisTask,stateRealUpSIMD,stateImagUpSIMD,stateRealLoSIMD,stateImagLoSIMD)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask+=4) {
            // store current state vector values in temp variables
            stateRealUpSIMD = _mm256_loadu_pd(stateVecRealUp+thisTask);
            stateImagUpSIMD = _mm256_loadu_pd(stateVecImagUp+thisTask);

            stateRealLoSIMD = _mm256_loadu_pd(stateVecRealLo+thisTask);
            stateImagLoSIMD = _mm256_loadu_pd(stateVecImagLo+thisTask);

            // state[indexUp] = alpha * state[indexUp] - conj(beta)  * state[indexLo]
            //stateVecRealOut[thisTask] = rot1Real*stateRealUp - rot1Imag*stateImagUp + rot2Real*stateRealLo + rot2Imag*stateImagLo;
            //stateVecImagOut[thisTask] = rot1Real*stateImagUp + rot1Imag*stateRealUp + rot2Real*stateImagLo - rot2Imag*stateRealLo;
            _mm256_storeu_pd(stateVecRealOut+thisTask, _mm256_add_pd( \
                                _mm256_sub_pd(_mm256_mul_pd(rot1RealSIMD,stateRealUpSIMD),_mm256_mul_pd(rot1ImagSIMD,stateImagUpSIMD)), \
                                _mm256_add_pd(_mm256_mul_pd(rot2RealSIMD,stateRealLoSIMD),_mm256_mul_pd(rot2ImagSIMD,stateImagLoSIMD))));

            _mm256_storeu_pd(stateVecImagOut+thisTask, _mm256_add_pd( \
                                _mm256_add_pd(_mm256_mul_pd(rot1RealSIMD,stateImagUpSIMD),_mm256_mul_pd(rot1ImagSIMD,stateRealUpSIMD)), \
                                _mm256_sub_pd(_mm256_mul_pd(rot2RealSIMD,stateImagLoSIMD),_mm256_mul_pd(rot2ImagSIMD,stateRealLoSIMD))));
        }
    }
}

/** Apply a unitary operation to a single qubit
 *  given a subset of the state vector with upper and lower block values
 * stored seperately.
 *
 *  @remarks Qubits are zero-based and the first qubit is the rightmost
 *
 *  @param[in,out] qureg object representing the set of qubits
 *  @param[in] u unitary matrix to apply
 *  @param[in] stateVecUp probability amplitudes in upper half of a block
 *  @param[in] stateVecLo probability amplitudes in lower half of a block
 *  @param[out] stateVecOut array section to update (will correspond to either the lower or upper half of a block)
 */
void statevec_unitaryDistributed (Qureg qureg,
        Complex rot1, Complex rot2,
        ComplexArray stateVecUp,
        ComplexArray stateVecLo,
        ComplexArray stateVecOut)
{

    qreal   stateRealUp,stateRealLo,stateImagUp,stateImagLo;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk;

    qreal rot1Real=rot1.real, rot1Imag=rot1.imag;
    qreal rot2Real=rot2.real, rot2Imag=rot2.imag;
    qreal *stateVecRealUp=stateVecUp.real, *stateVecImagUp=stateVecUp.imag;
    qreal *stateVecRealLo=stateVecLo.real, *stateVecImagLo=stateVecLo.imag;
    qreal *stateVecRealOut=stateVecOut.real, *stateVecImagOut=stateVecOut.imag;


# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecRealUp,stateVecImagUp,stateVecRealLo,stateVecImagLo,stateVecRealOut,stateVecImagOut, \
            rot1Real, rot1Imag, rot2Real, rot2Imag) \
    private  (thisTask,stateRealUp,stateImagUp,stateRealLo,stateImagLo)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            // store current state vector values in temp variables
            stateRealUp = stateVecRealUp[thisTask];
            stateImagUp = stateVecImagUp[thisTask];

            stateRealLo = stateVecRealLo[thisTask];
            stateImagLo = stateVecImagLo[thisTask];

            stateVecRealOut[thisTask] = rot1Real*stateRealUp - rot1Imag*stateImagUp
                + rot2Real*stateRealLo - rot2Imag*stateImagLo;
            stateVecImagOut[thisTask] = rot1Real*stateImagUp + rot1Imag*stateRealUp
                + rot2Real*stateImagLo + rot2Imag*stateRealLo;
        }
    }
}


void statevec_controlledCompactUnitaryLocalAllSmallSIMD (Qureg qureg, const int controlQubit, const int targetQubit,
        Complex alpha, Complex beta)
{
    long long int indexUp,indexLo;    // current index and corresponding index in lower half block

    const long long int numTasks = (qureg.numAmpsPerChunk>>(1 + targetQubit)) ;
    const long long int sizeTask = (1LL << targetQubit);
    long long thisTask;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;
    qreal alphaImag=alpha.imag, alphaReal=alpha.real;
    qreal betaImag=beta.imag, betaReal=beta.real;

    __m256d stateRealUpSIMD,stateImagUpSIMD,stateRealLoSIMD,stateImagLoSIMD;
    register const __m256d alphaRealSIMD = _mm256_set1_pd(alphaReal);
    register const __m256d alphaImagSIMD = _mm256_set1_pd(alphaImag);
    register const __m256d betaRealSIMD = _mm256_set1_pd(betaReal);
    register const __m256d betaImagSIMD = _mm256_set1_pd(betaImag); 

# ifdef _OPENMP
    if(numTasks >= omp_get_num_threads()){
# pragma omp parallel \
    shared   (stateVecReal,stateVecImag,alphaRealSIMD,alphaImagSIMD, betaRealSIMD,betaImagSIMD) \
    private  (thisTask,indexUp,indexLo,stateRealUpSIMD,stateImagUpSIMD,stateRealLoSIMD,stateImagLoSIMD)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask = 0; thisTask < numTasks; ++thisTask)
        for (indexUp = thisTask * sizeTask * 2; indexUp < thisTask * sizeTask * 2 + sizeTask; indexUp+=4) {

                indexLo     = indexUp + sizeTask;

                    // controlBit = extractBit (controlQubit, indexUp+chunkId*chunkSize);
                    // if (controlBit){
                    // store current state vector values in temp variables
                    stateRealUpSIMD = _mm256_loadu_pd(stateVecReal+indexUp);
                    stateImagUpSIMD = _mm256_loadu_pd(stateVecImag+indexUp);
                    stateRealLoSIMD = _mm256_loadu_pd(stateVecReal+indexLo);
                    stateImagLoSIMD = _mm256_loadu_pd(stateVecImag+indexLo);

                    // state[indexUp] = alpha * state[indexUp] - conj(beta)  * state[indexLo]
                    //stateVecReal[indexUp] = alphaReal*stateRealUp - alphaImag*stateImagUp
                    //    - betaReal*stateRealLo - betaImag*stateImagLo;
                    __m256d res1 =  _mm256_mul_pd(alphaRealSIMD,stateRealUpSIMD);
                    res1 = _mm256_sub_pd(res1,_mm256_mul_pd(alphaImagSIMD,stateImagUpSIMD));
                    res1 = _mm256_sub_pd(res1,_mm256_mul_pd(betaRealSIMD,stateRealLoSIMD));
                    res1 = _mm256_sub_pd(res1,_mm256_mul_pd(betaImagSIMD,stateImagLoSIMD));


                    //stateVecImag[indexUp] = alphaReal*stateImagUp + alphaImag*stateRealUp
                    //    - betaReal*stateImagLo + betaImag*stateRealLo;
                    __m256d res2 =  _mm256_mul_pd(alphaRealSIMD,stateImagUpSIMD);
                    res2 = _mm256_add_pd(res2,_mm256_mul_pd(alphaImagSIMD,stateRealUpSIMD));
                    res2 = _mm256_sub_pd(res2,_mm256_mul_pd(betaRealSIMD,stateImagLoSIMD));
                    res2 = _mm256_add_pd(res2,_mm256_mul_pd(betaImagSIMD,stateRealLoSIMD));

                    // state[indexLo] = beta  * state[indexUp] + conj(alpha) * state[indexLo]
                    //stateVecReal[indexLo] = betaReal*stateRealUp - betaImag*stateImagUp
                    //    + alphaReal*stateRealLo + alphaImag*stateImagLo;
                    __m256d res3 = _mm256_mul_pd(betaRealSIMD,stateRealUpSIMD);
                    res3 = _mm256_sub_pd(res3,_mm256_mul_pd(betaImagSIMD,stateImagUpSIMD));
                    res3 = _mm256_add_pd(res3,_mm256_mul_pd(alphaRealSIMD,stateRealLoSIMD));
                    res3 = _mm256_add_pd(res3,_mm256_mul_pd(alphaImagSIMD,stateImagLoSIMD));

                    //stateVecImag[indexLo] = betaReal*stateImagUp + betaImag*stateRealUp
                    //    + alphaReal*stateImagLo - alphaImag*stateRealLo;
                    __m256d res4 = _mm256_mul_pd(betaRealSIMD,stateImagUpSIMD);
                    res4 = _mm256_add_pd(res4,_mm256_mul_pd(betaImagSIMD,stateRealUpSIMD));
                    res4 = _mm256_add_pd(res4,_mm256_mul_pd(alphaRealSIMD,stateImagLoSIMD));
                    res4 = _mm256_sub_pd(res4,_mm256_mul_pd(alphaImagSIMD,stateRealLoSIMD));

                    _mm256_storeu_pd(stateVecReal+indexUp,res1);
                    _mm256_storeu_pd(stateVecImag+indexUp,res2);
                    _mm256_storeu_pd(stateVecReal+indexLo,res3);
                    _mm256_storeu_pd(stateVecImag+indexLo,res4);
                    // }        }
    }
    }
# ifdef _OPENMP
    }else {
        for (thisTask = 0; thisTask < numTasks; ++thisTask)

# pragma omp parallel \
    shared   (thisTask,stateVecReal,stateVecImag,alphaRealSIMD,alphaImagSIMD, betaRealSIMD,betaImagSIMD) \
    private  (indexUp,indexLo, stateRealUpSIMD,stateImagUpSIMD,stateRealLoSIMD,stateImagLoSIMD)
    {
# pragma omp for schedule (static)
        for (indexUp = thisTask * sizeTask * 2; indexUp < thisTask * sizeTask * 2 + sizeTask; indexUp+=4) {

                indexLo     = indexUp + sizeTask;

                    // controlBit = extractBit (controlQubit, indexUp+chunkId*chunkSize);
                    // if (controlBit){
                    // store current state vector values in temp variables
                    stateRealUpSIMD = _mm256_loadu_pd(stateVecReal+indexUp);
                    stateImagUpSIMD = _mm256_loadu_pd(stateVecImag+indexUp);
                    stateRealLoSIMD = _mm256_loadu_pd(stateVecReal+indexLo);
                    stateImagLoSIMD = _mm256_loadu_pd(stateVecImag+indexLo);

                    // state[indexUp] = alpha * state[indexUp] - conj(beta)  * state[indexLo]
                    //stateVecReal[indexUp] = alphaReal*stateRealUp - alphaImag*stateImagUp
                    //    - betaReal*stateRealLo - betaImag*stateImagLo;
                    __m256d res1 =  _mm256_mul_pd(alphaRealSIMD,stateRealUpSIMD);
                    res1 = _mm256_sub_pd(res1,_mm256_mul_pd(alphaImagSIMD,stateImagUpSIMD));
                    res1 = _mm256_sub_pd(res1,_mm256_mul_pd(betaRealSIMD,stateRealLoSIMD));
                    res1 = _mm256_sub_pd(res1,_mm256_mul_pd(betaImagSIMD,stateImagLoSIMD));


                    //stateVecImag[indexUp] = alphaReal*stateImagUp + alphaImag*stateRealUp
                    //    - betaReal*stateImagLo + betaImag*stateRealLo;
                    __m256d res2 =  _mm256_mul_pd(alphaRealSIMD,stateImagUpSIMD);
                    res2 = _mm256_add_pd(res2,_mm256_mul_pd(alphaImagSIMD,stateRealUpSIMD));
                    res2 = _mm256_sub_pd(res2,_mm256_mul_pd(betaRealSIMD,stateImagLoSIMD));
                    res2 = _mm256_add_pd(res2,_mm256_mul_pd(betaImagSIMD,stateRealLoSIMD));

                    // state[indexLo] = beta  * state[indexUp] + conj(alpha) * state[indexLo]
                    //stateVecReal[indexLo] = betaReal*stateRealUp - betaImag*stateImagUp
                    //    + alphaReal*stateRealLo + alphaImag*stateImagLo;
                    __m256d res3 = _mm256_mul_pd(betaRealSIMD,stateRealUpSIMD);
                    res3 = _mm256_sub_pd(res3,_mm256_mul_pd(betaImagSIMD,stateImagUpSIMD));
                    res3 = _mm256_add_pd(res3,_mm256_mul_pd(alphaRealSIMD,stateRealLoSIMD));
                    res3 = _mm256_add_pd(res3,_mm256_mul_pd(alphaImagSIMD,stateImagLoSIMD));

                    //stateVecImag[indexLo] = betaReal*stateImagUp + betaImag*stateRealUp
                    //    + alphaReal*stateImagLo - alphaImag*stateRealLo;
                    __m256d res4 = _mm256_mul_pd(betaRealSIMD,stateImagUpSIMD);
                    res4 = _mm256_add_pd(res4,_mm256_mul_pd(betaImagSIMD,stateRealUpSIMD));
                    res4 = _mm256_add_pd(res4,_mm256_mul_pd(alphaRealSIMD,stateImagLoSIMD));
                    res4 = _mm256_sub_pd(res4,_mm256_mul_pd(alphaImagSIMD,stateRealLoSIMD));

                    _mm256_storeu_pd(stateVecReal+indexUp,res1);
                    _mm256_storeu_pd(stateVecImag+indexUp,res2);
                    _mm256_storeu_pd(stateVecReal+indexLo,res3);
                    _mm256_storeu_pd(stateVecImag+indexLo,res4);
                    // }        }
        }
    }
    }
# endif
}

void statevec_controlledCompactUnitaryLocalAllSmall (Qureg qureg, const int controlQubit, const int targetQubit,
        Complex alpha, Complex beta)
{
    long long int indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateRealLo,stateImagUp,stateImagLo;
    const long long int numTasks = (qureg.numAmpsPerChunk>>(1 + targetQubit)) ;
    const long long int sizeTask = (1LL << targetQubit);
    long long thisTask;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;
    qreal alphaImag=alpha.imag, alphaReal=alpha.real;
    qreal betaImag=beta.imag, betaReal=beta.real;

# ifdef _OPENMP
    if(numTasks >= omp_get_num_threads()){
# pragma omp parallel \
    shared   (stateVecReal,stateVecImag) \
    private  (thisTask,indexUp,indexLo, stateRealUp,stateImagUp)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask = 0; thisTask < numTasks; ++thisTask)
        for (indexUp = thisTask * sizeTask * 2; indexUp < thisTask * sizeTask * 2 + sizeTask; ++ indexUp) {

            indexLo     = indexUp + sizeTask;

                // store current state vector values in temp variables
                stateRealUp = stateVecReal[indexUp];
                stateImagUp = stateVecImag[indexUp];

                stateRealLo = stateVecReal[indexLo];
                stateImagLo = stateVecImag[indexLo];

                // state[indexUp] = alpha * state[indexUp] - conj(beta)  * state[indexLo]
                stateVecReal[indexUp] = alphaReal*stateRealUp - alphaImag*stateImagUp
                    - betaReal*stateRealLo - betaImag*stateImagLo;
                stateVecImag[indexUp] = alphaReal*stateImagUp + alphaImag*stateRealUp
                    - betaReal*stateImagLo + betaImag*stateRealLo;

                // state[indexLo] = beta  * state[indexUp] + conj(alpha) * state[indexLo]
                stateVecReal[indexLo] = betaReal*stateRealUp - betaImag*stateImagUp
                    + alphaReal*stateRealLo + alphaImag*stateImagLo;
                stateVecImag[indexLo] = betaReal*stateImagUp + betaImag*stateRealUp
                    + alphaReal*stateImagLo - alphaImag*stateRealLo;
        }
    }
# ifdef _OPENMP
    }else {
        for (thisTask = 0; thisTask < numTasks; ++thisTask)

# pragma omp parallel \
    shared   (thisTask,stateVecReal,stateVecImag) \
    private  (indexUp,indexLo, stateRealUp,stateImagUp)
    {
# pragma omp for schedule (static)
        for (indexUp = thisTask * sizeTask * 2; indexUp < thisTask * sizeTask * 2 + sizeTask; ++ indexUp) {

            indexLo     = indexUp + sizeTask;

                // store current state vector values in temp variables
                stateRealUp = stateVecReal[indexUp];
                stateImagUp = stateVecImag[indexUp];

                stateRealLo = stateVecReal[indexLo];
                stateImagLo = stateVecImag[indexLo];

                // state[indexUp] = alpha * state[indexUp] - conj(beta)  * state[indexLo]
                stateVecReal[indexUp] = alphaReal*stateRealUp - alphaImag*stateImagUp
                    - betaReal*stateRealLo - betaImag*stateImagLo;
                stateVecImag[indexUp] = alphaReal*stateImagUp + alphaImag*stateRealUp
                    - betaReal*stateImagLo + betaImag*stateRealLo;

                // state[indexLo] = beta  * state[indexUp] + conj(alpha) * state[indexLo]
                stateVecReal[indexLo] = betaReal*stateRealUp - betaImag*stateImagUp
                    + alphaReal*stateRealLo + alphaImag*stateImagLo;
                stateVecImag[indexLo] = betaReal*stateImagUp + betaImag*stateRealUp
                    + alphaReal*stateImagLo - alphaImag*stateRealLo;
        }
    }
    }
# endif
}


void statevec_controlledCompactUnitaryLocalAll (Qureg qureg, const int controlQubit, const int targetQubit,
        Complex alpha, Complex beta)
{
    long long int sizeBlock, sizeHalfBlock;
    long long int thisBlock, // current block
         indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateRealLo,stateImagUp,stateImagLo;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk>>1;

    // set dimensions
    sizeHalfBlock = 1LL << targetQubit;
    sizeBlock     = 2LL * sizeHalfBlock;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;
    qreal alphaImag=alpha.imag, alphaReal=alpha.real;
    qreal betaImag=beta.imag, betaReal=beta.real;
    
# ifdef _OPENMP
# pragma omp parallel \
    shared   (sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag, alphaReal,alphaImag, betaReal,betaImag) \
    private  (thisTask,thisBlock ,indexUp,indexLo, stateRealUp,stateImagUp,stateRealLo,stateImagLo)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {

            thisBlock   = thisTask / sizeHalfBlock;
            indexUp     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
            indexLo     = indexUp + sizeHalfBlock;

                // store current state vector values in temp variables
                stateRealUp = stateVecReal[indexUp];
                stateImagUp = stateVecImag[indexUp];

                stateRealLo = stateVecReal[indexLo];
                stateImagLo = stateVecImag[indexLo];

                // state[indexUp] = alpha * state[indexUp] - conj(beta)  * state[indexLo]
                stateVecReal[indexUp] = alphaReal*stateRealUp - alphaImag*stateImagUp
                    - betaReal*stateRealLo - betaImag*stateImagLo;
                stateVecImag[indexUp] = alphaReal*stateImagUp + alphaImag*stateRealUp
                    - betaReal*stateImagLo + betaImag*stateRealLo;

                // state[indexLo] = beta  * state[indexUp] + conj(alpha) * state[indexLo]
                stateVecReal[indexLo] = betaReal*stateRealUp - betaImag*stateImagUp
                    + alphaReal*stateRealLo + alphaImag*stateImagLo;
                stateVecImag[indexLo] = betaReal*stateImagUp + betaImag*stateRealUp
                    + alphaReal*stateImagLo - alphaImag*stateRealLo;

        }
    }

}

void statevec_controlledCompactUnitaryLocalSmall (Qureg qureg, const int controlQubit, const int targetQubit,
        Complex alpha, Complex beta)
{

    if((1LL<<controlQubit)>=qureg.numAmpsPerChunk) {
        if(extractBit(controlQubit,qureg.chunkId*qureg.numAmpsPerChunk)) {
            statevec_controlledCompactUnitaryLocalAllSmall(qureg,controlQubit,targetQubit,alpha,beta);
        }
        return ;
    }
    long long int sizeBlock, sizeHalfBlock;
    long long int thisBlock, // current block
         indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateRealLo,stateImagUp,stateImagLo;
    long long int thisTask;

    const long long int sizeTask = ((targetQubit > controlQubit) ? (1LL << controlQubit) : (1LL << targetQubit));
    if(sizeTask >= 4){
        statevec_controlledCompactUnitaryLocalSIMD(qureg,controlQubit,targetQubit,alpha,beta);
        //printf("simd");
        return;
    }

    const long long int numTasks = ((targetQubit > controlQubit) ? (1LL << (targetQubit - controlQubit - 1)) : (1LL << (controlQubit - targetQubit - 1)));
    // const long long int chunkSize=qureg.numAmpsPerChunk;
    // const long long int chunkId=qureg.chunkId;
    const long long int numBlocks = ((targetQubit > controlQubit) ? (qureg.numAmpsPerChunk>>(1 + targetQubit)) : (qureg.numAmpsPerChunk>>(1 + controlQubit)));
    const long long int blockOffset = 1LL << controlQubit;

    // set dimensions
    sizeHalfBlock = 1LL << targetQubit;
    sizeBlock = ((targetQubit > controlQubit) ? (2LL * sizeHalfBlock) : (2LL << controlQubit));

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;
    qreal alphaImag=alpha.imag, alphaReal=alpha.real;
    qreal betaImag=beta.imag, betaReal=beta.real;

# ifdef _OPENMP
    if(numBlocks >= omp_get_num_threads()){
# pragma omp parallel \
    shared   (sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag, alphaReal,alphaImag, betaReal,betaImag) \
    private  (thisTask,thisBlock ,indexUp,indexLo, stateRealUp,stateImagUp,stateRealLo,stateImagLo)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisBlock = 0; thisBlock < numBlocks; ++thisBlock) {
            for(thisTask = 0; thisTask < numTasks; ++thisTask)
            for(indexUp = thisBlock * sizeBlock + sizeTask * thisTask * 2 + blockOffset; indexUp < thisBlock * sizeBlock + sizeTask * thisTask * 2 + sizeTask + blockOffset; ++indexUp) {
            // for(indexUp = thisBlock * sizeBlock; indexUp < thisBlock * sizeBlock + sizeHalfBlock; ++indexUp) {

                indexLo     = indexUp + sizeHalfBlock;
             //   if(thisBlock != thisTask / sizeHalfBlock) exit(1);
             //   if(indexUp != thisBlock*sizeBlock + thisTask%sizeHalfBlock) exit(2);
             //   if(indexLo != indexUp + sizeHalfBlock) exit(3);

//                int controlBit = extractBit (controlQubit, indexUp+chunkId*chunkSize);
//                if (controlBit){
                    // store current state vector values in temp variables
                    stateRealUp = stateVecReal[indexUp];
                    stateImagUp = stateVecImag[indexUp];

                    stateRealLo = stateVecReal[indexLo];
                    stateImagLo = stateVecImag[indexLo];

                    // state[indexUp] = alpha * state[indexUp] - conj(beta)  * state[indexLo]
                    stateVecReal[indexUp] = alphaReal*stateRealUp - alphaImag*stateImagUp
                        - betaReal*stateRealLo - betaImag*stateImagLo;
                    stateVecImag[indexUp] = alphaReal*stateImagUp + alphaImag*stateRealUp
                        - betaReal*stateImagLo + betaImag*stateRealLo;

                    // state[indexLo] = beta  * state[indexUp] + conj(alpha) * state[indexLo]
                    stateVecReal[indexLo] = betaReal*stateRealUp - betaImag*stateImagUp
                        + alphaReal*stateRealLo + alphaImag*stateImagLo;
                    stateVecImag[indexLo] = betaReal*stateImagUp + betaImag*stateRealUp
                        + alphaReal*stateImagLo - alphaImag*stateRealLo;
//                }
            }
        }
    }
# ifdef _OPENMP
    } else {
        for (thisBlock = 0; thisBlock < numBlocks; ++thisBlock) {
# pragma omp parallel \
    shared   (sizeBlock,thisBlock,sizeHalfBlock, stateVecReal,stateVecImag, alphaReal,alphaImag, betaReal,betaImag) \
    private  (thisTask ,indexUp,indexLo, stateRealUp,stateImagUp,stateRealLo,stateImagLo)
    {
# pragma omp for schedule (static)
            for(thisTask = 0; thisTask < numTasks; ++thisTask)
            for(indexUp = thisBlock * sizeBlock + sizeTask * thisTask * 2 + blockOffset; indexUp < thisBlock * sizeBlock + sizeTask * thisTask * 2 + sizeTask + blockOffset; ++indexUp) {
            // for(indexUp = thisBlock * sizeBlock; indexUp < thisBlock * sizeBlock + sizeHalfBlock; ++indexUp) {

                indexLo     = indexUp + sizeHalfBlock;
             //   if(thisBlock != thisTask / sizeHalfBlock) exit(1);
             //   if(indexUp != thisBlock*sizeBlock + thisTask%sizeHalfBlock) exit(2);
             //   if(indexLo != indexUp + sizeHalfBlock) exit(3);

//                int controlBit = extractBit (controlQubit, indexUp+chunkId*chunkSize);
//                if (controlBit){
                    // store current state vector values in temp variables
                    stateRealUp = stateVecReal[indexUp];
                    stateImagUp = stateVecImag[indexUp];

                    stateRealLo = stateVecReal[indexLo];
                    stateImagLo = stateVecImag[indexLo];

                    // state[indexUp] = alpha * state[indexUp] - conj(beta)  * state[indexLo]
                    stateVecReal[indexUp] = alphaReal*stateRealUp - alphaImag*stateImagUp
                        - betaReal*stateRealLo - betaImag*stateImagLo;
                    stateVecImag[indexUp] = alphaReal*stateImagUp + alphaImag*stateRealUp
                        - betaReal*stateImagLo + betaImag*stateRealLo;

                    // state[indexLo] = beta  * state[indexUp] + conj(alpha) * state[indexLo]
                    stateVecReal[indexLo] = betaReal*stateRealUp - betaImag*stateImagUp
                        + alphaReal*stateRealLo + alphaImag*stateImagLo;
                    stateVecImag[indexLo] = betaReal*stateImagUp + betaImag*stateRealUp
                        + alphaReal*stateImagLo - alphaImag*stateRealLo;
//                }
            }
        }
    }
    }
# endif
}

void statevec_controlledCompactUnitaryLocalSIMD (Qureg qureg, const int controlQubit, const int targetQubit,
        Complex alpha, Complex beta)
{
    long long int sizeBlock, sizeHalfBlock;
    long long int thisBlock, // current block
         indexUp,indexLo;    // current index and corresponding index in lower half block

    long long int thisTask;
    const long long int numTasks = ((targetQubit > controlQubit) ? (1LL << (targetQubit - controlQubit - 1)) : (1LL << (controlQubit - targetQubit - 1)));
    // const long long int chunkSize=qureg.numAmpsPerChunk;
    // const long long int chunkId=qureg.chunkId;
    const long long int numBlocks = ((targetQubit > controlQubit) ? (qureg.numAmpsPerChunk>>(1 + targetQubit)) : (qureg.numAmpsPerChunk>>(1 + controlQubit)));
    const long long int sizeTask = ((targetQubit > controlQubit) ? (1LL << controlQubit) : (1LL << targetQubit));
    const long long int blockOffset = 1LL << controlQubit;

    // set dimensions
    sizeHalfBlock = 1LL << targetQubit;
    sizeBlock     = ((targetQubit > controlQubit) ? (2LL * sizeHalfBlock) : (2LL << controlQubit));

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;
    qreal alphaImag=alpha.imag, alphaReal=alpha.real;
    qreal betaImag=beta.imag, betaReal=beta.real;

    __m256d stateRealUpSIMD,stateImagUpSIMD,stateRealLoSIMD,stateImagLoSIMD;
    register const __m256d alphaRealSIMD = _mm256_set1_pd(alphaReal);
    register const __m256d alphaImagSIMD = _mm256_set1_pd(alphaImag);
    register const __m256d betaRealSIMD = _mm256_set1_pd(betaReal);
    register const __m256d betaImagSIMD = _mm256_set1_pd(betaImag);
    
# ifdef _OPENMP
    if(numBlocks >= omp_get_num_threads()){
# pragma omp parallel \
    shared   (sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag, alphaRealSIMD,alphaImagSIMD, betaRealSIMD,betaImagSIMD) \
    private  (thisTask,thisBlock ,indexUp,indexLo, stateRealUpSIMD,stateImagUpSIMD,stateRealLoSIMD,stateImagLoSIMD)
# endif
        {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
            for (thisBlock = 0; thisBlock < numBlocks; ++thisBlock) {
                for(thisTask = 0; thisTask < numTasks; ++thisTask)
                for(indexUp = thisBlock * sizeBlock + sizeTask * thisTask * 2 + blockOffset; indexUp < thisBlock * sizeBlock + sizeTask * thisTask * 2 + sizeTask + blockOffset; indexUp+=4) {
                // for(indexUp = thisBlock * sizeBlock; indexUp < thisBlock * sizeBlock + sizeHalfBlock; ++indexUp) {

                    indexLo     = indexUp + sizeHalfBlock;

                    // controlBit = extractBit (controlQubit, indexUp+chunkId*chunkSize);
                    // if (controlBit){
                    // store current state vector values in temp variables
                    stateRealUpSIMD = _mm256_loadu_pd(stateVecReal+indexUp);
                    stateImagUpSIMD = _mm256_loadu_pd(stateVecImag+indexUp);
                    stateRealLoSIMD = _mm256_loadu_pd(stateVecReal+indexLo);
                    stateImagLoSIMD = _mm256_loadu_pd(stateVecImag+indexLo);

                    // state[indexUp] = alpha * state[indexUp] - conj(beta)  * state[indexLo]
                    //stateVecReal[indexUp] = alphaReal*stateRealUp - alphaImag*stateImagUp
                    //    - betaReal*stateRealLo - betaImag*stateImagLo;
                    __m256d res1 =  _mm256_mul_pd(alphaRealSIMD,stateRealUpSIMD);
                    res1 = _mm256_sub_pd(res1,_mm256_mul_pd(alphaImagSIMD,stateImagUpSIMD));
                    res1 = _mm256_sub_pd(res1,_mm256_mul_pd(betaRealSIMD,stateRealLoSIMD));
                    res1 = _mm256_sub_pd(res1,_mm256_mul_pd(betaImagSIMD,stateImagLoSIMD));


                    //stateVecImag[indexUp] = alphaReal*stateImagUp + alphaImag*stateRealUp
                    //    - betaReal*stateImagLo + betaImag*stateRealLo;
                    __m256d res2 =  _mm256_mul_pd(alphaRealSIMD,stateImagUpSIMD);
                    res2 = _mm256_add_pd(res2,_mm256_mul_pd(alphaImagSIMD,stateRealUpSIMD));
                    res2 = _mm256_sub_pd(res2,_mm256_mul_pd(betaRealSIMD,stateImagLoSIMD));
                    res2 = _mm256_add_pd(res2,_mm256_mul_pd(betaImagSIMD,stateRealLoSIMD));

                    // state[indexLo] = beta  * state[indexUp] + conj(alpha) * state[indexLo]
                    //stateVecReal[indexLo] = betaReal*stateRealUp - betaImag*stateImagUp
                    //    + alphaReal*stateRealLo + alphaImag*stateImagLo;
                    __m256d res3 = _mm256_mul_pd(betaRealSIMD,stateRealUpSIMD);
                    res3 = _mm256_sub_pd(res3,_mm256_mul_pd(betaImagSIMD,stateImagUpSIMD));
                    res3 = _mm256_add_pd(res3,_mm256_mul_pd(alphaRealSIMD,stateRealLoSIMD));
                    res3 = _mm256_add_pd(res3,_mm256_mul_pd(alphaImagSIMD,stateImagLoSIMD));

                    //stateVecImag[indexLo] = betaReal*stateImagUp + betaImag*stateRealUp
                    //    + alphaReal*stateImagLo - alphaImag*stateRealLo;
                    __m256d res4 = _mm256_mul_pd(betaRealSIMD,stateImagUpSIMD);
                    res4 = _mm256_add_pd(res4,_mm256_mul_pd(betaImagSIMD,stateRealUpSIMD));
                    res4 = _mm256_add_pd(res4,_mm256_mul_pd(alphaRealSIMD,stateImagLoSIMD));
                    res4 = _mm256_sub_pd(res4,_mm256_mul_pd(alphaImagSIMD,stateRealLoSIMD));

                    _mm256_storeu_pd(stateVecReal+indexUp,res1);
                    _mm256_storeu_pd(stateVecImag+indexUp,res2);
                    _mm256_storeu_pd(stateVecReal+indexLo,res3);
                    _mm256_storeu_pd(stateVecImag+indexLo,res4);
                    // }
                }
            }
        }
# ifdef _OPENMP
    } else {
         for (thisBlock = 0; thisBlock < numBlocks; ++thisBlock) 

# pragma omp parallel \
    shared   (thisBlock,sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag, alphaRealSIMD,alphaImagSIMD, betaRealSIMD,betaImagSIMD) \
    private  (thisTask ,indexUp,indexLo, stateRealUpSIMD,stateImagUpSIMD,stateRealLoSIMD,stateImagLoSIMD)
        {
# pragma omp for schedule (static)
                for(thisTask = 0; thisTask < numTasks; ++thisTask) {
                for(indexUp = thisBlock * sizeBlock + sizeTask * thisTask * 2 + blockOffset; indexUp < thisBlock * sizeBlock + sizeTask * thisTask * 2 + sizeTask + blockOffset; indexUp+=4) {
                // for(indexUp = thisBlock * sizeBlock; indexUp < thisBlock * sizeBlock + sizeHalfBlock; ++indexUp) {

                    indexLo     = indexUp + sizeHalfBlock;

                    // controlBit = extractBit (controlQubit, indexUp+chunkId*chunkSize);
                    // if (controlBit){
                    // store current state vector values in temp variables
                    stateRealUpSIMD = _mm256_loadu_pd(stateVecReal+indexUp);
                    stateImagUpSIMD = _mm256_loadu_pd(stateVecImag+indexUp);
                    stateRealLoSIMD = _mm256_loadu_pd(stateVecReal+indexLo);
                    stateImagLoSIMD = _mm256_loadu_pd(stateVecImag+indexLo);

                    // state[indexUp] = alpha * state[indexUp] - conj(beta)  * state[indexLo]
                    //stateVecReal[indexUp] = alphaReal*stateRealUp - alphaImag*stateImagUp
                    //    - betaReal*stateRealLo - betaImag*stateImagLo;
                    __m256d res1 =  _mm256_mul_pd(alphaRealSIMD,stateRealUpSIMD);
                    res1 = _mm256_sub_pd(res1,_mm256_mul_pd(alphaImagSIMD,stateImagUpSIMD));
                    res1 = _mm256_sub_pd(res1,_mm256_mul_pd(betaRealSIMD,stateRealLoSIMD));
                    res1 = _mm256_sub_pd(res1,_mm256_mul_pd(betaImagSIMD,stateImagLoSIMD));


                    //stateVecImag[indexUp] = alphaReal*stateImagUp + alphaImag*stateRealUp
                    //    - betaReal*stateImagLo + betaImag*stateRealLo;
                    __m256d res2 =  _mm256_mul_pd(alphaRealSIMD,stateImagUpSIMD);
                    res2 = _mm256_add_pd(res2,_mm256_mul_pd(alphaImagSIMD,stateRealUpSIMD));
                    res2 = _mm256_sub_pd(res2,_mm256_mul_pd(betaRealSIMD,stateImagLoSIMD));
                    res2 = _mm256_add_pd(res2,_mm256_mul_pd(betaImagSIMD,stateRealLoSIMD));

                    // state[indexLo] = beta  * state[indexUp] + conj(alpha) * state[indexLo]
                    //stateVecReal[indexLo] = betaReal*stateRealUp - betaImag*stateImagUp
                    //    + alphaReal*stateRealLo + alphaImag*stateImagLo;
                    __m256d res3 = _mm256_mul_pd(betaRealSIMD,stateRealUpSIMD);
                    res3 = _mm256_sub_pd(res3,_mm256_mul_pd(betaImagSIMD,stateImagUpSIMD));
                    res3 = _mm256_add_pd(res3,_mm256_mul_pd(alphaRealSIMD,stateRealLoSIMD));
                    res3 = _mm256_add_pd(res3,_mm256_mul_pd(alphaImagSIMD,stateImagLoSIMD));

                    //stateVecImag[indexLo] = betaReal*stateImagUp + betaImag*stateRealUp
                    //    + alphaReal*stateImagLo - alphaImag*stateRealLo;
                    __m256d res4 = _mm256_mul_pd(betaRealSIMD,stateImagUpSIMD);
                    res4 = _mm256_add_pd(res4,_mm256_mul_pd(betaImagSIMD,stateRealUpSIMD));
                    res4 = _mm256_add_pd(res4,_mm256_mul_pd(alphaRealSIMD,stateImagLoSIMD));
                    res4 = _mm256_sub_pd(res4,_mm256_mul_pd(alphaImagSIMD,stateRealLoSIMD));

                    _mm256_storeu_pd(stateVecReal+indexUp,res1);
                    _mm256_storeu_pd(stateVecImag+indexUp,res2);
                    _mm256_storeu_pd(stateVecReal+indexLo,res3);
                    _mm256_storeu_pd(stateVecImag+indexLo,res4);
                    // }
                }
            }
        }

    }
# endif

}

void statevec_controlledCompactUnitaryLocal (Qureg qureg, const int controlQubit, const int targetQubit,
        Complex alpha, Complex beta)
{
    long long int sizeBlock, sizeHalfBlock;
    long long int thisBlock, // current block
         indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateRealLo,stateImagUp,stateImagLo;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk>>1;
    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;

    int controlBit;

    // set dimensions
    sizeHalfBlock = 1LL << targetQubit;
    sizeBlock     = 2LL * sizeHalfBlock;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;
    qreal alphaImag=alpha.imag, alphaReal=alpha.real;
    qreal betaImag=beta.imag, betaReal=beta.real;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag, alphaReal,alphaImag, betaReal,betaImag) \
    private  (thisTask,thisBlock ,indexUp,indexLo, stateRealUp,stateImagUp,stateRealLo,stateImagLo,controlBit)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {

            thisBlock   = thisTask / sizeHalfBlock;
            indexUp     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
            indexLo     = indexUp + sizeHalfBlock;

            controlBit = extractBit (controlQubit, indexUp+chunkId*chunkSize);
            if (controlBit){
                // store current state vector values in temp variables
                stateRealUp = stateVecReal[indexUp];
                stateImagUp = stateVecImag[indexUp];

                stateRealLo = stateVecReal[indexLo];
                stateImagLo = stateVecImag[indexLo];

                // state[indexUp] = alpha * state[indexUp] - conj(beta)  * state[indexLo]
                stateVecReal[indexUp] = alphaReal*stateRealUp - alphaImag*stateImagUp
                    - betaReal*stateRealLo - betaImag*stateImagLo;
                stateVecImag[indexUp] = alphaReal*stateImagUp + alphaImag*stateRealUp
                    - betaReal*stateImagLo + betaImag*stateRealLo;

                // state[indexLo] = beta  * state[indexUp] + conj(alpha) * state[indexLo]
                stateVecReal[indexLo] = betaReal*stateRealUp - betaImag*stateImagUp
                    + alphaReal*stateRealLo + alphaImag*stateImagLo;
                stateVecImag[indexLo] = betaReal*stateImagUp + betaImag*stateRealUp
                    + alphaReal*stateImagLo - alphaImag*stateRealLo;
            }

        }
    }

}

/* ctrlQubitsMask is a bit mask indicating which qubits are control Qubits
 * ctrlFlipMask is a bit mask indicating which control qubits should be 'flipped'
 * in the condition, i.e. they should have value 0 when the unitary is applied
 */
void statevec_multiControlledUnitaryLocal(
    Qureg qureg, const int targetQubit,
    long long int ctrlQubitsMask, long long int ctrlFlipMask,
    ComplexMatrix2 u)
{
    long long int sizeBlock, sizeHalfBlock;
    long long int thisBlock, // current block
         indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateRealLo,stateImagUp,stateImagLo;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk>>1;
    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;

    // set dimensions
    sizeHalfBlock = 1LL << targetQubit;
    sizeBlock     = 2LL * sizeHalfBlock;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag, u, ctrlQubitsMask,ctrlFlipMask) \
    private  (thisTask,thisBlock, indexUp,indexLo, stateRealUp,stateImagUp,stateRealLo,stateImagLo)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {

            thisBlock   = thisTask / sizeHalfBlock;
            indexUp     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
            indexLo     = indexUp + sizeHalfBlock;


            // take the basis index, flip the designated (XOR) 'control' bits, AND with the controls.
            // if this equals the control mask, the control qubits have the desired values in the basis index
            if (ctrlQubitsMask == (ctrlQubitsMask & ((indexUp+chunkId*chunkSize) ^ ctrlFlipMask))) {
                // store current state vector values in temp variables
                stateRealUp = stateVecReal[indexUp];
                stateImagUp = stateVecImag[indexUp];

                stateRealLo = stateVecReal[indexLo];
                stateImagLo = stateVecImag[indexLo];

                // state[indexUp] = u00 * state[indexUp] + u01 * state[indexLo]
                stateVecReal[indexUp] = u.real[0][0]*stateRealUp - u.imag[0][0]*stateImagUp
                    + u.real[0][1]*stateRealLo - u.imag[0][1]*stateImagLo;
                stateVecImag[indexUp] = u.real[0][0]*stateImagUp + u.imag[0][0]*stateRealUp
                    + u.real[0][1]*stateImagLo + u.imag[0][1]*stateRealLo;

                // state[indexLo] = u10  * state[indexUp] + u11 * state[indexLo]
                stateVecReal[indexLo] = u.real[1][0]*stateRealUp  - u.imag[1][0]*stateImagUp
                    + u.real[1][1]*stateRealLo  -  u.imag[1][1]*stateImagLo;
                stateVecImag[indexLo] = u.real[1][0]*stateImagUp + u.imag[1][0]*stateRealUp
                    + u.real[1][1]*stateImagLo + u.imag[1][1]*stateRealLo;
            }
        }
    }

}

void statevec_controlledUnitaryLocal(Qureg qureg, const int controlQubit, const int targetQubit,
        ComplexMatrix2 u)
{
    long long int sizeBlock, sizeHalfBlock;
    long long int thisBlock, // current block
         indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateRealLo,stateImagUp,stateImagLo;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk>>1;
    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;

    int controlBit;

    // set dimensions
    sizeHalfBlock = 1LL << targetQubit;
    sizeBlock     = 2LL * sizeHalfBlock;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag, u) \
    private  (thisTask,thisBlock ,indexUp,indexLo, stateRealUp,stateImagUp,stateRealLo,stateImagLo,controlBit)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {

            thisBlock   = thisTask / sizeHalfBlock;
            indexUp     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
            indexLo     = indexUp + sizeHalfBlock;

            controlBit = extractBit (controlQubit, indexUp+chunkId*chunkSize);
            if (controlBit){
                // store current state vector values in temp variables
                stateRealUp = stateVecReal[indexUp];
                stateImagUp = stateVecImag[indexUp];

                stateRealLo = stateVecReal[indexLo];
                stateImagLo = stateVecImag[indexLo];


                // state[indexUp] = u00 * state[indexUp] + u01 * state[indexLo]
                stateVecReal[indexUp] = u.real[0][0]*stateRealUp - u.imag[0][0]*stateImagUp
                    + u.real[0][1]*stateRealLo - u.imag[0][1]*stateImagLo;
                stateVecImag[indexUp] = u.real[0][0]*stateImagUp + u.imag[0][0]*stateRealUp
                    + u.real[0][1]*stateImagLo + u.imag[0][1]*stateRealLo;

                // state[indexLo] = u10  * state[indexUp] + u11 * state[indexLo]
                stateVecReal[indexLo] = u.real[1][0]*stateRealUp  - u.imag[1][0]*stateImagUp
                    + u.real[1][1]*stateRealLo  -  u.imag[1][1]*stateImagLo;
                stateVecImag[indexLo] = u.real[1][0]*stateImagUp + u.imag[1][0]*stateRealUp
                    + u.real[1][1]*stateImagLo + u.imag[1][1]*stateRealLo;
            }
        }
    }
}
void statevec_controlledCompactUnitaryDistributedAllSIMD (Qureg qureg, const int controlQubit,
        Complex rot1, Complex rot2,
        ComplexArray stateVecUp,
        ComplexArray stateVecLo,
        ComplexArray stateVecOut)
{

    __m256d stateRealUpSIMD,stateRealLoSIMD,stateImagUpSIMD,stateImagLoSIMD;

    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk;

    qreal rot1Real=rot1.real, rot1Imag=rot1.imag;
    qreal rot2Real=rot2.real, rot2Imag=rot2.imag;
    qreal *stateVecRealUp=stateVecUp.real, *stateVecImagUp=stateVecUp.imag;
    qreal *stateVecRealLo=stateVecLo.real, *stateVecImagLo=stateVecLo.imag;
    qreal *stateVecRealOut=stateVecOut.real, *stateVecImagOut=stateVecOut.imag;

    register const __m256d rot1RealSIMD = _mm256_set1_pd(rot1Real);
    register const __m256d rot1ImagSIMD = _mm256_set1_pd(rot1Imag);
    register const __m256d rot2RealSIMD = _mm256_set1_pd(rot2Real);
    register const __m256d rot2ImagSIMD = _mm256_set1_pd(rot2Imag);

# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecRealUp,stateVecImagUp,stateVecRealLo,stateVecImagLo,stateVecRealOut,stateVecImagOut, \
            rot1RealSIMD,rot1ImagSIMD, rot2RealSIMD,rot2ImagSIMD) \
    private  (thisTask,stateRealUpSIMD,stateImagUpSIMD,stateRealLoSIMD,stateImagLoSIMD)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask+=4) {
                stateRealUpSIMD = _mm256_loadu_pd(stateVecRealUp+thisTask);
                stateImagUpSIMD = _mm256_loadu_pd(stateVecImagUp+thisTask);

                stateRealLoSIMD = _mm256_loadu_pd(stateVecRealLo+thisTask);
                stateImagLoSIMD = _mm256_loadu_pd(stateVecImagLo+thisTask);

                _mm256_storeu_pd(stateVecRealOut+thisTask, _mm256_add_pd( \
                                _mm256_sub_pd(_mm256_mul_pd(rot1RealSIMD,stateRealUpSIMD),_mm256_mul_pd(rot1ImagSIMD,stateImagUpSIMD)), \
                                _mm256_add_pd(_mm256_mul_pd(rot2RealSIMD,stateRealLoSIMD),_mm256_mul_pd(rot2ImagSIMD,stateImagLoSIMD))));

                _mm256_storeu_pd(stateVecImagOut+thisTask, _mm256_add_pd( \
                                _mm256_add_pd(_mm256_mul_pd(rot1RealSIMD,stateImagUpSIMD),_mm256_mul_pd(rot1ImagSIMD,stateRealUpSIMD)), \
                                _mm256_sub_pd(_mm256_mul_pd(rot2RealSIMD,stateImagLoSIMD),_mm256_mul_pd(rot2ImagSIMD,stateRealLoSIMD))));
        }
    }
}


void statevec_controlledCompactUnitaryDistributedAll (Qureg qureg, const int controlQubit,
        Complex rot1, Complex rot2,
        ComplexArray stateVecUp,
        ComplexArray stateVecLo,
        ComplexArray stateVecOut)
{
    qreal   stateRealUp,stateRealLo,stateImagUp,stateImagLo;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk;
    if(numTasks>4){
        statevec_controlledCompactUnitaryDistributedAllSIMD(qureg,controlQubit,rot1,rot2,stateVecUp,stateVecLo,stateVecOut);
        return ;
    }
    qreal rot1Real=rot1.real, rot1Imag=rot1.imag;
    qreal rot2Real=rot2.real, rot2Imag=rot2.imag;
    qreal *stateVecRealUp=stateVecUp.real, *stateVecImagUp=stateVecUp.imag;
    qreal *stateVecRealLo=stateVecLo.real, *stateVecImagLo=stateVecLo.imag;
    qreal *stateVecRealOut=stateVecOut.real, *stateVecImagOut=stateVecOut.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecRealUp,stateVecImagUp,stateVecRealLo,stateVecImagLo,stateVecRealOut,stateVecImagOut, \
            rot1Real,rot1Imag, rot2Real,rot2Imag) \
    private  (thisTask,stateRealUp,stateImagUp,stateRealLo,stateImagLo)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
                // store current state vector values in temp variables
                stateRealUp = stateVecRealUp[thisTask];
                stateImagUp = stateVecImagUp[thisTask];

                stateRealLo = stateVecRealLo[thisTask];
                stateImagLo = stateVecImagLo[thisTask];

                // state[indexUp] = alpha * state[indexUp] - conj(beta)  * state[indexLo]
                stateVecRealOut[thisTask] = rot1Real*stateRealUp - rot1Imag*stateImagUp + rot2Real*stateRealLo + rot2Imag*stateImagLo;
                stateVecImagOut[thisTask] = rot1Real*stateImagUp + rot1Imag*stateRealUp + rot2Real*stateImagLo - rot2Imag*stateRealLo;
        }
    }
}


/** Rotate a single qubit in the state vector of probability amplitudes, given two complex
 * numbers alpha and beta and a subset of the state vector with upper and lower block values
 * stored seperately. Only perform the rotation where the control qubit is one.
 *
 *  @param[in,out] qureg object representing the set of qubits
 *  @param[in] controlQubit qubit to determine whether or not to perform a rotation
 *  @param[in] rot1 rotation angle
 *  @param[in] rot2 rotation angle
 *  @param[in] stateVecUp probability amplitudes in upper half of a block
 *  @param[in] stateVecLo probability amplitudes in lower half of a block
 *  @param[out] stateVecOut array section to update (will correspond to either the lower or upper half of a block)
 */
void statevec_controlledCompactUnitaryDistributed (Qureg qureg, const int controlQubit,
        Complex rot1, Complex rot2,
        ComplexArray stateVecUp,
        ComplexArray stateVecLo,
        ComplexArray stateVecOut)
{
    if((1LL<<controlQubit)>=qureg.numAmpsPerChunk) {
        if(extractBit(controlQubit,qureg.chunkId*qureg.numAmpsPerChunk)) {
            statevec_controlledCompactUnitaryDistributedAll(qureg,controlQubit,rot1,rot2,stateVecUp,stateVecLo,stateVecOut);
        }
        return ;
    }
    qreal   stateRealUp,stateRealLo,stateImagUp,stateImagLo;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk;
    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;

    if((1<<(controlQubit-1)) >= 4){
        statevec_controlledCompactUnitaryDistributedSIMD(qureg,controlQubit,rot1,rot2,stateVecUp,stateVecLo,stateVecOut);
        return;
    }

    int controlBit;

    qreal rot1Real=rot1.real, rot1Imag=rot1.imag;
    qreal rot2Real=rot2.real, rot2Imag=rot2.imag;
    qreal *stateVecRealUp=stateVecUp.real, *stateVecImagUp=stateVecUp.imag;
    qreal *stateVecRealLo=stateVecLo.real, *stateVecImagLo=stateVecLo.imag;
    qreal *stateVecRealOut=stateVecOut.real, *stateVecImagOut=stateVecOut.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecRealUp,stateVecImagUp,stateVecRealLo,stateVecImagLo,stateVecRealOut,stateVecImagOut, \
            rot1Real,rot1Imag, rot2Real,rot2Imag) \
    private  (thisTask,stateRealUp,stateImagUp,stateRealLo,stateImagLo,controlBit)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            controlBit = extractBit (controlQubit, thisTask+chunkId*chunkSize);
            if (controlBit){
                // store current state vector values in temp variables
                stateRealUp = stateVecRealUp[thisTask];
                stateImagUp = stateVecImagUp[thisTask];

                stateRealLo = stateVecRealLo[thisTask];
                stateImagLo = stateVecImagLo[thisTask];

                // state[indexUp] = alpha * state[indexUp] - conj(beta)  * state[indexLo]
                stateVecRealOut[thisTask] = rot1Real*stateRealUp - rot1Imag*stateImagUp + rot2Real*stateRealLo + rot2Imag*stateImagLo;
                stateVecImagOut[thisTask] = rot1Real*stateImagUp + rot1Imag*stateRealUp + rot2Real*stateImagLo - rot2Imag*stateRealLo;
            }
        }
    }
}

void statevec_controlledCompactUnitaryDistributedSIMD (Qureg qureg, const int controlQubit,
        Complex rot1, Complex rot2,
        ComplexArray stateVecUp,
        ComplexArray stateVecLo,
        ComplexArray stateVecOut)
{

    __m256d stateRealUpSIMD,stateRealLoSIMD,stateImagUpSIMD,stateImagLoSIMD;

    long long int thisTask;
    long long int thisBlock;
    long long int taskEnd;
    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;

    const long long int blockSize=2LL<<controlQubit;
    const long long int halfBlockSize = blockSize>>1;

    const long long int blockStart=(chunkId*chunkSize)/(blockSize);
    const long long int blockRange=(chunkSize>blockSize)?(chunkSize/blockSize):1;
    const long long int blockEnd=(chunkId+1)*chunkSize-blockStart*blockSize;
    if(blockEnd <= 0) return;

    qreal rot1Real=rot1.real, rot1Imag=rot1.imag;
    qreal rot2Real=rot2.real, rot2Imag=rot2.imag;
    qreal *stateVecRealUp=stateVecUp.real, *stateVecImagUp=stateVecUp.imag;
    qreal *stateVecRealLo=stateVecLo.real, *stateVecImagLo=stateVecLo.imag;
    qreal *stateVecRealOut=stateVecOut.real, *stateVecImagOut=stateVecOut.imag;

    register const __m256d rot1RealSIMD = _mm256_set1_pd(rot1Real);
    register const __m256d rot1ImagSIMD = _mm256_set1_pd(rot1Imag);
    register const __m256d rot2RealSIMD = _mm256_set1_pd(rot2Real);
    register const __m256d rot2ImagSIMD = _mm256_set1_pd(rot2Imag);

# ifdef _OPENMP
    if(blockRange >= omp_get_num_threads()){
# pragma omp parallel \
    shared   (stateVecRealUp,stateVecImagUp,stateVecRealLo,stateVecImagLo,stateVecRealOut,stateVecImagOut, \
            rot1RealSIMD,rot1ImagSIMD, rot2RealSIMD,rot2ImagSIMD) \
    private  (thisTask,thisBlock,taskEnd,stateRealUpSIMD,stateImagUpSIMD,stateRealLoSIMD,stateImagLoSIMD)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for(thisBlock=0; thisBlock<blockRange; thisBlock++){
            taskEnd = fmin(blockEnd,(thisBlock+1)*blockSize);

            for(thisTask=thisBlock*blockSize+halfBlockSize; thisTask<taskEnd; thisTask+=4){
                stateRealUpSIMD = _mm256_loadu_pd(stateVecRealUp+thisTask);
                stateImagUpSIMD = _mm256_loadu_pd(stateVecImagUp+thisTask);

                stateRealLoSIMD = _mm256_loadu_pd(stateVecRealLo+thisTask);
                stateImagLoSIMD = _mm256_loadu_pd(stateVecImagLo+thisTask);

                _mm256_storeu_pd(stateVecRealOut+thisTask, _mm256_add_pd( \
                                _mm256_sub_pd(_mm256_mul_pd(rot1RealSIMD,stateRealUpSIMD),_mm256_mul_pd(rot1ImagSIMD,stateImagUpSIMD)), \
                                _mm256_add_pd(_mm256_mul_pd(rot2RealSIMD,stateRealLoSIMD),_mm256_mul_pd(rot2ImagSIMD,stateImagLoSIMD))));

                _mm256_storeu_pd(stateVecImagOut+thisTask, _mm256_add_pd( \
                                _mm256_add_pd(_mm256_mul_pd(rot1RealSIMD,stateImagUpSIMD),_mm256_mul_pd(rot1ImagSIMD,stateRealUpSIMD)), \
                                _mm256_sub_pd(_mm256_mul_pd(rot2RealSIMD,stateImagLoSIMD),_mm256_mul_pd(rot2ImagSIMD,stateRealLoSIMD))));
            }
        }
    }
# ifdef _OPENMP
    } else {
        for(thisBlock=0; thisBlock<blockRange; thisBlock++){
            taskEnd = fmin(blockEnd,(thisBlock+1)*blockSize);
# pragma omp parallel \
    shared   (taskEnd,thisBlock,stateVecRealUp,stateVecImagUp,stateVecRealLo,stateVecImagLo,stateVecRealOut,stateVecImagOut, \
            rot1RealSIMD,rot1ImagSIMD, rot2RealSIMD,rot2ImagSIMD) \
    private  (thisTask,stateRealUpSIMD,stateImagUpSIMD,stateRealLoSIMD,stateImagLoSIMD)
    {
# pragma omp for schedule (static)
            for(thisTask=thisBlock*blockSize+halfBlockSize; thisTask<taskEnd; thisTask+=4){
                stateRealUpSIMD = _mm256_loadu_pd(stateVecRealUp+thisTask);
                stateImagUpSIMD = _mm256_loadu_pd(stateVecImagUp+thisTask);

                stateRealLoSIMD = _mm256_loadu_pd(stateVecRealLo+thisTask);
                stateImagLoSIMD = _mm256_loadu_pd(stateVecImagLo+thisTask);

                _mm256_storeu_pd(stateVecRealOut+thisTask, _mm256_add_pd( \
                                _mm256_sub_pd(_mm256_mul_pd(rot1RealSIMD,stateRealUpSIMD),_mm256_mul_pd(rot1ImagSIMD,stateImagUpSIMD)), \
                                _mm256_add_pd(_mm256_mul_pd(rot2RealSIMD,stateRealLoSIMD),_mm256_mul_pd(rot2ImagSIMD,stateImagLoSIMD))));

                _mm256_storeu_pd(stateVecImagOut+thisTask, _mm256_add_pd( \
                                _mm256_add_pd(_mm256_mul_pd(rot1RealSIMD,stateImagUpSIMD),_mm256_mul_pd(rot1ImagSIMD,stateRealUpSIMD)), \
                                _mm256_sub_pd(_mm256_mul_pd(rot2RealSIMD,stateImagLoSIMD),_mm256_mul_pd(rot2ImagSIMD,stateRealLoSIMD))));
            }
        }
    }
    }
#endif
}

/** Rotate a single qubit in the state vector of probability amplitudes, given two complex
 *  numbers alpha and beta and a subset of the state vector with upper and lower block values
 *  stored seperately. Only perform the rotation where the control qubit is one.
 *
 *  @param[in,out] qureg object representing the set of qubits
 *  @param[in] controlQubit qubit to determine whether or not to perform a rotation
 *  @param[in] rot1 rotation angle
 *  @param[in] rot2 rotation angle
 *  @param[in] stateVecUp probability amplitudes in upper half of a block
 *  @param[in] stateVecLo probability amplitudes in lower half of a block
 *  @param[out] stateVecOut array section to update (will correspond to either the lower or upper half of a block)
 */
void statevec_controlledUnitaryDistributed (Qureg qureg, const int controlQubit,
        Complex rot1, Complex rot2,
        ComplexArray stateVecUp,
        ComplexArray stateVecLo,
        ComplexArray stateVecOut)
{

    qreal   stateRealUp,stateRealLo,stateImagUp,stateImagLo;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk;
    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;

    int controlBit;

    qreal rot1Real=rot1.real, rot1Imag=rot1.imag;
    qreal rot2Real=rot2.real, rot2Imag=rot2.imag;
    qreal *stateVecRealUp=stateVecUp.real, *stateVecImagUp=stateVecUp.imag;
    qreal *stateVecRealLo=stateVecLo.real, *stateVecImagLo=stateVecLo.imag;
    qreal *stateVecRealOut=stateVecOut.real, *stateVecImagOut=stateVecOut.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecRealUp,stateVecImagUp,stateVecRealLo,stateVecImagLo,stateVecRealOut,stateVecImagOut, \
            rot1Real,rot1Imag, rot2Real,rot2Imag) \
    private  (thisTask,stateRealUp,stateImagUp,stateRealLo,stateImagLo,controlBit)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            controlBit = extractBit (controlQubit, thisTask+chunkId*chunkSize);
            if (controlBit){
                // store current state vector values in temp variables
                stateRealUp = stateVecRealUp[thisTask];
                stateImagUp = stateVecImagUp[thisTask];

                stateRealLo = stateVecRealLo[thisTask];
                stateImagLo = stateVecImagLo[thisTask];

                stateVecRealOut[thisTask] = rot1Real*stateRealUp - rot1Imag*stateImagUp
                    + rot2Real*stateRealLo - rot2Imag*stateImagLo;
                stateVecImagOut[thisTask] = rot1Real*stateImagUp + rot1Imag*stateRealUp
                    + rot2Real*stateImagLo + rot2Imag*stateRealLo;
            }
        }
    }
}

/** Apply a unitary operation to a single qubit in the state vector of probability amplitudes, given
 *  a subset of the state vector with upper and lower block values
 stored seperately. Only perform the rotation where all the control qubits are 1.
 *
 *  @param[in,out] qureg object representing the set of qubits
 *  @param[in] targetQubit qubit to rotate
 *  @param[in] ctrlQubitsMask a bit mask indicating whether each qubit is a control (1) or not (0)
 *  @param[in] ctrlFlipMask a bit mask indicating whether each qubit (only controls are relevant)
 *             should be flipped when checking the control condition
 *  @param[in] rot1 rotation angle
 *  @param[in] rot2 rotation angle
 *  @param[in] stateVecUp probability amplitudes in upper half of a block
 *  @param[in] stateVecLo probability amplitudes in lower half of a block
 *  @param[out] stateVecOut array section to update (will correspond to either the lower or upper half of a block)
 */
void statevec_multiControlledUnitaryDistributed (
        Qureg qureg,
        const int targetQubit,
        long long int ctrlQubitsMask, long long int ctrlFlipMask,
        Complex rot1, Complex rot2,
        ComplexArray stateVecUp,
        ComplexArray stateVecLo,
        ComplexArray stateVecOut)
{

    qreal   stateRealUp,stateRealLo,stateImagUp,stateImagLo;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk;
    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;

    qreal rot1Real=rot1.real, rot1Imag=rot1.imag;
    qreal rot2Real=rot2.real, rot2Imag=rot2.imag;
    qreal *stateVecRealUp=stateVecUp.real, *stateVecImagUp=stateVecUp.imag;
    qreal *stateVecRealLo=stateVecLo.real, *stateVecImagLo=stateVecLo.imag;
    qreal *stateVecRealOut=stateVecOut.real, *stateVecImagOut=stateVecOut.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecRealUp,stateVecImagUp,stateVecRealLo,stateVecImagLo,stateVecRealOut,stateVecImagOut, \
            rot1Real,rot1Imag, rot2Real,rot2Imag, ctrlQubitsMask,ctrlFlipMask) \
    private  (thisTask,stateRealUp,stateImagUp,stateRealLo,stateImagLo)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            if (ctrlQubitsMask == (ctrlQubitsMask & ((thisTask+chunkId*chunkSize) ^ ctrlFlipMask))) {
                // store current state vector values in temp variables
                stateRealUp = stateVecRealUp[thisTask];
                stateImagUp = stateVecImagUp[thisTask];

                stateRealLo = stateVecRealLo[thisTask];
                stateImagLo = stateVecImagLo[thisTask];

                stateVecRealOut[thisTask] = rot1Real*stateRealUp - rot1Imag*stateImagUp
                    + rot2Real*stateRealLo - rot2Imag*stateImagLo;
                stateVecImagOut[thisTask] = rot1Real*stateImagUp + rot1Imag*stateRealUp
                    + rot2Real*stateImagLo + rot2Imag*stateRealLo;
            }
        }
    }
}

void statevec_pauliXLocalSmall(Qureg qureg, const int targetQubit)
{
    long long int indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateImagUp;
    const long long int numTasks = (qureg.numAmpsPerChunk>>(1 + targetQubit)) ;
    const long long int sizeTask = (1LL << targetQubit);
    long long thisTask;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

# ifdef _OPENMP
    if(numTasks >= omp_get_num_threads()){
# pragma omp parallel \
    shared   (stateVecReal,stateVecImag) \
    private  (thisTask,indexUp,indexLo, stateRealUp,stateImagUp)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask = 0; thisTask < numTasks; ++thisTask)
        for (indexUp = thisTask * sizeTask * 2; indexUp < thisTask * sizeTask * 2 + sizeTask; ++ indexUp) {

            indexLo     = indexUp + sizeTask;

            stateRealUp = stateVecReal[indexUp];
            stateImagUp = stateVecImag[indexUp];

            stateVecReal[indexUp] = stateVecReal[indexLo];
            stateVecImag[indexUp] = stateVecImag[indexLo];

            stateVecReal[indexLo] = stateRealUp;
            stateVecImag[indexLo] = stateImagUp;
        }
    }
# ifdef _OPENMP
    }else {
        for (thisTask = 0; thisTask < numTasks; ++thisTask)

# pragma omp parallel \
    shared   (thisTask,stateVecReal,stateVecImag) \
    private  (indexUp,indexLo, stateRealUp,stateImagUp)
    {
# pragma omp for schedule (static)
        for (indexUp = thisTask * sizeTask * 2; indexUp < thisTask * sizeTask * 2 + sizeTask; ++ indexUp) {

            indexLo     = indexUp + sizeTask;

            stateRealUp = stateVecReal[indexUp];
            stateImagUp = stateVecImag[indexUp];

            stateVecReal[indexUp] = stateVecReal[indexLo];
            stateVecImag[indexUp] = stateVecImag[indexLo];

            stateVecReal[indexLo] = stateRealUp;
            stateVecImag[indexLo] = stateImagUp;
        }
    }
    }
# endif
}

void statevec_pauliXLocal(Qureg qureg, const int targetQubit)
{
    long long int sizeBlock, sizeHalfBlock;
    long long int thisBlock, // current block
         indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateImagUp;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk>>1;

    // set dimensions
    sizeHalfBlock = 1LL << targetQubit;
    sizeBlock     = 2LL * sizeHalfBlock;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag) \
    private  (thisTask,thisBlock ,indexUp,indexLo, stateRealUp,stateImagUp)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            thisBlock   = thisTask / sizeHalfBlock;
            indexUp     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
            indexLo     = indexUp + sizeHalfBlock;

            stateRealUp = stateVecReal[indexUp];
            stateImagUp = stateVecImag[indexUp];

            stateVecReal[indexUp] = stateVecReal[indexLo];
            stateVecImag[indexUp] = stateVecImag[indexLo];

            stateVecReal[indexLo] = stateRealUp;
            stateVecImag[indexLo] = stateImagUp;
        }
    }

}

/** Rotate a single qubit by {{0,1},{1,0}.
 *  Operate on a subset of the state vector with upper and lower block values
 *  stored seperately. This rotation is just swapping upper and lower values, and
 *  stateVecIn must already be the correct section for this chunk
 *
 *  @remarks Qubits are zero-based and the
 *  the first qubit is the rightmost
 *
 *  @param[in,out] qureg object representing the set of qubits
 *  @param[in] stateVecIn probability amplitudes in lower or upper half of a block depending on chunkId
 *  @param[out] stateVecOut array section to update (will correspond to either the lower or upper half of a block)
 */
void statevec_pauliXDistributed (Qureg qureg,
        ComplexArray stateVecIn,
        ComplexArray stateVecOut)
{

    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk;

    qreal *stateVecRealIn=stateVecIn.real, *stateVecImagIn=stateVecIn.imag;
    qreal *stateVecRealOut=stateVecOut.real, *stateVecImagOut=stateVecOut.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecRealIn,stateVecImagIn,stateVecRealOut,stateVecImagOut) \
    private  (thisTask)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            stateVecRealOut[thisTask] = stateVecRealIn[thisTask];
            stateVecImagOut[thisTask] = stateVecImagIn[thisTask];
        }
    }
}
void statevec_controlledNotLocalAll(Qureg qureg, const int controlQubit, const int targetQubit)
{
    long long int sizeBlock, sizeHalfBlock;
    long long int thisBlock, // current block
         indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateImagUp;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk>>1;
    
    // set dimensions
    sizeHalfBlock = 1LL << targetQubit;
    sizeBlock     = 2LL * sizeHalfBlock;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag) \
    private  (thisTask,thisBlock ,indexUp,indexLo, stateRealUp,stateImagUp)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            thisBlock   = thisTask / sizeHalfBlock;
            indexUp     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
            indexLo     = indexUp + sizeHalfBlock;
            stateRealUp = stateVecReal[indexUp];
            stateImagUp = stateVecImag[indexUp];

            stateVecReal[indexUp] = stateVecReal[indexLo];
            stateVecImag[indexUp] = stateVecImag[indexLo];

            stateVecReal[indexLo] = stateRealUp;
            stateVecImag[indexLo] = stateImagUp;
        }
    }
}
void statevec_controlledNotLocalSmall(Qureg qureg, const int controlQubit, const int targetQubit)
{
    long long int sizeBlock, sizeHalfBlock;
    long long int thisBlock, // current block
         indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateImagUp;
    if((1LL<<controlQubit)>=qureg.numAmpsPerChunk) {
        if(extractBit(controlQubit,qureg.chunkId*qureg.numAmpsPerChunk)) {
            statevec_controlledNotLocalAll(qureg,controlQubit,targetQubit);
        }
        return ;
    }
    long long int thisTask;
    const long long int numTasks = ((targetQubit > controlQubit) ? (1LL << (targetQubit - controlQubit - 1)) : (1LL << (controlQubit - targetQubit - 1)));
    // const long long int chunkSize=qureg.numAmpsPerChunk;
    // const long long int chunkId=qureg.chunkId;
    const long long int numBlocks = ((targetQubit > controlQubit) ? (qureg.numAmpsPerChunk>>(1 + targetQubit)) : (qureg.numAmpsPerChunk>>(1 + controlQubit)));
    const long long int sizeTask = ((targetQubit > controlQubit) ? (1LL << controlQubit) : (1LL << targetQubit));
    const long long int blockOffset = 1LL << controlQubit;

    sizeHalfBlock = 1LL << targetQubit;
    sizeBlock     = ((targetQubit > controlQubit) ? (2LL * sizeHalfBlock) : (2LL << controlQubit));


    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

# ifdef _OPENMP
    if(numBlocks >= omp_get_num_threads()) {
# pragma omp parallel \
    shared   (sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag,blockOffset) \
    private  (thisTask,thisBlock ,indexUp,indexLo, stateRealUp,stateImagUp)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisBlock = 0; thisBlock < numBlocks; ++thisBlock) {
            for(thisTask = 0; thisTask < numTasks; ++thisTask) {
                for(indexUp = thisBlock * sizeBlock + sizeTask * thisTask * 2 + blockOffset; indexUp < thisBlock * sizeBlock + sizeTask * thisTask * 2 + sizeTask + blockOffset; ++indexUp) {
                    indexLo     = indexUp + sizeHalfBlock;

                    stateRealUp = stateVecReal[indexUp];
                    stateImagUp = stateVecImag[indexUp];

                    stateVecReal[indexUp] = stateVecReal[indexLo];
                    stateVecImag[indexUp] = stateVecImag[indexLo];

                    stateVecReal[indexLo] = stateRealUp;
                    stateVecImag[indexLo] = stateImagUp;
                }
            }
        }
    }
# ifdef _OPENMP
    } else {
        for (thisBlock = 0; thisBlock < numBlocks; ++thisBlock) {
# pragma omp parallel \
    shared   (thisBlock,sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag,blockOffset) \
    private  (thisTask ,indexUp,indexLo, stateRealUp,stateImagUp)
    {
# pragma omp for schedule (static)
            for(thisTask = 0; thisTask < numTasks; ++thisTask) {
                for(indexUp = thisBlock * sizeBlock + sizeTask * thisTask * 2 + blockOffset; indexUp < thisBlock * sizeBlock + sizeTask * thisTask * 2 + sizeTask + blockOffset; ++indexUp) {
                    indexLo     = indexUp + sizeHalfBlock;

                    stateRealUp = stateVecReal[indexUp];
                    stateImagUp = stateVecImag[indexUp];

                    stateVecReal[indexUp] = stateVecReal[indexLo];
                    stateVecImag[indexUp] = stateVecImag[indexLo];

                    stateVecReal[indexLo] = stateRealUp;
                    stateVecImag[indexLo] = stateImagUp;
                }
            }
        }
    }
    }
#endif
}

void statevec_controlledNotLocal(Qureg qureg, const int controlQubit, const int targetQubit)
{
    long long int sizeBlock, sizeHalfBlock;
    long long int thisBlock, // current block
         indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateImagUp;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk>>1;
    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;

    int controlBit;

    // set dimensions
    sizeHalfBlock = 1LL << targetQubit;
    sizeBlock     = 2LL * sizeHalfBlock;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag) \
    private  (thisTask,thisBlock ,indexUp,indexLo, stateRealUp,stateImagUp,controlBit)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            thisBlock   = thisTask / sizeHalfBlock;
            indexUp     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
            indexLo     = indexUp + sizeHalfBlock;

            controlBit = extractBit(controlQubit, indexUp+chunkId*chunkSize);
            if (controlBit){
                stateRealUp = stateVecReal[indexUp];
                stateImagUp = stateVecImag[indexUp];

                stateVecReal[indexUp] = stateVecReal[indexLo];
                stateVecImag[indexUp] = stateVecImag[indexLo];

                stateVecReal[indexLo] = stateRealUp;
                stateVecImag[indexLo] = stateImagUp;
            }
        }
    }
}

/** Rotate a single qubit by {{0,1},{1,0}.
 *  Operate on a subset of the state vector with upper and lower block values
 *  stored seperately. This rotation is just swapping upper and lower values, and
 *  stateVecIn must already be the correct section for this chunk. Only perform the rotation
 *  for elements where controlQubit is one.
 *
 *  @param[in,out] qureg object representing the set of qubits
 *  @param[in] stateVecIn probability amplitudes in lower or upper half of a block depending on chunkId
 *  @param[out] stateVecOut array section to update (will correspond to either the lower or upper half of a block)
 */
void statevec_controlledNotDistributed (Qureg qureg, const int controlQubit,
        ComplexArray stateVecIn,
        ComplexArray stateVecOut)
{

    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk;
    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;

    int controlBit;

    qreal *stateVecRealIn=stateVecIn.real, *stateVecImagIn=stateVecIn.imag;
    qreal *stateVecRealOut=stateVecOut.real, *stateVecImagOut=stateVecOut.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecRealIn,stateVecImagIn,stateVecRealOut,stateVecImagOut) \
    private  (thisTask,controlBit)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            controlBit = extractBit (controlQubit, thisTask+chunkId*chunkSize);
            if (controlBit){
                stateVecRealOut[thisTask] = stateVecRealIn[thisTask];
                stateVecImagOut[thisTask] = stateVecImagIn[thisTask];
            }
        }
    }
}

void statevec_pauliYLocalSmall(Qureg qureg, const int targetQubit, const int conjFac)
{
    long long int indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateImagUp;
    const long long int numTasks = (qureg.numAmpsPerChunk>>(1 + targetQubit)) ;
    const long long int sizeTask = (1LL << targetQubit);
    long long thisTask;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecReal,stateVecImag) \
    private  (thisTask,indexUp,indexLo, stateRealUp,stateImagUp)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask = 0; thisTask < numTasks; ++thisTask)
        for (indexUp = thisTask * sizeTask * 2; indexUp < thisTask * sizeTask * 2 + sizeTask; ++ indexUp) {

            indexLo     = indexUp + sizeTask;

            stateRealUp = stateVecReal[indexUp];
            stateImagUp = stateVecImag[indexUp];

            stateVecReal[indexUp] = conjFac * stateVecImag[indexLo];
            stateVecImag[indexUp] = conjFac * -stateVecReal[indexLo];
            stateVecReal[indexLo] = conjFac * -stateImagUp;
            stateVecImag[indexLo] = conjFac * stateRealUp;
        }
    }
}

void statevec_pauliYLocal(Qureg qureg, const int targetQubit, const int conjFac)
{
    long long int sizeBlock, sizeHalfBlock;
    long long int thisBlock, // current block
         indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateImagUp;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk>>1;

    // set dimensions
    sizeHalfBlock = 1LL << targetQubit;
    sizeBlock     = 2LL * sizeHalfBlock;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag) \
    private  (thisTask,thisBlock ,indexUp,indexLo, stateRealUp,stateImagUp)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            thisBlock   = thisTask / sizeHalfBlock;
            indexUp     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
            indexLo     = indexUp + sizeHalfBlock;

            stateRealUp = stateVecReal[indexUp];
            stateImagUp = stateVecImag[indexUp];

            stateVecReal[indexUp] = conjFac * stateVecImag[indexLo];
            stateVecImag[indexUp] = conjFac * -stateVecReal[indexLo];
            stateVecReal[indexLo] = conjFac * -stateImagUp;
            stateVecImag[indexLo] = conjFac * stateRealUp;
        }
    }
}

/** Rotate a single qubit by +-{{0,-i},{i,0}.
 *  Operate on a subset of the state vector with upper and lower block values
 *  stored seperately. This rotation is just swapping upper and lower values, and
 *  stateVecIn must already be the correct section for this chunk
 *
 *  @remarks Qubits are zero-based and the
 *  the first qubit is the rightmost
 *
 *  @param[in,out] qureg object representing the set of qubits
 *  @param[in] stateVecIn probability amplitudes in lower or upper half of a block depending on chunkId
 *  @param[in] updateUpper flag, 1: updating upper values, 0: updating lower values in block
 *  @param[out] stateVecOut array section to update (will correspond to either the lower or upper half of a block)
 */
void statevec_pauliYDistributed(Qureg qureg,
        ComplexArray stateVecIn,
        ComplexArray stateVecOut,
        int updateUpper, const int conjFac)
{

    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk;

    qreal *stateVecRealIn=stateVecIn.real, *stateVecImagIn=stateVecIn.imag;
    qreal *stateVecRealOut=stateVecOut.real, *stateVecImagOut=stateVecOut.imag;

    int realSign=1, imagSign=1;
    if (updateUpper) imagSign=-1;
    else realSign = -1;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecRealIn,stateVecImagIn,stateVecRealOut,stateVecImagOut,realSign,imagSign) \
    private  (thisTask)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            stateVecRealOut[thisTask] = conjFac * realSign * stateVecImagIn[thisTask];
            stateVecImagOut[thisTask] = conjFac * imagSign * stateVecRealIn[thisTask];
        }
    }
}
void statevec_controlledPauliYLocalAll(Qureg qureg, const int controlQubit, const int targetQubit, const int conjFac) {
    long long int sizeBlock, sizeHalfBlock;
    long long int thisBlock, // current block
         indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateImagUp;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk>>1;

    // set dimensions
    sizeHalfBlock = 1LL << targetQubit;
    sizeBlock     = 2LL * sizeHalfBlock;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag) \
    private  (thisTask,thisBlock ,indexUp,indexLo, stateRealUp,stateImagUp)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            thisBlock   = thisTask / sizeHalfBlock;
            indexUp     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
            indexLo     = indexUp + sizeHalfBlock;

                stateRealUp = stateVecReal[indexUp];
                stateImagUp = stateVecImag[indexUp];

                // update under +-{{0, -i}, {i, 0}}
                stateVecReal[indexUp] = conjFac * stateVecImag[indexLo];
                stateVecImag[indexUp] = conjFac * -stateVecReal[indexLo];
                stateVecReal[indexLo] = conjFac * -stateImagUp;
                stateVecImag[indexLo] = conjFac * stateRealUp;
        }
    }
}

void statevec_controlledPauliYLocalSmall(Qureg qureg, const int controlQubit, const int targetQubit, const int conjFac)
{
    if((1LL<<controlQubit)>=qureg.numAmpsPerChunk) {
        if(extractBit(controlQubit,qureg.chunkId*qureg.numAmpsPerChunk)) {
            statevec_controlledPauliYLocalAll(qureg, controlQubit, targetQubit, conjFac);
        }
        return ;
    }
    long long int sizeBlock, sizeHalfBlock;
    long long int thisBlock, // current block
         indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateImagUp;
    long long int thisTask;
    const long long int numTasks = ((targetQubit > controlQubit) ? (1LL << (targetQubit - controlQubit - 1)) : (1LL << (controlQubit - targetQubit - 1)));
    // const long long int chunkSize=qureg.numAmpsPerChunk;
    // const long long int chunkId=qureg.chunkId;
    const long long int numBlocks = ((targetQubit > controlQubit) ? (qureg.numAmpsPerChunk>>(1 + targetQubit)) : (qureg.numAmpsPerChunk>>(1 + controlQubit)));
    const long long int sizeTask = ((targetQubit > controlQubit) ? (1LL << controlQubit) : (1LL << targetQubit));
    const long long int blockOffset = 1LL << controlQubit;

    // set dimensions
    sizeHalfBlock = 1LL << targetQubit;
    sizeBlock     = ((targetQubit > controlQubit) ? (2LL * sizeHalfBlock) : (2LL << controlQubit));


    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag, blockOffset) \
    private  (thisTask,thisBlock ,indexUp,indexLo, stateRealUp,stateImagUp)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisBlock = 0; thisBlock < numBlocks; ++thisBlock) {
            for(thisTask = 0; thisTask < numTasks; ++thisTask){
                for(indexUp = thisBlock * sizeBlock + sizeTask * thisTask * 2 + blockOffset; indexUp < thisBlock * sizeBlock + sizeTask * thisTask * 2 + sizeTask + blockOffset; ++indexUp) {
                    indexLo     = indexUp + sizeHalfBlock;

                    stateRealUp = stateVecReal[indexUp];
                    stateImagUp = stateVecImag[indexUp];

                    // update under +-{{0, -i}, {i, 0}}
                    stateVecReal[indexUp] = conjFac * stateVecImag[indexLo];
                    stateVecImag[indexUp] = conjFac * -stateVecReal[indexLo];
                    stateVecReal[indexLo] = conjFac * -stateImagUp;
                    stateVecImag[indexLo] = conjFac * stateRealUp;
                }
            }
        }
    }
}


void statevec_controlledPauliYLocal(Qureg qureg, const int controlQubit, const int targetQubit, const int conjFac)
{
    long long int sizeBlock, sizeHalfBlock;
    long long int thisBlock, // current block
         indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateImagUp;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk>>1;
    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;

    int controlBit;

    // set dimensions
    sizeHalfBlock = 1LL << targetQubit;
    sizeBlock     = 2LL * sizeHalfBlock;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag) \
    private  (thisTask,thisBlock ,indexUp,indexLo, stateRealUp,stateImagUp,controlBit)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            thisBlock   = thisTask / sizeHalfBlock;
            indexUp     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
            indexLo     = indexUp + sizeHalfBlock;

            controlBit = extractBit(controlQubit, indexUp+chunkId*chunkSize);
            if (controlBit){
                stateRealUp = stateVecReal[indexUp];
                stateImagUp = stateVecImag[indexUp];

                // update under +-{{0, -i}, {i, 0}}
                stateVecReal[indexUp] = conjFac * stateVecImag[indexLo];
                stateVecImag[indexUp] = conjFac * -stateVecReal[indexLo];
                stateVecReal[indexLo] = conjFac * -stateImagUp;
                stateVecImag[indexLo] = conjFac * stateRealUp;
            }
        }
    }
}


void statevec_controlledPauliYDistributed (Qureg qureg, const int controlQubit,
        ComplexArray stateVecIn,
        ComplexArray stateVecOut, const int conjFac)
{

    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk;
    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;

    int controlBit;

    qreal *stateVecRealIn=stateVecIn.real, *stateVecImagIn=stateVecIn.imag;
    qreal *stateVecRealOut=stateVecOut.real, *stateVecImagOut=stateVecOut.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecRealIn,stateVecImagIn,stateVecRealOut,stateVecImagOut) \
    private  (thisTask,controlBit)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            controlBit = extractBit (controlQubit, thisTask+chunkId*chunkSize);
            if (controlBit){
                stateVecRealOut[thisTask] = conjFac * stateVecImagIn[thisTask];
                stateVecImagOut[thisTask] = conjFac * -stateVecRealIn[thisTask];
            }
        }
    }
}

void statevec_hadamardLocalSmall(Qureg qureg, const int targetQubit)
{
    long long int indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateRealLo,stateImagUp,stateImagLo;
    const long long int sizeTask = (1LL << targetQubit);
    if(sizeTask >= 4){
        statevec_hadamardLocalSIMD(qureg,targetQubit);
        //printf("simd");
        return;
    }

    const long long int numTasks = (qureg.numAmpsPerChunk>>(1 + targetQubit)) ;
    long long thisTask;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    qreal recRoot2 = 1.0/sqrt(2);

# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecReal,stateVecImag, recRoot2) \
    private  (thisTask,indexUp,indexLo, stateRealUp,stateImagUp,stateRealLo,stateImagLo)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask = 0; thisTask < numTasks; ++thisTask)
        for (indexUp = thisTask * sizeTask * 2; indexUp < thisTask * sizeTask * 2 + sizeTask; ++ indexUp) {

            indexLo     = indexUp + sizeTask;

            stateRealUp = stateVecReal[indexUp];
            stateImagUp = stateVecImag[indexUp];

            stateRealLo = stateVecReal[indexLo];
            stateImagLo = stateVecImag[indexLo];

            stateVecReal[indexUp] = recRoot2*(stateRealUp + stateRealLo);
            stateVecImag[indexUp] = recRoot2*(stateImagUp + stateImagLo);

            stateVecReal[indexLo] = recRoot2*(stateRealUp - stateRealLo);
            stateVecImag[indexLo] = recRoot2*(stateImagUp - stateImagLo);
        }
    }
}

void statevec_hadamardLocalSIMD(Qureg qureg, const int targetQubit)
{
    long long int indexUp,indexLo;    // current index and corresponding index in lower half block

    __m256d stateRealUpSIMD,stateRealLoSIMD,stateImagUpSIMD,stateImagLoSIMD;
    const long long int numTasks = (qureg.numAmpsPerChunk>>(1 + targetQubit)) ;
    const long long int sizeTask = (1LL << targetQubit);
    long long thisTask;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    qreal recRoot2 = 1.0/sqrt(2);
    register const __m256d recRoot2SIMD = _mm256_set1_pd(recRoot2);

# ifdef _OPENMP
    if(numTasks >= omp_get_num_threads()){
# pragma omp parallel \
    shared   (stateVecReal,stateVecImag, recRoot2SIMD) \
    private  (thisTask,indexUp,indexLo, stateRealUpSIMD,stateImagUpSIMD,stateRealLoSIMD,stateImagLoSIMD)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask = 0; thisTask < numTasks; ++thisTask)
        for (indexUp = thisTask * sizeTask * 2; indexUp < thisTask * sizeTask * 2 + sizeTask; indexUp+=4) {

            indexLo     = indexUp + sizeTask;

            stateRealUpSIMD = _mm256_loadu_pd(stateVecReal+indexUp);
            stateImagUpSIMD = _mm256_loadu_pd(stateVecImag+indexUp);
            stateRealLoSIMD = _mm256_loadu_pd(stateVecReal+indexLo);
            stateImagLoSIMD = _mm256_loadu_pd(stateVecImag+indexLo);

            _mm256_storeu_pd(stateVecReal+indexUp, _mm256_mul_pd(recRoot2SIMD,_mm256_add_pd(stateRealUpSIMD, stateRealLoSIMD)));
            _mm256_storeu_pd(stateVecImag+indexUp, _mm256_mul_pd(recRoot2SIMD,_mm256_add_pd(stateImagUpSIMD, stateImagLoSIMD)));
            _mm256_storeu_pd(stateVecReal+indexLo, _mm256_mul_pd(recRoot2SIMD,_mm256_sub_pd(stateRealUpSIMD, stateRealLoSIMD)));
            _mm256_storeu_pd(stateVecImag+indexLo, _mm256_mul_pd(recRoot2SIMD,_mm256_sub_pd(stateImagUpSIMD, stateImagLoSIMD)));
        }
    }
    
# ifdef _OPENMP
    } else {
        for (thisTask = 0; thisTask < numTasks; ++thisTask)
# pragma omp parallel \
    shared   (thisTask,stateVecReal,stateVecImag, recRoot2SIMD) \
    private  (indexUp,indexLo, stateRealUpSIMD,stateImagUpSIMD,stateRealLoSIMD,stateImagLoSIMD)
    {
# pragma omp for schedule (static)
        for (indexUp = thisTask * sizeTask * 2; indexUp < thisTask * sizeTask * 2 + sizeTask; indexUp+=4) {

            indexLo     = indexUp + sizeTask;

            stateRealUpSIMD = _mm256_loadu_pd(stateVecReal+indexUp);
            stateImagUpSIMD = _mm256_loadu_pd(stateVecImag+indexUp);
            stateRealLoSIMD = _mm256_loadu_pd(stateVecReal+indexLo);
            stateImagLoSIMD = _mm256_loadu_pd(stateVecImag+indexLo);

            _mm256_storeu_pd(stateVecReal+indexUp, _mm256_mul_pd(recRoot2SIMD,_mm256_add_pd(stateRealUpSIMD, stateRealLoSIMD)));
            _mm256_storeu_pd(stateVecImag+indexUp, _mm256_mul_pd(recRoot2SIMD,_mm256_add_pd(stateImagUpSIMD, stateImagLoSIMD)));
            _mm256_storeu_pd(stateVecReal+indexLo, _mm256_mul_pd(recRoot2SIMD,_mm256_sub_pd(stateRealUpSIMD, stateRealLoSIMD)));
            _mm256_storeu_pd(stateVecImag+indexLo, _mm256_mul_pd(recRoot2SIMD,_mm256_sub_pd(stateImagUpSIMD, stateImagLoSIMD)));
        }
    }
    }
# endif

}

void statevec_hadamardLocal(Qureg qureg, const int targetQubit)
{
    long long int sizeBlock, sizeHalfBlock;
    long long int thisBlock, // current block
         indexUp,indexLo;    // current index and corresponding index in lower half block

    qreal stateRealUp,stateRealLo,stateImagUp,stateImagLo;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk>>1;

    // set dimensions
    sizeHalfBlock = 1LL << targetQubit;
    sizeBlock     = 2LL * sizeHalfBlock;

    // Can't use qureg.stateVec as a private OMP var
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    qreal recRoot2 = 1.0/sqrt(2);

# ifdef _OPENMP
# pragma omp parallel \
    shared   (sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag, recRoot2) \
    private  (thisTask,thisBlock ,indexUp,indexLo, stateRealUp,stateImagUp,stateRealLo,stateImagLo)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            thisBlock   = thisTask / sizeHalfBlock;
            indexUp     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
            indexLo     = indexUp + sizeHalfBlock;

            stateRealUp = stateVecReal[indexUp];
            stateImagUp = stateVecImag[indexUp];

            stateRealLo = stateVecReal[indexLo];
            stateImagLo = stateVecImag[indexLo];

            stateVecReal[indexUp] = recRoot2*(stateRealUp + stateRealLo);
            stateVecImag[indexUp] = recRoot2*(stateImagUp + stateImagLo);

            stateVecReal[indexLo] = recRoot2*(stateRealUp - stateRealLo);
            stateVecImag[indexLo] = recRoot2*(stateImagUp - stateImagLo);
        }
    }
}

/** Rotate a single qubit by {{1,1},{1,-1}}/sqrt2.
 *  Operate on a subset of the state vector with upper and lower block values
 *  stored seperately. This rotation is just swapping upper and lower values, and
 *  stateVecIn must already be the correct section for this chunk
 *
 *  @param[in,out] qureg object representing the set of qubits
 *  @param[in] stateVecIn probability amplitudes in lower or upper half of a block depending on chunkId
 *  @param[in] updateUpper flag, 1: updating upper values, 0: updating lower values in block
 *  @param[out] stateVecOut array section to update (will correspond to either the lower or upper half of a block)
 */
void statevec_hadamardDistributed(Qureg qureg,
        ComplexArray stateVecUp,
        ComplexArray stateVecLo,
        ComplexArray stateVecOut,
        int updateUpper)
{

    qreal   stateRealUp,stateRealLo,stateImagUp,stateImagLo;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk;

    if(numTasks >= 4){
        statevec_hadamardDistributedSIMD(qureg,stateVecUp,stateVecLo,stateVecOut,updateUpper);
        return;
    }

    int sign;
    if (updateUpper) sign=1;
    else sign=-1;

    qreal recRoot2 = 1.0/sqrt(2);

    qreal *stateVecRealUp=stateVecUp.real, *stateVecImagUp=stateVecUp.imag;
    qreal *stateVecRealLo=stateVecLo.real, *stateVecImagLo=stateVecLo.imag;
    qreal *stateVecRealOut=stateVecOut.real, *stateVecImagOut=stateVecOut.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecRealUp,stateVecImagUp,stateVecRealLo,stateVecImagLo,stateVecRealOut,stateVecImagOut, \
            recRoot2, sign) \
    private  (thisTask,stateRealUp,stateImagUp,stateRealLo,stateImagLo)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            // store current state vector values in temp variables
            stateRealUp = stateVecRealUp[thisTask];
            stateImagUp = stateVecImagUp[thisTask];

            stateRealLo = stateVecRealLo[thisTask];
            stateImagLo = stateVecImagLo[thisTask];

            stateVecRealOut[thisTask] = recRoot2*(stateRealUp + sign*stateRealLo);
            stateVecImagOut[thisTask] = recRoot2*(stateImagUp + sign*stateImagLo);
        }
    }
}

void statevec_hadamardDistributedSIMD(Qureg qureg,
        ComplexArray stateVecUp,
        ComplexArray stateVecLo,
        ComplexArray stateVecOut,
        int updateUpper)
{

    __m256d   stateRealUpSIMD,stateRealLoSIMD,stateImagUpSIMD,stateImagLoSIMD;
    long long int thisTask;
    const long long int numTasks=qureg.numAmpsPerChunk;

    int sign;
    if (updateUpper) sign=1;
    else sign=-1;

    qreal recRoot2 = 1.0/sqrt(2);
    register const __m256d recRoot2SIMD = _mm256_set1_pd(recRoot2);
    register const __m256d signSIMD = _mm256_set1_pd(sign);

    qreal *stateVecRealUp=stateVecUp.real, *stateVecImagUp=stateVecUp.imag;
    qreal *stateVecRealLo=stateVecLo.real, *stateVecImagLo=stateVecLo.imag;
    qreal *stateVecRealOut=stateVecOut.real, *stateVecImagOut=stateVecOut.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecRealUp,stateVecImagUp,stateVecRealLo,stateVecImagLo,stateVecRealOut,stateVecImagOut, \
            recRoot2SIMD, signSIMD) \
    private  (thisTask,stateRealUpSIMD,stateImagUpSIMD,stateRealLoSIMD,stateImagLoSIMD)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask+=4) {
            // store current state vector values in temp variables
            stateRealUpSIMD = _mm256_loadu_pd(stateVecRealUp+thisTask);
            stateImagUpSIMD = _mm256_loadu_pd(stateVecImagUp+thisTask);
            stateRealLoSIMD = _mm256_loadu_pd(stateVecRealLo+thisTask);
            stateImagLoSIMD = _mm256_loadu_pd(stateVecImagLo+thisTask);

            _mm256_storeu_pd(stateVecRealOut+thisTask, _mm256_mul_pd(recRoot2SIMD,_mm256_add_pd(stateRealUpSIMD,_mm256_mul_pd(signSIMD,stateRealLoSIMD))));
            _mm256_storeu_pd(stateVecImagOut+thisTask, _mm256_mul_pd(recRoot2SIMD,_mm256_add_pd(stateImagUpSIMD,_mm256_mul_pd(signSIMD,stateImagLoSIMD))));
        }
    }
}
void statevec_phaseShiftByTermAllSIMD (Qureg qureg, const int targetQubit, Complex term)
{
    long long int index;

    // const long long int chunkSize=qureg.numAmpsPerChunk;
    // const long long int chunkId=qureg.chunkId;

    // dimension of the state vector
    long long int stateVecSize = qureg.numAmpsPerChunk;

    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    __m256d stateRealLoSIMD, stateImagLoSIMD;
    register const __m256d cosAngleSIMD = _mm256_set1_pd(term.real);
    register const __m256d sinAngleSIMD = _mm256_set1_pd(term.imag);

# ifdef _OPENMP
# pragma omp parallel for \
    shared   (stateVecReal,stateVecImag ) \
    private  (index,stateRealLoSIMD,stateImagLoSIMD)             \
    schedule (static)
# endif
    for(index=0; index<stateVecSize; index+=4) {

        // update the coeff of the |1> state of the target qubit
        // targetBit = extractBit (targetQubit, index+chunkId*chunkSize);
        // if (targetBit) {

            stateRealLoSIMD = _mm256_loadu_pd(stateVecReal+index);
            stateImagLoSIMD = _mm256_loadu_pd(stateVecImag+index);

            _mm256_storeu_pd(stateVecReal+index, _mm256_sub_pd(\
                            _mm256_mul_pd(cosAngleSIMD,stateRealLoSIMD),\
                            _mm256_mul_pd(sinAngleSIMD,stateImagLoSIMD)));
            //stateVecReal[index] = cosAngle*stateRealLo - sinAngle*stateImagLo;

            _mm256_storeu_pd(stateVecImag+index, _mm256_add_pd(\
                            _mm256_mul_pd(sinAngleSIMD,stateRealLoSIMD),\
                            _mm256_mul_pd(cosAngleSIMD,stateImagLoSIMD)));
            //stateVecImag[index] = sinAngle*stateRealLo + cosAngle*stateImagLo;
        // }
    }
}


void statevec_phaseShiftByTermAll (Qureg qureg, const int targetQubit, Complex term)
{
    long long int index;
    long long int stateVecSize;

    // dimension of the state vector
    stateVecSize = qureg.numAmpsPerChunk;
    if(stateVecSize>=4) {
        statevec_phaseShiftByTermAllSIMD(qureg,targetQubit,term);
        return ;
    }

    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    qreal stateRealLo, stateImagLo;
    const qreal cosAngle = term.real;
    const qreal sinAngle = term.imag;

# ifdef _OPENMP
# pragma omp parallel for \
    shared   (stateVecSize, stateVecReal,stateVecImag ) \
    private  (index,stateRealLo,stateImagLo)             \
    schedule (static)
# endif
    for (index=0; index<stateVecSize; index++) {

        // update the coeff of the |1> state of the target qubit
            stateRealLo = stateVecReal[index];
            stateImagLo = stateVecImag[index];

            stateVecReal[index] = cosAngle*stateRealLo - sinAngle*stateImagLo;
            stateVecImag[index] = sinAngle*stateRealLo + cosAngle*stateImagLo;
    }
}

void statevec_phaseShiftByTermSmall (Qureg qureg, const int targetQubit, Complex term)
{
    long long int index;

    // const long long int chunkSize=qureg.numAmpsPerChunk;
    // const long long int chunkId=qureg.chunkId;
    if((1LL<<targetQubit)>=qureg.numAmpsPerChunk) {
        if(extractBit(targetQubit,qureg.chunkId*qureg.numAmpsPerChunk)) {
            statevec_phaseShiftByTermAll(qureg,targetQubit,term);
        }
        return ;
    }
    // dimension of the state vector
    const long long int sizeTask = (1LL << targetQubit);
    if(sizeTask >= 4){
        statevec_phaseShiftByTermSIMD(qureg,targetQubit,term);
        return;
    }

    const long long int numTasks = (qureg.numAmpsPerChunk>>(1 + targetQubit)) ;
    long long thisTask;

    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    qreal stateRealLo, stateImagLo;
    const qreal cosAngle = term.real;
    const qreal sinAngle = term.imag;

# ifdef _OPENMP
# pragma omp parallel for \
    shared   (stateVecReal,stateVecImag ) \
    private  (index,stateRealLo,stateImagLo)             \
    schedule (static)
# endif
    for(thisTask = 0; thisTask < numTasks; ++thisTask)
    for(index = sizeTask * thisTask * 2 + sizeTask; index < sizeTask * (thisTask + 1) * 2; ++index) {

        // update the coeff of the |1> state of the target qubit
        // targetBit = extractBit (targetQubit, index+chunkId*chunkSize);
        // if (targetBit) {

            stateRealLo = stateVecReal[index];
            stateImagLo = stateVecImag[index];

            stateVecReal[index] = cosAngle*stateRealLo - sinAngle*stateImagLo;
            stateVecImag[index] = sinAngle*stateRealLo + cosAngle*stateImagLo;
        // }
    }
}

void statevec_phaseShiftByTermSIMD (Qureg qureg, const int targetQubit, Complex term)
{
    long long int index;

    // const long long int chunkSize=qureg.numAmpsPerChunk;
    // const long long int chunkId=qureg.chunkId;

    // dimension of the state vector
    const long long int numTasks = (qureg.numAmpsPerChunk>>(1 + targetQubit)) ;
    const long long int sizeTask = (1LL << targetQubit);
    long long thisTask;

    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    __m256d stateRealLoSIMD, stateImagLoSIMD;
    register const __m256d cosAngleSIMD = _mm256_set1_pd(term.real);
    register const __m256d sinAngleSIMD = _mm256_set1_pd(term.imag);

# ifdef _OPENMP
    if(numTasks >= omp_get_num_threads()) {
# pragma omp parallel for \
    shared   (stateVecReal,stateVecImag ) \
    private  (index,stateRealLoSIMD,stateImagLoSIMD)             \
    schedule (static)
# endif
    for(thisTask = 0; thisTask < numTasks; ++thisTask)
    for(index = sizeTask * thisTask * 2 + sizeTask; index < sizeTask * (thisTask + 1) * 2; index+=4) {

        // update the coeff of the |1> state of the target qubit
        // targetBit = extractBit (targetQubit, index+chunkId*chunkSize);
        // if (targetBit) {

            stateRealLoSIMD = _mm256_loadu_pd(stateVecReal+index);
            stateImagLoSIMD = _mm256_loadu_pd(stateVecImag+index);

            _mm256_storeu_pd(stateVecReal+index, _mm256_sub_pd(\
                            _mm256_mul_pd(cosAngleSIMD,stateRealLoSIMD),\
                            _mm256_mul_pd(sinAngleSIMD,stateImagLoSIMD)));
            //stateVecReal[index] = cosAngle*stateRealLo - sinAngle*stateImagLo;

            _mm256_storeu_pd(stateVecImag+index, _mm256_add_pd(\
                            _mm256_mul_pd(sinAngleSIMD,stateRealLoSIMD),\
                            _mm256_mul_pd(cosAngleSIMD,stateImagLoSIMD)));
            //stateVecImag[index] = sinAngle*stateRealLo + cosAngle*stateImagLo;
        // }
    }
    
# ifdef _OPENMP
   } else {
        for(thisTask = 0; thisTask < numTasks; ++thisTask)

# pragma omp parallel for \
    shared   (stateVecReal,stateVecImag ) \
    private  (index,stateRealLoSIMD,stateImagLoSIMD)             \
    schedule (static)
    for(index = sizeTask * thisTask * 2 + sizeTask; index < sizeTask * (thisTask + 1) * 2; index+=4) {

        // update the coeff of the |1> state of the target qubit
        // targetBit = extractBit (targetQubit, index+chunkId*chunkSize);
        // if (targetBit) {

            stateRealLoSIMD = _mm256_loadu_pd(stateVecReal+index);
            stateImagLoSIMD = _mm256_loadu_pd(stateVecImag+index);

            _mm256_storeu_pd(stateVecReal+index, _mm256_sub_pd(\
                            _mm256_mul_pd(cosAngleSIMD,stateRealLoSIMD),\
                            _mm256_mul_pd(sinAngleSIMD,stateImagLoSIMD)));
            //stateVecReal[index] = cosAngle*stateRealLo - sinAngle*stateImagLo;

            _mm256_storeu_pd(stateVecImag+index, _mm256_add_pd(\
                            _mm256_mul_pd(sinAngleSIMD,stateRealLoSIMD),\
                            _mm256_mul_pd(cosAngleSIMD,stateImagLoSIMD)));
            //stateVecImag[index] = sinAngle*stateRealLo + cosAngle*stateImagLo;
        // }
    }
   }
# endif

}

void statevec_phaseShiftByTerm (Qureg qureg, const int targetQubit, Complex term)
{
    long long int index;
    long long int stateVecSize;
    int targetBit;

    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;

    // dimension of the state vector
    stateVecSize = qureg.numAmpsPerChunk;
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    qreal stateRealLo, stateImagLo;
    const qreal cosAngle = term.real;
    const qreal sinAngle = term.imag;

# ifdef _OPENMP
# pragma omp parallel for \
    shared   (stateVecSize, stateVecReal,stateVecImag ) \
    private  (index,targetBit,stateRealLo,stateImagLo)             \
    schedule (static)
# endif
    for (index=0; index<stateVecSize; index++) {

        // update the coeff of the |1> state of the target qubit
        targetBit = extractBit (targetQubit, index+chunkId*chunkSize);
        if (targetBit) {

            stateRealLo = stateVecReal[index];
            stateImagLo = stateVecImag[index];

            stateVecReal[index] = cosAngle*stateRealLo - sinAngle*stateImagLo;
            stateVecImag[index] = sinAngle*stateRealLo + cosAngle*stateImagLo;
        }
    }
}

void statevec_controlledPhaseShift (Qureg qureg, const int idQubit1, const int idQubit2, qreal angle)
{
    long long int index;
    long long int stateVecSize;
    int bit1, bit2;

    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;

    // dimension of the state vector
    stateVecSize = qureg.numAmpsPerChunk;
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    qreal stateRealLo, stateImagLo;
    const qreal cosAngle = cos(angle);
    const qreal sinAngle = sin(angle);

# ifdef _OPENMP
# pragma omp parallel for \
    shared   (stateVecSize, stateVecReal,stateVecImag ) \
    private  (index,bit1,bit2,stateRealLo,stateImagLo)             \
    schedule (static)
# endif
    for (index=0; index<stateVecSize; index++) {
        bit1 = extractBit (idQubit1, index+chunkId*chunkSize);
        bit2 = extractBit (idQubit2, index+chunkId*chunkSize);
        if (bit1 && bit2) {

            stateRealLo = stateVecReal[index];
            stateImagLo = stateVecImag[index];

            stateVecReal[index] = cosAngle*stateRealLo - sinAngle*stateImagLo;
            stateVecImag[index] = sinAngle*stateRealLo + cosAngle*stateImagLo;
        }
    }
}

void statevec_multiControlledPhaseShift(Qureg qureg, int *controlQubits, int numControlQubits, qreal angle)
{
    long long int index;
    long long int stateVecSize;

    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;

    long long int mask = getQubitBitMask(controlQubits, numControlQubits);

    stateVecSize = qureg.numAmpsPerChunk;
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    qreal stateRealLo, stateImagLo;
    const qreal cosAngle = cos(angle);
    const qreal sinAngle = sin(angle);

# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecSize, stateVecReal, stateVecImag, mask) \
    private  (index, stateRealLo, stateImagLo)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (index=0; index<stateVecSize; index++) {
            if (mask == (mask & (index+chunkId*chunkSize)) ){

                stateRealLo = stateVecReal[index];
                stateImagLo = stateVecImag[index];

                stateVecReal[index] = cosAngle*stateRealLo - sinAngle*stateImagLo;
                stateVecImag[index] = sinAngle*stateRealLo + cosAngle*stateImagLo;
            }
        }
    }
}

int getBitMaskParity(long long int mask) {
    int parity = 0;
    while (mask) {
        parity = !parity;
        mask = mask & (mask-1);
    }
    return parity;
}

void statevec_multiRotateZ(Qureg qureg, long long int mask, qreal angle)
{
    long long int index;
    long long int stateVecSize;

    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;

    stateVecSize = qureg.numAmpsPerChunk;
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    qreal stateReal, stateImag;
    const qreal cosAngle = cos(angle/2.0);
    const qreal sinAngle = sin(angle/2.0);

    // = +-1, to flip sinAngle based on target qubit parity, to effect
    // exp(-angle/2 i fac_j)|j>
    int fac;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecSize, stateVecReal, stateVecImag, mask) \
    private  (index, fac, stateReal, stateImag)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (index=0; index<stateVecSize; index++) {
            stateReal = stateVecReal[index];
            stateImag = stateVecImag[index];

            // odd-parity target qubits get fac_j = -1
            fac = getBitMaskParity(mask & (index+chunkId*chunkSize))? -1 : 1;
            stateVecReal[index] = cosAngle*stateReal + fac * sinAngle*stateImag;
            stateVecImag[index] = - fac * sinAngle*stateReal + cosAngle*stateImag;
        }
    }
}

qreal densmatr_findProbabilityOfZeroLocal(Qureg qureg, const int measureQubit) {

    // computes first local index containing a diagonal element
    long long int localNumAmps = qureg.numAmpsPerChunk;
    long long int densityDim = (1LL << qureg.numQubitsRepresented);
    long long int diagSpacing = 1LL + densityDim;
    long long int maxNumDiagsPerChunk = 1 + localNumAmps / diagSpacing;
    long long int numPrevDiags = (qureg.chunkId>0)? 1+(qureg.chunkId*localNumAmps)/diagSpacing : 0;
    long long int globalIndNextDiag = diagSpacing * numPrevDiags;
    long long int localIndNextDiag = globalIndNextDiag % localNumAmps;

    // computes how many diagonals are contained in this chunk
    long long int numDiagsInThisChunk = maxNumDiagsPerChunk;
    if (localIndNextDiag + (numDiagsInThisChunk-1)*diagSpacing >= localNumAmps)
        numDiagsInThisChunk -= 1;

    long long int visitedDiags;     // number of visited diagonals in this chunk so far
    long long int basisStateInd;    // current diagonal index being considered
    long long int index;            // index in the local chunk

    qreal zeroProb = 0;
    qreal *stateVecReal = qureg.stateVec.real;

# ifdef _OPENMP
# pragma omp parallel \
    shared    (localIndNextDiag, numPrevDiags, diagSpacing, stateVecReal, numDiagsInThisChunk) \
    private   (visitedDiags, basisStateInd, index) \
    reduction ( +:zeroProb )
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule  (static)
# endif
        // sums the diagonal elems of the density matrix where measureQubit=0
        for (visitedDiags = 0; visitedDiags < numDiagsInThisChunk; visitedDiags++) {

            basisStateInd = numPrevDiags + visitedDiags;
            index = localIndNextDiag + diagSpacing * visitedDiags;

            if (extractBit(measureQubit, basisStateInd) == 0)
                zeroProb += stateVecReal[index]; // assume imag[diagonls] ~ 0

        }
    }

    return zeroProb;
}

/** Measure the total probability of a specified qubit being in the zero state across all amplitudes in this chunk.
 *  Size of regions to skip is less than the size of one chunk.
 *
 *  @param[in] qureg object representing the set of qubits
 *  @param[in] measureQubit qubit to measure
 *  @return probability of qubit measureQubit being zero
 */
qreal statevec_findProbabilityOfZeroLocal (Qureg qureg,
        const int measureQubit)
{
    // ----- sizes
    long long int sizeBlock,                                  // size of blocks
         sizeHalfBlock;                                       // size of blocks halved
    // ----- indices
    long long int thisBlock,                                  // current block
         index;                                               // current index for first half block
    // ----- measured probability
    qreal   totalProbability;                                  // probability (returned) value
    // ----- temp variables
    long long int thisTask;
    long long int numTasks=qureg.numAmpsPerChunk>>1;

    // ---------------------------------------------------------------- //
    //            dimensions                                            //
    // ---------------------------------------------------------------- //
    sizeHalfBlock = 1LL << (measureQubit);                       // number of state vector elements to sum,
    // and then the number to skip
    sizeBlock     = 2LL * sizeHalfBlock;                         // size of blocks (pairs of measure and skip entries)

    // initialise returned value
    totalProbability = 0.0;

    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared    (numTasks,sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag) \
    private   (thisTask,thisBlock,index) \
    reduction ( +:totalProbability )
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule  (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            thisBlock = thisTask / sizeHalfBlock;
            index     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;

            totalProbability += stateVecReal[index]*stateVecReal[index]
                + stateVecImag[index]*stateVecImag[index];
        }
    }
    return totalProbability;
}

/** Measure the probability of a specified qubit being in the zero state across all amplitudes held in this chunk.
 * Size of regions to skip is a multiple of chunkSize.
 * The results are communicated and aggregated by the caller
 *
 *  @param[in] qureg object representing the set of qubits
 *  @return probability of qubit measureQubit being zero
 */
qreal statevec_findProbabilityOfZeroDistributed (Qureg qureg) {
    // ----- measured probability
    qreal   totalProbability;                                  // probability (returned) value
    // ----- temp variables
    long long int thisTask;                                   // task based approach for expose loop with small granularity
    long long int numTasks=qureg.numAmpsPerChunk;

    // ---------------------------------------------------------------- //
    //            find probability                                      //
    // ---------------------------------------------------------------- //

    // initialise returned value
    totalProbability = 0.0;

    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared    (numTasks,stateVecReal,stateVecImag) \
    private   (thisTask) \
    reduction ( +:totalProbability )
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule  (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            totalProbability += stateVecReal[thisTask]*stateVecReal[thisTask]
                + stateVecImag[thisTask]*stateVecImag[thisTask];
        }
    }

    return totalProbability;
}



void statevec_controlledPhaseFlip (Qureg qureg, const int idQubit1, const int idQubit2)
{
    long long int index;
    long long int stateVecSize;
    int bit1, bit2;

    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;

    // dimension of the state vector
    stateVecSize = qureg.numAmpsPerChunk;
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

# ifdef _OPENMP
# pragma omp parallel for \
    shared   (stateVecSize, stateVecReal,stateVecImag ) \
    private  (index,bit1,bit2)             \
    schedule (static)
# endif
    for (index=0; index<stateVecSize; index++) {
        bit1 = extractBit (idQubit1, index+chunkId*chunkSize);
        bit2 = extractBit (idQubit2, index+chunkId*chunkSize);
        if (bit1 && bit2) {
            stateVecReal [index] = - stateVecReal [index];
            stateVecImag [index] = - stateVecImag [index];
        }
    }
}

void statevec_multiControlledPhaseFlip(Qureg qureg, int *controlQubits, int numControlQubits)
{
    long long int index;
    long long int stateVecSize;

    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;

    long long int mask = getQubitBitMask(controlQubits, numControlQubits);

    stateVecSize = qureg.numAmpsPerChunk;
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (stateVecSize, stateVecReal,stateVecImag, mask ) \
    private  (index)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (index=0; index<stateVecSize; index++) {
            if (mask == (mask & (index+chunkId*chunkSize)) ){
                stateVecReal [index] = - stateVecReal [index];
                stateVecImag [index] = - stateVecImag [index];
            }
        }
    }
}

/** Update the state vector to be consistent with measuring measureQubit=0 if outcome=0 and measureQubit=1
 *  if outcome=1.
 *  Performs an irreversible change to the state vector: it updates the vector according
 *  to the event that an outcome have been measured on the qubit indicated by measureQubit (where
 *  this label starts from 0, of course). It achieves this by setting all inconsistent
 *  amplitudes to 0 and
 *  then renormalising based on the total probability of measuring measureQubit=0 or 1 according to the
 *  value of outcome.
 *  In the local version, one or more blocks (with measureQubit=0 in the first half of the block and
 *  measureQubit=1 in the second half of the block) fit entirely into one chunk.
 *
 *  @param[in,out] qureg object representing the set of qubits
 *  @param[in] measureQubit qubit to measure
 *  @param[in] totalProbability probability of qubit measureQubit being either zero or one
 *  @param[in] outcome to measure the probability of and set the state to -- either zero or one
 */
void statevec_collapseToKnownProbOutcomeLocal(Qureg qureg, int measureQubit, int outcome, qreal totalProbability)
{
    // ----- sizes
    long long int sizeBlock,                                  // size of blocks
         sizeHalfBlock;                                       // size of blocks halved
    // ----- indices
    long long int thisBlock,                                  // current block
         index;                                               // current index for first half block
    // ----- measured probability
    qreal   renorm;                                            // probability (returned) value
    // ----- temp variables
    long long int thisTask;                                   // task based approach for expose loop with small granularity
    // (good for shared memory parallelism)
    long long int numTasks=qureg.numAmpsPerChunk>>1;

    // ---------------------------------------------------------------- //
    //            dimensions                                            //
    // ---------------------------------------------------------------- //
    sizeHalfBlock = 1LL << (measureQubit);                       // number of state vector elements to sum,
    // and then the number to skip
    sizeBlock     = 2LL * sizeHalfBlock;                         // size of blocks (pairs of measure and skip entries)

    renorm=1/sqrt(totalProbability);
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;


# ifdef _OPENMP
# pragma omp parallel \
    shared    (numTasks,sizeBlock,sizeHalfBlock, stateVecReal,stateVecImag,renorm,outcome) \
    private   (thisTask,thisBlock,index)
# endif
    {
        if (outcome==0){
            // measure qubit is 0
# ifdef _OPENMP
# pragma omp for schedule  (static)
# endif
            for (thisTask=0; thisTask<numTasks; thisTask++) {
                thisBlock = thisTask / sizeHalfBlock;
                index     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
                stateVecReal[index]=stateVecReal[index]*renorm;
                stateVecImag[index]=stateVecImag[index]*renorm;

                stateVecReal[index+sizeHalfBlock]=0;
                stateVecImag[index+sizeHalfBlock]=0;
            }
        } else {
            // measure qubit is 1
# ifdef _OPENMP
# pragma omp for schedule  (static)
# endif
            for (thisTask=0; thisTask<numTasks; thisTask++) {
                thisBlock = thisTask / sizeHalfBlock;
                index     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
                stateVecReal[index]=0;
                stateVecImag[index]=0;

                stateVecReal[index+sizeHalfBlock]=stateVecReal[index+sizeHalfBlock]*renorm;
                stateVecImag[index+sizeHalfBlock]=stateVecImag[index+sizeHalfBlock]*renorm;
            }
        }
    }

}

/** Renormalise parts of the state vector where measureQubit=0 or 1, based on the total probability of that qubit being
 *  in state 0 or 1.
 *  Measure in Zero performs an irreversible change to the state vector: it updates the vector according
 *  to the event that the value 'outcome' has been measured on the qubit indicated by measureQubit (where
 *  this label starts from 0, of course). It achieves this by setting all inconsistent amplitudes to 0 and
 *  then renormalising based on the total probability of measuring measureQubit=0 if outcome=0 and
 *  measureQubit=1 if outcome=1.
 *  In the distributed version, one block (with measureQubit=0 in the first half of the block and
 *  measureQubit=1 in the second half of the block) is spread over multiple chunks, meaning that each chunks performs
 *  only renormalisation or only setting amplitudes to 0. This function handles the renormalisation.
 *
 *  @param[in,out] qureg object representing the set of qubits
 *  @param[in] measureQubit qubit to measure
 *  @param[in] totalProbability probability of qubit measureQubit being zero
 */
void statevec_collapseToKnownProbOutcomeDistributedRenorm (Qureg qureg, const int measureQubit, const qreal totalProbability)
{
    // ----- temp variables
    long long int thisTask;
    long long int numTasks=qureg.numAmpsPerChunk;

    qreal renorm=1/sqrt(totalProbability);

    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared    (numTasks,stateVecReal,stateVecImag) \
    private   (thisTask)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule  (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            stateVecReal[thisTask] = stateVecReal[thisTask]*renorm;
            stateVecImag[thisTask] = stateVecImag[thisTask]*renorm;
        }
    }
}

/** Set all amplitudes in one chunk to 0.
 *  Measure in Zero performs an irreversible change to the state vector: it updates the vector according
 *  to the event that a zero have been measured on the qubit indicated by measureQubit (where
 *  this label starts from 0, of course). It achieves this by setting all inconsistent amplitudes to 0 and
 *  then renormalising based on the total probability of measuring measureQubit=0 or 1.
 *  In the distributed version, one block (with measureQubit=0 in the first half of the block and
 *  measureQubit=1 in the second half of the block) is spread over multiple chunks, meaning that each chunks performs
 *  only renormalisation or only setting amplitudes to 0. This function handles setting amplitudes to 0.
 *
 *  @param[in,out] qureg object representing the set of qubits
 *  @param[in] measureQubit qubit to measure
 */
void statevec_collapseToOutcomeDistributedSetZero(Qureg qureg)
{
    // ----- temp variables
    long long int thisTask;
    long long int numTasks=qureg.numAmpsPerChunk;

    // ---------------------------------------------------------------- //
    //            find probability                                      //
    // ---------------------------------------------------------------- //

    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

# ifdef _OPENMP
# pragma omp parallel \
    shared    (numTasks,stateVecReal,stateVecImag) \
    private   (thisTask)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule  (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            stateVecReal[thisTask] = 0;
            stateVecImag[thisTask] = 0;
        }
    }
}

/** It is ensured that all amplitudes needing to be swapped are on this node.
 * This means that amplitudes for |a 0..0..> to |a 1..1..> all exist on this node
 * and each node has a different bit-string prefix "a". The prefix 'a' (and ergo,
 * the chunkID) don't enter the calculations for the offset of |a 0..1..> and
 * |a 1..0..> from |a 0..0..> and ergo are not featured below.
 */
void statevec_swapQubitAmpsLocal(Qureg qureg, int qb1, int qb2) {

    // can't use qureg.stateVec as a private OMP var
    qreal *reVec = qureg.stateVec.real;
    qreal *imVec = qureg.stateVec.imag;

    long long int numTasks = qureg.numAmpsPerChunk >> 2; // each iteration updates 2 amps and skips 2 amps
    long long int thisTask;
    long long int ind00, ind01, ind10;
    qreal re01, re10;
    qreal im01, im10;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (reVec,imVec,numTasks,qb1,qb2) \
    private  (thisTask, ind00,ind01,ind10, re01,re10, im01,im10)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (thisTask=0; thisTask<numTasks; thisTask++) {
            // determine ind00 of |..0..0..>, |..0..1..> and |..1..0..>
            ind00 = insertTwoZeroBits(thisTask, qb1, qb2);
            ind01 = flipBit(ind00, qb1);
            ind10 = flipBit(ind00, qb2);

            // extract statevec amplitudes
            re01 = reVec[ind01]; im01 = imVec[ind01];
            re10 = reVec[ind10]; im10 = imVec[ind10];

            // swap 01 and 10 amps
            reVec[ind01] = re10; reVec[ind10] = re01;
            imVec[ind01] = im10; imVec[ind10] = im01;
        }
    }
}

/** qureg.pairStateVec contains the entire set of amplitudes of the paired node
 * which includes the set of all amplitudes which need to be swapped between
 * |..0..1..> and |..1..0..>
 */
void statevec_swapQubitAmpsDistributed(Qureg qureg, int pairRank, int qb1, int qb2) {

    // can't use qureg.stateVec as a private OMP var
    qreal *reVec = qureg.stateVec.real;
    qreal *imVec = qureg.stateVec.imag;
    qreal *rePairVec = qureg.pairStateVec.real;
    qreal *imPairVec = qureg.pairStateVec.imag;

    long long int numLocalAmps = qureg.numAmpsPerChunk;
    long long int globalStartInd = qureg.chunkId * numLocalAmps;
    long long int pairGlobalStartInd = pairRank * numLocalAmps;

    long long int localInd, globalInd;
    long long int pairLocalInd, pairGlobalInd;

# ifdef _OPENMP
# pragma omp parallel \
    shared   (reVec,imVec,rePairVec,imPairVec,numLocalAmps,globalStartInd,pairGlobalStartInd,qb1,qb2) \
    private  (localInd,globalInd, pairLocalInd,pairGlobalInd)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule (static)
# endif
        for (localInd=0; localInd < numLocalAmps; localInd++) {

            globalInd = globalStartInd + localInd;
            if (isOddParity(globalInd, qb1, qb2)) {

                pairGlobalInd = flipBit(flipBit(globalInd, qb1), qb2);
                pairLocalInd = pairGlobalInd - pairGlobalStartInd;

                reVec[localInd] = rePairVec[pairLocalInd];
                imVec[localInd] = imPairVec[pairLocalInd];
            }
        }
    }
}

void statevec_setWeightedQureg(Complex fac1, Qureg qureg1, Complex fac2, Qureg qureg2, Complex facOut, Qureg out) {

    long long int numAmps = qureg1.numAmpsPerChunk;

    qreal *vecRe1 = qureg1.stateVec.real;
    qreal *vecIm1 = qureg1.stateVec.imag;
    qreal *vecRe2 = qureg2.stateVec.real;
    qreal *vecIm2 = qureg2.stateVec.imag;
    qreal *vecReOut = out.stateVec.real;
    qreal *vecImOut = out.stateVec.imag;

    qreal facRe1 = fac1.real;
    qreal facIm1 = fac1.imag;
    qreal facRe2 = fac2.real;
    qreal facIm2 = fac2.imag;
    qreal facReOut = facOut.real;
    qreal facImOut = facOut.imag;

    qreal re1,im1, re2,im2, reOut,imOut;
    long long int index;

# ifdef _OPENMP
# pragma omp parallel \
    shared    (vecRe1,vecIm1, vecRe2,vecIm2, vecReOut,vecImOut, facRe1,facIm1,facRe2,facIm2, numAmps) \
    private   (index, re1,im1, re2,im2, reOut,imOut)
# endif
    {
# ifdef _OPENMP
# pragma omp for schedule  (static)
# endif
        for (index=0LL; index<numAmps; index++) {
            re1 = vecRe1[index]; im1 = vecIm1[index];
            re2 = vecRe2[index]; im2 = vecIm2[index];
            reOut = vecReOut[index];
            imOut = vecImOut[index];

            vecReOut[index] = (facReOut*reOut - facImOut*imOut) + (facRe1*re1 - facIm1*im1) + (facRe2*re2 - facIm2*im2);
            vecImOut[index] = (facReOut*imOut + facImOut*reOut) + (facRe1*im1 + facIm1*re1) + (facRe2*im2 + facIm2*re2);
        }
    }
}

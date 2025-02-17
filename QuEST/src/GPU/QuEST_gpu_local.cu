#include "QuEST_gpu_internal.h"

//
// State Vector Operations
//

#ifdef __cplusplus
extern "C" {
#endif

__global__ void statevec_compactUnitaryKernel (Qureg qureg, const int rotQubit, Complex alpha, Complex beta){
    // ----- sizes
    long long int sizeBlock,                                           // size of blocks
         sizeHalfBlock;                                       // size of blocks halved
    // ----- indices
    long long int thisBlock,                                           // current block
         indexUp,indexLo;                                     // current index and corresponding index in lower half block

    // ----- temp variables
    qreal   stateRealUp,stateRealLo,                             // storage for previous state values
           stateImagUp,stateImagLo;                             // (used in updates)
    // ----- temp variables
    long long int thisTask;                                   // task based approach for expose loop with small granularity
    const long long int numTasks=qureg.numAmpsPerChunk>>1;

    sizeHalfBlock = 1LL << rotQubit;                               // size of blocks halved
    sizeBlock     = 2LL * sizeHalfBlock;                           // size of blocks

    // ---------------------------------------------------------------- //
    //            rotate                                                //
    // ---------------------------------------------------------------- //

    //! fix -- no necessary for GPU version
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;
    qreal alphaImag=alpha.imag, alphaReal=alpha.real;
    qreal betaImag=beta.imag, betaReal=beta.real;

    thisTask = blockIdx.x*blockDim.x + threadIdx.x;
    if (thisTask>=numTasks) return;

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

void statevec_compactUnitaryLocal(Qureg qureg, const int targetQubit, Complex alpha, Complex beta) 
{
    // stage 1 done!
    // chunkID done!

    int threadsPerCUDABlock, CUDABlocks;
    threadsPerCUDABlock = DEFAULT_THREADS_PER_BLOCK;
    CUDABlocks = ceil((qreal)(qureg.numAmpsPerChunk>>1)/threadsPerCUDABlock);
    statevec_compactUnitaryKernel<<<CUDABlocks, threadsPerCUDABlock>>>(qureg, targetQubit, alpha, beta);
}

__global__ void statevec_controlledCompactUnitaryKernel (Qureg qureg, const int controlQubit, const int targetQubit, Complex alpha, Complex beta){
    // ----- sizes
    long long int sizeBlock,                                           // size of blocks
         sizeHalfBlock;                                       // size of blocks halved
    // ----- indices
    long long int thisBlock,                                           // current block
         indexUp,indexLo;                                     // current index and corresponding index in lower half block

    // ----- temp variables
    qreal   stateRealUp,stateRealLo,                             // storage for previous state values
           stateImagUp,stateImagLo;                             // (used in updates)
    // ----- temp variables
    long long int thisTask;                                   // task based approach for expose loop with small granularity
    const long long int numTasks=qureg.numAmpsPerChunk>>1;
    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;
    int controlBit;

    sizeHalfBlock = 1LL << targetQubit;                               // size of blocks halved
    sizeBlock     = 2LL * sizeHalfBlock;                           // size of blocks

    // ---------------------------------------------------------------- //
    //            rotate                                                //
    // ---------------------------------------------------------------- //

    //! fix -- no necessary for GPU version
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;
    qreal alphaImag=alpha.imag, alphaReal=alpha.real;
    qreal betaImag=beta.imag, betaReal=beta.real;

    thisTask = blockIdx.x*blockDim.x + threadIdx.x;
    if (thisTask>=numTasks) return;

    thisBlock   = thisTask / sizeHalfBlock;
    indexUp     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
    indexLo     = indexUp + sizeHalfBlock;

    controlBit = extractBit(controlQubit, indexUp+chunkId*chunkSize);
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

void statevec_controlledCompactUnitaryLocal(Qureg qureg, const int controlQubit, const int targetQubit, Complex alpha, Complex beta) 
{
    // stage 1 done!
    // chunkID done!

    int threadsPerCUDABlock, CUDABlocks;
    threadsPerCUDABlock = DEFAULT_THREADS_PER_BLOCK;
    CUDABlocks = ceil((qreal)(qureg.numAmpsPerChunk>>1)/threadsPerCUDABlock);
    statevec_controlledCompactUnitaryKernel<<<CUDABlocks, threadsPerCUDABlock>>>(qureg, controlQubit, targetQubit, alpha, beta);
}

__global__ void statevec_unitaryKernel(Qureg qureg, const int targetQubit, ArgMatrix2 u){
    // ----- sizes
    long long int sizeBlock,                                           // size of blocks
         sizeHalfBlock;                                       // size of blocks halved
    // ----- indices
    long long int thisBlock,                                           // current block
         indexUp,indexLo;                                     // current index and corresponding index in lower half block

    // ----- temp variables
    qreal   stateRealUp,stateRealLo,                             // storage for previous state values
           stateImagUp,stateImagLo;                             // (used in updates)
    // ----- temp variables
    long long int thisTask;                                   // task based approach for expose loop with small granularity
    const long long int numTasks=qureg.numAmpsPerChunk>>1;

    sizeHalfBlock = 1LL << targetQubit;                               // size of blocks halved
    sizeBlock     = 2LL * sizeHalfBlock;                           // size of blocks

    // ---------------------------------------------------------------- //
    //            rotate                                                //
    // ---------------------------------------------------------------- //

    //! fix -- no necessary for GPU version
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    thisTask = blockIdx.x*blockDim.x + threadIdx.x;
    if (thisTask>=numTasks) return;

    thisBlock   = thisTask / sizeHalfBlock;
    indexUp     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
    indexLo     = indexUp + sizeHalfBlock;

    // store current state vector values in temp variables
    stateRealUp = stateVecReal[indexUp];
    stateImagUp = stateVecImag[indexUp];

    stateRealLo = stateVecReal[indexLo];
    stateImagLo = stateVecImag[indexLo];

    // state[indexUp] = u00 * state[indexUp] + u01 * state[indexLo]
    stateVecReal[indexUp] = u.r0c0.real*stateRealUp - u.r0c0.imag*stateImagUp 
        + u.r0c1.real*stateRealLo - u.r0c1.imag*stateImagLo;
    stateVecImag[indexUp] = u.r0c0.real*stateImagUp + u.r0c0.imag*stateRealUp 
        + u.r0c1.real*stateImagLo + u.r0c1.imag*stateRealLo;

    // state[indexLo] = u10  * state[indexUp] + u11 * state[indexLo]
    stateVecReal[indexLo] = u.r1c0.real*stateRealUp  - u.r1c0.imag*stateImagUp 
        + u.r1c1.real*stateRealLo  -  u.r1c1.imag*stateImagLo;
    stateVecImag[indexLo] = u.r1c0.real*stateImagUp + u.r1c0.imag*stateRealUp 
        + u.r1c1.real*stateImagLo + u.r1c1.imag*stateRealLo;
}

void statevec_unitaryLocal(Qureg qureg, const int targetQubit, ComplexMatrix2 u)
{
    // stage 1 done!
    // chunkId done!

    int threadsPerCUDABlock, CUDABlocks;
    threadsPerCUDABlock = DEFAULT_THREADS_PER_BLOCK;
    CUDABlocks = ceil((qreal)(qureg.numAmpsPerChunk>>1)/threadsPerCUDABlock);
    statevec_unitaryKernel<<<CUDABlocks, threadsPerCUDABlock>>>(qureg, targetQubit, argifyMatrix2(u));
}

__global__ void statevec_multiControlledMultiQubitUnitaryKernel(
    Qureg qureg, long long int ctrlMask, int* targs, int numTargs, 
    qreal* uRe, qreal* uIm, long long int* ampInds, qreal* reAmps, qreal* imAmps, long long int numTargAmps)
{
    
    // decide the amplitudes this thread will modify
    long long int thisTask = blockIdx.x*blockDim.x + threadIdx.x;                        
    long long int numTasks = qureg.numAmpsPerChunk >> numTargs; // kernel called on every 1 in 2^numTargs amplitudes
    if (thisTask>=numTasks) return;
    
    // the global (between all nodes) index of this node's start index
    long long int globalIndStart = qureg.chunkId*qureg.numAmpsPerChunk;
    long long int thisGlobalInd00; // the global (between all nodes) index of this thread's |..0..0..> state

    // this thread's index of |..0..0..> (target qubits = 0)
    // find this task's start index (where all targs are 0)
    long long int ind00 = insertZeroBits(thisTask, targs, numTargs);
    
    // this task only modifies amplitudes if control qubits are 1 for this state
    thisGlobalInd00 = ind00 + globalIndStart;
    if (ctrlMask && ((ctrlMask & thisGlobalInd00) != ctrlMask))
        return;
        
    qreal *reVec = qureg.stateVec.real;
    qreal *imVec = qureg.stateVec.imag;
    
    /*
    each thread needs:
        long long int ampInds[numAmps];
        qreal reAmps[numAmps];
        qreal imAmps[numAmps];
    but instead has access to shared arrays, with below stride and offset
    */
    size_t stride = gridDim.x*blockDim.x;
    size_t offset = blockIdx.x*blockDim.x + threadIdx.x;
    
    // determine the indices and record values of target amps
    long long int ind;
    for (int i=0; i < numTargAmps; i++) {
        
        // get global index of current target qubit assignment
        ind = ind00;
        for (int t=0; t < numTargs; t++)
            if (extractBit(t, i))
                ind = flipBit(ind, targs[t]);
        
        ampInds[i*stride+offset] = ind;
        reAmps [i*stride+offset] = reVec[ind];
        imAmps [i*stride+offset] = imVec[ind];
    }
    
    // update the amplitudes
    for (int r=0; r < numTargAmps; r++) {
        ind = ampInds[r*stride+offset];
        reVec[ind] = 0;
        imVec[ind] = 0;
        for (int c=0; c < numTargAmps; c++) {
            qreal uReElem = uRe[c + r*numTargAmps];
            qreal uImElem = uIm[c + r*numTargAmps];
            reVec[ind] += reAmps[c*stride+offset]*uReElem - imAmps[c*stride+offset]*uImElem;
            imVec[ind] += reAmps[c*stride+offset]*uImElem + imAmps[c*stride+offset]*uReElem;
        }
    }
}

void statevec_multiControlledMultiQubitUnitaryLocal(Qureg qureg, long long int ctrlMask, int* targs, const int numTargs, ComplexMatrixN u)
{
    // stage 1 done!
    // chunkId done!
    
    int threadsPerCUDABlock = DEFAULT_THREADS_PER_BLOCK;
    int CUDABlocks = ceil((qreal)(qureg.numAmpsPerChunk>>numTargs)/threadsPerCUDABlock);
    
    // allocate device space for global {targs} (length: numTargs) and populate
    int *d_targs;
    size_t targMemSize = numTargs * sizeof *d_targs;
    cudaMalloc(&d_targs, targMemSize);
    cudaMemcpy(d_targs, targs, targMemSize, cudaMemcpyHostToDevice);
    
    // flatten out the u.real and u.imag lists
    int uNumRows = (1 << u.numQubits);
    qreal* uReFlat = (qreal*) malloc(uNumRows*uNumRows * sizeof *uReFlat);
    qreal* uImFlat = (qreal*) malloc(uNumRows*uNumRows * sizeof *uImFlat);
    long long int i = 0;
    for (int r=0; r < uNumRows; r++)
        for (int c=0; c < uNumRows; c++) {
            uReFlat[i] = u.real[r][c];
            uImFlat[i] = u.imag[r][c];
            i++;
        }
    
    // allocate device space for global u.real and u.imag (flatten by concatenating rows) and populate
    qreal* d_uRe;
    qreal* d_uIm;
    size_t uMemSize = uNumRows*uNumRows * sizeof *d_uRe; // size of each of d_uRe and d_uIm
    cudaMalloc(&d_uRe, uMemSize);
    cudaMalloc(&d_uIm, uMemSize);
    cudaMemcpy(d_uRe, uReFlat, uMemSize, cudaMemcpyHostToDevice);
    cudaMemcpy(d_uIm, uImFlat, uMemSize, cudaMemcpyHostToDevice);
    
    // allocate device Wspace for thread-local {ampInds}, {reAmps}, {imAmps} (length: 1<<numTargs)
    long long int *d_ampInds;
    qreal *d_reAmps;
    qreal *d_imAmps;
    size_t gridSize = (size_t) threadsPerCUDABlock * CUDABlocks;
    int numTargAmps = uNumRows;
    cudaMalloc(&d_ampInds, numTargAmps*gridSize * sizeof *d_ampInds);
    cudaMalloc(&d_reAmps,  numTargAmps*gridSize * sizeof *d_reAmps);
    cudaMalloc(&d_imAmps,  numTargAmps*gridSize * sizeof *d_imAmps);
    
    // call kernel
    statevec_multiControlledMultiQubitUnitaryKernel<<<CUDABlocks,threadsPerCUDABlock>>>(
        qureg, ctrlMask, d_targs, numTargs, d_uRe, d_uIm, d_ampInds, d_reAmps, d_imAmps, numTargAmps);
        
    // free kernel memory
    free(uReFlat);
    free(uImFlat);
    cudaFree(d_targs);
    cudaFree(d_uRe);
    cudaFree(d_uIm);
    cudaFree(d_ampInds);
    cudaFree(d_reAmps);
    cudaFree(d_imAmps);
}

__global__ void statevec_multiControlledTwoQubitUnitaryKernel(Qureg qureg, long long int ctrlMask, const int q1, const int q2, ArgMatrix4 u){
    
    // decide the 4 amplitudes this thread will modify
    long long int thisTask = blockIdx.x*blockDim.x + threadIdx.x;                        
    long long int numTasks = qureg.numAmpsPerChunk >> 2; // kernel called on every 1 in 4 amplitudes
    if (thisTask>=numTasks) return;
    
    qreal *reVec = qureg.stateVec.real;
    qreal *imVec = qureg.stateVec.imag;
    
    // find indices of amplitudes to modify (treat q1 as the least significant bit)
    long long int ind00, ind01, ind10, ind11;
    ind00 = insertTwoZeroBits(thisTask, q1, q2);
    
    // the global (between all nodes) index of this node's start index
    long long int globalIndStart = qureg.chunkId*qureg.numAmpsPerChunk;
    long long int thisGlobalInd00 = ind00 + globalIndStart;

    // skip amplitude if controls aren't in 1 state (overloaded for speed)
    // modify only if control qubits are 1 for this state
    if (ctrlMask && ((ctrlMask & thisGlobalInd00) != ctrlMask))
        return;
    
    // inds of |..0..1..>, |..1..0..> and |..1..1..>
    ind01 = flipBit(ind00, q1);
    ind10 = flipBit(ind00, q2);
    ind11 = flipBit(ind01, q2);
    
    // extract statevec amplitudes 
    qreal re00, re01, re10, re11;
    qreal im00, im01, im10, im11;
    re00 = reVec[ind00]; im00 = imVec[ind00];
    re01 = reVec[ind01]; im01 = imVec[ind01];
    re10 = reVec[ind10]; im10 = imVec[ind10];
    re11 = reVec[ind11]; im11 = imVec[ind11];
    
    // apply u * {amp00, amp01, amp10, amp11}
    reVec[ind00] = 
        u.r0c0.real*re00 - u.r0c0.imag*im00 +
        u.r0c1.real*re01 - u.r0c1.imag*im01 +
        u.r0c2.real*re10 - u.r0c2.imag*im10 +
        u.r0c3.real*re11 - u.r0c3.imag*im11;
    imVec[ind00] =
        u.r0c0.imag*re00 + u.r0c0.real*im00 +
        u.r0c1.imag*re01 + u.r0c1.real*im01 +
        u.r0c2.imag*re10 + u.r0c2.real*im10 +
        u.r0c3.imag*re11 + u.r0c3.real*im11;
        
    reVec[ind01] = 
        u.r1c0.real*re00 - u.r1c0.imag*im00 +
        u.r1c1.real*re01 - u.r1c1.imag*im01 +
        u.r1c2.real*re10 - u.r1c2.imag*im10 +
        u.r1c3.real*re11 - u.r1c3.imag*im11;
    imVec[ind01] =
        u.r1c0.imag*re00 + u.r1c0.real*im00 +
        u.r1c1.imag*re01 + u.r1c1.real*im01 +
        u.r1c2.imag*re10 + u.r1c2.real*im10 +
        u.r1c3.imag*re11 + u.r1c3.real*im11;
        
    reVec[ind10] = 
        u.r2c0.real*re00 - u.r2c0.imag*im00 +
        u.r2c1.real*re01 - u.r2c1.imag*im01 +
        u.r2c2.real*re10 - u.r2c2.imag*im10 +
        u.r2c3.real*re11 - u.r2c3.imag*im11;
    imVec[ind10] =
        u.r2c0.imag*re00 + u.r2c0.real*im00 +
        u.r2c1.imag*re01 + u.r2c1.real*im01 +
        u.r2c2.imag*re10 + u.r2c2.real*im10 +
        u.r2c3.imag*re11 + u.r2c3.real*im11;    
        
    reVec[ind11] = 
        u.r3c0.real*re00 - u.r3c0.imag*im00 +
        u.r3c1.real*re01 - u.r3c1.imag*im01 +
        u.r3c2.real*re10 - u.r3c2.imag*im10 +
        u.r3c3.real*re11 - u.r3c3.imag*im11;
    imVec[ind11] =
        u.r3c0.imag*re00 + u.r3c0.real*im00 +
        u.r3c1.imag*re01 + u.r3c1.real*im01 +
        u.r3c2.imag*re10 + u.r3c2.real*im10 +
        u.r3c3.imag*re11 + u.r3c3.real*im11;    
}

void statevec_multiControlledTwoQubitUnitaryLocal(Qureg qureg, long long int ctrlMask, const int q1, const int q2, ComplexMatrix4 u)
{
    // stage 1 done!
    // chunkId done!

    int threadsPerCUDABlock = DEFAULT_THREADS_PER_BLOCK;
    int CUDABlocks = ceil((qreal)(qureg.numAmpsPerChunk>>2)/threadsPerCUDABlock); // one kernel eval for every 4 amplitudes
    statevec_multiControlledTwoQubitUnitaryKernel<<<CUDABlocks, threadsPerCUDABlock>>>(qureg, ctrlMask, q1, q2, argifyMatrix4(u));
}

__global__ void statevec_controlledUnitaryKernel(Qureg qureg, const int controlQubit, const int targetQubit, ArgMatrix2 u){
    // ----- sizes
    long long int sizeBlock,                                           // size of blocks
         sizeHalfBlock;                                       // size of blocks halved
    // ----- indices
    long long int thisBlock,                                           // current block
         indexUp,indexLo;                                     // current index and corresponding index in lower half block

    // ----- temp variables
    qreal   stateRealUp,stateRealLo,                             // storage for previous state values
           stateImagUp,stateImagLo;                             // (used in updates)
    // ----- temp variables
    long long int thisTask;                                   // task based approach for expose loop with small granularity
    const long long int numTasks=qureg.numAmpsPerChunk>>1;
    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;

    int controlBit;

    sizeHalfBlock = 1LL << targetQubit;                               // size of blocks halved
    sizeBlock     = 2LL * sizeHalfBlock;                           // size of blocks

    // ---------------------------------------------------------------- //
    //            rotate                                                //
    // ---------------------------------------------------------------- //

    //! fix -- no necessary for GPU version
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    thisTask = blockIdx.x*blockDim.x + threadIdx.x;
    if (thisTask>=numTasks) return;

    thisBlock   = thisTask / sizeHalfBlock;
    indexUp     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
    indexLo     = indexUp + sizeHalfBlock;

    // store current state vector values in temp variables
    stateRealUp = stateVecReal[indexUp];
    stateImagUp = stateVecImag[indexUp];

    stateRealLo = stateVecReal[indexLo];
    stateImagLo = stateVecImag[indexLo];

    controlBit = extractBit(controlQubit, indexUp+chunkId*chunkSize);
    if (controlBit){
        // state[indexUp] = u00 * state[indexUp] + u01 * state[indexLo]
        stateVecReal[indexUp] = u.r0c0.real*stateRealUp - u.r0c0.imag*stateImagUp 
            + u.r0c1.real*stateRealLo - u.r0c1.imag*stateImagLo;
        stateVecImag[indexUp] = u.r0c0.real*stateImagUp + u.r0c0.imag*stateRealUp 
            + u.r0c1.real*stateImagLo + u.r0c1.imag*stateRealLo;

        // state[indexLo] = u10  * state[indexUp] + u11 * state[indexLo]
        stateVecReal[indexLo] = u.r1c0.real*stateRealUp  - u.r1c0.imag*stateImagUp 
            + u.r1c1.real*stateRealLo  -  u.r1c1.imag*stateImagLo;
        stateVecImag[indexLo] = u.r1c0.real*stateImagUp + u.r1c0.imag*stateRealUp 
            + u.r1c1.real*stateImagLo + u.r1c1.imag*stateRealLo;
    }
}

void statevec_controlledUnitaryLocal(Qureg qureg, const int controlQubit, const int targetQubit, ComplexMatrix2 u)
{
    // stage 1 done!
    // chunkId done!

    int threadsPerCUDABlock, CUDABlocks;
    threadsPerCUDABlock = DEFAULT_THREADS_PER_BLOCK;
    CUDABlocks = ceil((qreal)(qureg.numAmpsPerChunk>>1)/threadsPerCUDABlock);
    statevec_controlledUnitaryKernel<<<CUDABlocks, threadsPerCUDABlock>>>(qureg, controlQubit, targetQubit, argifyMatrix2(u));
}

__global__ void statevec_multiControlledUnitaryKernel(
    Qureg qureg, 
    long long int ctrlQubitsMask, long long int ctrlFlipMask, 
    const int targetQubit, ArgMatrix2 u
){
    // ----- sizes
    long long int sizeBlock,                                           // size of blocks
         sizeHalfBlock;                                       // size of blocks halved
    // ----- indices
    long long int thisBlock,                                           // current block
         indexUp,indexLo;                                     // current index and corresponding index in lower half block

    // ----- temp variables
    qreal   stateRealUp,stateRealLo,                             // storage for previous state values
           stateImagUp,stateImagLo;                             // (used in updates)
    // ----- temp variables
    long long int thisTask;                                   // task based approach for expose loop with small granularity
    const long long int numTasks=qureg.numAmpsPerChunk>>1;
    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;


    sizeHalfBlock = 1LL << targetQubit;                               // size of blocks halved
    sizeBlock     = 2LL * sizeHalfBlock;                           // size of blocks

    // ---------------------------------------------------------------- //
    //            rotate                                                //
    // ---------------------------------------------------------------- //

    //! fix -- no necessary for GPU version
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    thisTask = blockIdx.x*blockDim.x + threadIdx.x;
    if (thisTask>=numTasks) return;

    thisBlock   = thisTask / sizeHalfBlock;
    indexUp     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
    indexLo     = indexUp + sizeHalfBlock;

    if (ctrlQubitsMask == (ctrlQubitsMask & ((indexUp+chunkId*chunkSize) ^ ctrlFlipMask))) {
        // store current state vector values in temp variables
        stateRealUp = stateVecReal[indexUp];
        stateImagUp = stateVecImag[indexUp];

        stateRealLo = stateVecReal[indexLo];
        stateImagLo = stateVecImag[indexLo];

        // state[indexUp] = u00 * state[indexUp] + u01 * state[indexLo]
        stateVecReal[indexUp] = u.r0c0.real*stateRealUp - u.r0c0.imag*stateImagUp 
            + u.r0c1.real*stateRealLo - u.r0c1.imag*stateImagLo;
        stateVecImag[indexUp] = u.r0c0.real*stateImagUp + u.r0c0.imag*stateRealUp 
            + u.r0c1.real*stateImagLo + u.r0c1.imag*stateRealLo;

        // state[indexLo] = u10  * state[indexUp] + u11 * state[indexLo]
        stateVecReal[indexLo] = u.r1c0.real*stateRealUp  - u.r1c0.imag*stateImagUp 
            + u.r1c1.real*stateRealLo  -  u.r1c1.imag*stateImagLo;
        stateVecImag[indexLo] = u.r1c0.real*stateImagUp + u.r1c0.imag*stateRealUp 
            + u.r1c1.real*stateImagLo + u.r1c1.imag*stateRealLo;
    }
}

void statevec_multiControlledUnitaryLocal(
    Qureg qureg, 
    long long int ctrlQubitsMask, long long int ctrlFlipMask, 
    const int targetQubit, ComplexMatrix2 u
){
    // stage 1 done!
    // chunkId done!
    
    int threadsPerCUDABlock = DEFAULT_THREADS_PER_BLOCK;
    int CUDABlocks = ceil((qreal)(qureg.numAmpsPerChunk>>1)/threadsPerCUDABlock);
    statevec_multiControlledUnitaryKernel<<<CUDABlocks, threadsPerCUDABlock>>>(
        qureg, ctrlQubitsMask, ctrlFlipMask, targetQubit, argifyMatrix2(u));
}

__global__ void statevec_pauliXKernel(Qureg qureg, const int targetQubit){
    // ----- sizes
    long long int sizeBlock,                                           // size of blocks
         sizeHalfBlock;                                       // size of blocks halved
    // ----- indices
    long long int thisBlock,                                           // current block
         indexUp,indexLo;                                     // current index and corresponding index in lower half block

    // ----- temp variables
    qreal   stateRealUp,                             // storage for previous state values
           stateImagUp;                             // (used in updates)
    // ----- temp variables
    long long int thisTask;                                   // task based approach for expose loop with small granularity
    const long long int numTasks=qureg.numAmpsPerChunk>>1;

    sizeHalfBlock = 1LL << targetQubit;                               // size of blocks halved
    sizeBlock     = 2LL * sizeHalfBlock;                           // size of blocks

    // ---------------------------------------------------------------- //
    //            rotate                                                //
    // ---------------------------------------------------------------- //

    //! fix -- no necessary for GPU version
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    thisTask = blockIdx.x*blockDim.x + threadIdx.x;
    if (thisTask>=numTasks) return;

    thisBlock   = thisTask / sizeHalfBlock;
    indexUp     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
    indexLo     = indexUp + sizeHalfBlock;

    // store current state vector values in temp variables
    stateRealUp = stateVecReal[indexUp];
    stateImagUp = stateVecImag[indexUp];

    stateVecReal[indexUp] = stateVecReal[indexLo];
    stateVecImag[indexUp] = stateVecImag[indexLo];

    stateVecReal[indexLo] = stateRealUp;
    stateVecImag[indexLo] = stateImagUp;
}

void statevec_pauliXLocal(Qureg qureg, const int targetQubit) 
{
    // stage 1 done!
    // chunkId done!

    int threadsPerCUDABlock, CUDABlocks;
    threadsPerCUDABlock = DEFAULT_THREADS_PER_BLOCK;
    CUDABlocks = ceil((qreal)(qureg.numAmpsPerChunk>>1)/threadsPerCUDABlock);
    statevec_pauliXKernel<<<CUDABlocks, threadsPerCUDABlock>>>(qureg, targetQubit);
}

__global__ void statevec_pauliYKernel(Qureg qureg, const int targetQubit, const int conjFac){

    long long int sizeHalfBlock = 1LL << targetQubit;
    long long int sizeBlock     = 2LL * sizeHalfBlock;
    long long int numTasks      = qureg.numAmpsPerChunk >> 1;
    long long int thisTask      = blockIdx.x*blockDim.x + threadIdx.x;
    if (thisTask>=numTasks) return;
    
    long long int thisBlock     = thisTask / sizeHalfBlock;
    long long int indexUp       = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
    long long int indexLo       = indexUp + sizeHalfBlock;
    qreal  stateRealUp, stateImagUp;

    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;
    stateRealUp = stateVecReal[indexUp];
    stateImagUp = stateVecImag[indexUp];

    // update under +-{{0, -i}, {i, 0}}
    stateVecReal[indexUp] = conjFac * stateVecImag[indexLo];
    stateVecImag[indexUp] = conjFac * -stateVecReal[indexLo];
    stateVecReal[indexLo] = conjFac * -stateImagUp;
    stateVecImag[indexLo] = conjFac * stateRealUp;
}

void statevec_pauliYLocal(Qureg qureg, const int targetQubit) 
{
    // stage 1 done!
    // chunkID done!

    int threadsPerCUDABlock, CUDABlocks;
    threadsPerCUDABlock = DEFAULT_THREADS_PER_BLOCK;
    CUDABlocks = ceil((qreal)(qureg.numAmpsPerChunk>>1)/threadsPerCUDABlock);
    statevec_pauliYKernel<<<CUDABlocks, threadsPerCUDABlock>>>(qureg, targetQubit, 1);
}

void statevec_pauliYConjLocal(Qureg qureg, const int targetQubit) 
{
    // stage 1 done!
    // chunkID done!

    int threadsPerCUDABlock, CUDABlocks;
    threadsPerCUDABlock = DEFAULT_THREADS_PER_BLOCK;
    CUDABlocks = ceil((qreal)(qureg.numAmpsPerChunk>>1)/threadsPerCUDABlock);
    statevec_pauliYKernel<<<CUDABlocks, threadsPerCUDABlock>>>(qureg, targetQubit, -1);
}

__global__ void statevec_controlledPauliYKernel(Qureg qureg, const int controlQubit, const int targetQubit, const int conjFac)
{
    long long int index;
    long long int sizeBlock, sizeHalfBlock;
    long long int stateVecSize;
    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;
    int controlBit;

    qreal   stateRealUp, stateImagUp; 
    long long int thisBlock, indexUp, indexLo;                                     
    sizeHalfBlock = 1LL << targetQubit;
    sizeBlock     = 2LL * sizeHalfBlock;

    stateVecSize = qureg.numAmpsPerChunk;
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    index = blockIdx.x*blockDim.x + threadIdx.x;
    if (index>=(stateVecSize>>1)) return;
    thisBlock   = index / sizeHalfBlock;
    indexUp     = thisBlock*sizeBlock + index%sizeHalfBlock;
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

void statevec_controlledPauliYLocal(Qureg qureg, const int controlQubit, const int targetQubit)
{
    // stage 1 done!
    // chunkID done!

    int conjFactor = 1;
    int threadsPerCUDABlock, CUDABlocks;
    threadsPerCUDABlock = DEFAULT_THREADS_PER_BLOCK;
    CUDABlocks = ceil((qreal)(qureg.numAmpsPerChunk)/threadsPerCUDABlock);
    statevec_controlledPauliYKernel<<<CUDABlocks, threadsPerCUDABlock>>>(qureg, controlQubit, targetQubit, conjFactor);
}

void statevec_controlledPauliYConjLocal(Qureg qureg, const int controlQubit, const int targetQubit)
{
    // stage 1 done!
    // chunkID done!

    int conjFactor = -1;
    int threadsPerCUDABlock, CUDABlocks;
    threadsPerCUDABlock = DEFAULT_THREADS_PER_BLOCK;
    CUDABlocks = ceil((qreal)(qureg.numAmpsPerChunk)/threadsPerCUDABlock);
    statevec_controlledPauliYKernel<<<CUDABlocks, threadsPerCUDABlock>>>(qureg, controlQubit, targetQubit, conjFactor);
}

__global__ void statevec_swapQubitAmpsKernel(Qureg qureg, int qb1, int qb2) {

    qreal *reVec = qureg.stateVec.real;
    qreal *imVec = qureg.stateVec.imag;
    
    long long int numTasks = qureg.numAmpsPerChunk >> 2; // each iteration updates 2 amps and skips 2 amps
    long long int thisTask = blockIdx.x*blockDim.x + threadIdx.x;
    if (thisTask>=numTasks) return;
    
    long long int ind00, ind01, ind10;
    qreal re01, re10, im01, im10;
  
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

void statevec_swapQubitAmpsLocal(Qureg qureg, int qb1, int qb2) 
{
    // stage 1 done!
    // chunkId done!
    
    int threadsPerCUDABlock, CUDABlocks;
    threadsPerCUDABlock = DEFAULT_THREADS_PER_BLOCK;
    CUDABlocks = ceil((qreal)(qureg.numAmpsPerChunk>>2)/threadsPerCUDABlock);
    statevec_swapQubitAmpsKernel<<<CUDABlocks, threadsPerCUDABlock>>>(qureg, qb1, qb2);
}

__global__ void statevec_hadamardKernel (Qureg qureg, const int targetQubit){
    // ----- sizes
    long long int sizeBlock,                                           // size of blocks
         sizeHalfBlock;                                       // size of blocks halved
    // ----- indices
    long long int thisBlock,                                           // current block
         indexUp,indexLo;                                     // current index and corresponding index in lower half block

    // ----- temp variables
    qreal   stateRealUp,stateRealLo,                             // storage for previous state values
           stateImagUp,stateImagLo;                             // (used in updates)
    // ----- temp variables
    long long int thisTask;                                   // task based approach for expose loop with small granularity
    const long long int numTasks=qureg.numAmpsPerChunk>>1;

    sizeHalfBlock = 1LL << targetQubit;                               // size of blocks halved
    sizeBlock     = 2LL * sizeHalfBlock;                           // size of blocks

    // ---------------------------------------------------------------- //
    //            rotate                                                //
    // ---------------------------------------------------------------- //

    //! fix -- no necessary for GPU version
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    qreal recRoot2 = 1.0/sqrt(2.0);

    thisTask = blockIdx.x*blockDim.x + threadIdx.x;
    if (thisTask>=numTasks) return;

    thisBlock   = thisTask / sizeHalfBlock;
    indexUp     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
    indexLo     = indexUp + sizeHalfBlock;

    // store current state vector values in temp variables
    stateRealUp = stateVecReal[indexUp];
    stateImagUp = stateVecImag[indexUp];

    stateRealLo = stateVecReal[indexLo];
    stateImagLo = stateVecImag[indexLo];

    stateVecReal[indexUp] = recRoot2*(stateRealUp + stateRealLo);
    stateVecImag[indexUp] = recRoot2*(stateImagUp + stateImagLo);

    stateVecReal[indexLo] = recRoot2*(stateRealUp - stateRealLo);
    stateVecImag[indexLo] = recRoot2*(stateImagUp - stateImagLo);
}

void statevec_hadamardLocal(Qureg qureg, const int targetQubit) 
{
    // stage 1 done!
    // chunkID done!

    int threadsPerCUDABlock, CUDABlocks;
    threadsPerCUDABlock = DEFAULT_THREADS_PER_BLOCK;
    CUDABlocks = ceil((qreal)(qureg.numAmpsPerChunk>>1)/threadsPerCUDABlock);
    statevec_hadamardKernel<<<CUDABlocks, threadsPerCUDABlock>>>(qureg, targetQubit);
}

__global__ void statevec_controlledNotKernel(Qureg qureg, const int controlQubit, const int targetQubit)
{
    long long int index;
    long long int sizeBlock,                                           // size of blocks
         sizeHalfBlock;                                       // size of blocks halved
    long long int stateVecSize;
    const long long int chunkSize=qureg.numAmpsPerChunk;
    const long long int chunkId=qureg.chunkId;
    int controlBit;

    // ----- temp variables
    qreal   stateRealUp,                             // storage for previous state values
           stateImagUp;                             // (used in updates)
    long long int thisBlock,                                           // current block
         indexUp,indexLo;                                     // current index and corresponding index in lower half block
    sizeHalfBlock = 1LL << targetQubit;                               // size of blocks halved
    sizeBlock     = 2LL * sizeHalfBlock;                           // size of blocks

    stateVecSize = qureg.numAmpsPerChunk;
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    index = blockIdx.x*blockDim.x + threadIdx.x;
    if (index>=(stateVecSize>>1)) return;
    thisBlock   = index / sizeHalfBlock;
    indexUp     = thisBlock*sizeBlock + index%sizeHalfBlock;
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

void statevec_controlledNotLocal(Qureg qureg, const int controlQubit, const int targetQubit)
{
    // stage 1 done!
    // chunkID done!
    
    int threadsPerCUDABlock, CUDABlocks;
    threadsPerCUDABlock = DEFAULT_THREADS_PER_BLOCK;
    CUDABlocks = ceil((qreal)(qureg.numAmpsPerChunk)/threadsPerCUDABlock);
    statevec_controlledNotKernel<<<CUDABlocks, threadsPerCUDABlock>>>(qureg, controlQubit, targetQubit);
}

__device__ __host__ unsigned int log2Int( unsigned int x )
{
    unsigned int ans = 0 ;
    while( x>>=1 ) ans++;
    return ans ;
}

__device__ void reduceBlock(qreal *arrayIn, qreal *reducedArray, int length){
    int i, l, r;
    int threadMax, maxDepth;
    threadMax = length/2;
    maxDepth = log2Int(length/2);

    for (i=0; i<maxDepth+1; i++){
        if (threadIdx.x<threadMax){
            l = threadIdx.x;
            r = l + threadMax;
            arrayIn[l] = arrayIn[r] + arrayIn[l];
        }
        threadMax = threadMax >> 1;
        __syncthreads(); // optimise -- use warp shuffle instead
    }

    if (threadIdx.x==0) reducedArray[blockIdx.x] = arrayIn[0];
}

__global__ void copySharedReduceBlock(qreal*arrayIn, qreal *reducedArray, int length){
    extern __shared__ qreal tempReductionArray[];
    int blockOffset = blockIdx.x*length;
    tempReductionArray[threadIdx.x*2] = arrayIn[blockOffset + threadIdx.x*2];
    tempReductionArray[threadIdx.x*2+1] = arrayIn[blockOffset + threadIdx.x*2+1];
    __syncthreads();
    reduceBlock(tempReductionArray, reducedArray, length);
}

__global__ void statevec_findProbabilityOfZeroKernel(
        Qureg qureg, const int measureQubit, qreal *reducedArray
) {
    // ----- sizes
    long long int sizeBlock,                                           // size of blocks
         sizeHalfBlock;                                       // size of blocks halved
    // ----- indices
    long long int thisBlock,                                           // current block
         index;                                               // current index for first half block
    // ----- temp variables
    long long int thisTask;                                   // task based approach for expose loop with small granularity
    long long int numTasks=qureg.numAmpsPerChunk>>1;
    // (good for shared memory parallelism)

    extern __shared__ qreal tempReductionArray[];

    // ---------------------------------------------------------------- //
    //            dimensions                                            //
    // ---------------------------------------------------------------- //
    sizeHalfBlock = 1LL << (measureQubit);                       // number of state vector elements to sum,
    // and then the number to skip
    sizeBlock     = 2LL * sizeHalfBlock;                           // size of blocks (pairs of measure and skip entries)

    // ---------------------------------------------------------------- //
    //            find probability                                      //
    // ---------------------------------------------------------------- //

    //
    // --- task-based shared-memory parallel implementation
    //

    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    thisTask = blockIdx.x*blockDim.x + threadIdx.x;
    if (thisTask>=numTasks) return;

    thisBlock = thisTask / sizeHalfBlock;
    index     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;
    qreal realVal, imagVal;
    realVal = stateVecReal[index];
    imagVal = stateVecImag[index];  
    tempReductionArray[threadIdx.x] = realVal*realVal + imagVal*imagVal;
    __syncthreads();

    if (threadIdx.x<blockDim.x/2){
        reduceBlock(tempReductionArray, reducedArray, blockDim.x);
    }
}

qreal statevec_findProbabilityOfZeroLocal(Qureg qureg, const int measureQubit)
{
    // stage 1 done!
    // chunkId done!
    
    long long int numValuesToReduce = qureg.numAmpsPerChunk>>1;
    int valuesPerCUDABlock, numCUDABlocks, sharedMemSize;
    qreal stateProb=0;
    int firstTime=1;
    int maxReducedPerLevel = REDUCE_SHARED_SIZE;

    while(numValuesToReduce>1){ 
        if (numValuesToReduce<maxReducedPerLevel){
            // Need less than one CUDA block to reduce values
            valuesPerCUDABlock = numValuesToReduce;
            numCUDABlocks = 1;
        } else {
            // Use full CUDA blocks, with block size constrained by shared mem usage
            valuesPerCUDABlock = maxReducedPerLevel;
            numCUDABlocks = ceil((qreal)numValuesToReduce/valuesPerCUDABlock);
        }
        sharedMemSize = valuesPerCUDABlock*sizeof(qreal);

        if (firstTime){
            statevec_findProbabilityOfZeroKernel<<<numCUDABlocks, valuesPerCUDABlock, sharedMemSize>>>(
                    qureg, measureQubit, qureg.firstLevelReduction);
            firstTime=0;
        } else {
            cudaDeviceSynchronize();    
            copySharedReduceBlock<<<numCUDABlocks, valuesPerCUDABlock/2, sharedMemSize>>>(
                    qureg.firstLevelReduction, 
                    qureg.secondLevelReduction, valuesPerCUDABlock); 
            cudaDeviceSynchronize();    
            swapDouble(&(qureg.firstLevelReduction), &(qureg.secondLevelReduction));
        }
        numValuesToReduce = numValuesToReduce/maxReducedPerLevel;
    }
    cudaMemcpy(&stateProb, qureg.firstLevelReduction, sizeof(qreal), cudaMemcpyDeviceToHost);
    return stateProb;
}

/** computes either a real or imag term in the inner product */
__global__ void statevec_calcInnerProductKernel(
    int getRealComp,
    qreal* vecReal1, qreal* vecImag1, qreal* vecReal2, qreal* vecImag2, 
    long long int numTermsToSum, qreal* reducedArray) 
{
    long long int index = blockIdx.x*blockDim.x + threadIdx.x;
    if (index >= numTermsToSum) return;
    
    // choose whether to calculate the real or imaginary term of the inner product
    qreal innerProdTerm;
    if (getRealComp)
        innerProdTerm = vecReal1[index]*vecReal2[index] + vecImag1[index]*vecImag2[index];
    else
        innerProdTerm = vecReal1[index]*vecImag2[index] - vecImag1[index]*vecReal2[index];
    
    // array of each thread's collected sum term, to be summed
    extern __shared__ qreal tempReductionArray[];
    tempReductionArray[threadIdx.x] = innerProdTerm;
    __syncthreads();
    
    // every second thread reduces
    if (threadIdx.x<blockDim.x/2)
        reduceBlock(tempReductionArray, reducedArray, blockDim.x);
}

/** Terrible code which unnecessarily individually computes and sums the real and imaginary components of the
 * inner product, so as to not have to worry about keeping the sums separated during reduction.
 * Truly disgusting, probably doubles runtime, please fix.
 * @TODO could even do the kernel twice, storing real in bra.reduc and imag in ket.reduc?
 */
Complex statevec_calcInnerProductLocal(Qureg bra, Qureg ket) {
    // stage 1 done!
    // chunkId done!

    qreal innerProdReal, innerProdImag;
    
    int getRealComp;
    long long int numValuesToReduce;
    int valuesPerCUDABlock, numCUDABlocks, sharedMemSize;
    int maxReducedPerLevel;
    int firstTime;
    
    // compute real component of inner product
    getRealComp = 1;
    numValuesToReduce = bra.numAmpsPerChunk;
    maxReducedPerLevel = REDUCE_SHARED_SIZE;
    firstTime = 1;
    while (numValuesToReduce > 1) {
        if (numValuesToReduce < maxReducedPerLevel) {
            valuesPerCUDABlock = numValuesToReduce;
            numCUDABlocks = 1;
        }
        else {
            valuesPerCUDABlock = maxReducedPerLevel;
            numCUDABlocks = ceil((qreal)numValuesToReduce/valuesPerCUDABlock);
        }
        sharedMemSize = valuesPerCUDABlock*sizeof(qreal);
        if (firstTime) {
             statevec_calcInnerProductKernel<<<numCUDABlocks, valuesPerCUDABlock, sharedMemSize>>>(
                 getRealComp,
                 bra.stateVec.real, bra.stateVec.imag, 
                 ket.stateVec.real, ket.stateVec.imag, 
                 numValuesToReduce, 
                 bra.firstLevelReduction);
            firstTime = 0;
        } else {
            cudaDeviceSynchronize();    
            copySharedReduceBlock<<<numCUDABlocks, valuesPerCUDABlock/2, sharedMemSize>>>(
                    bra.firstLevelReduction, 
                    bra.secondLevelReduction, valuesPerCUDABlock); 
            cudaDeviceSynchronize();    
            swapDouble(&(bra.firstLevelReduction), &(bra.secondLevelReduction));
        }
        numValuesToReduce = numValuesToReduce/maxReducedPerLevel;
    }
    cudaMemcpy(&innerProdReal, bra.firstLevelReduction, sizeof(qreal), cudaMemcpyDeviceToHost);
    
    // compute imag component of inner product
    getRealComp = 0;
    numValuesToReduce = bra.numAmpsPerChunk;
    maxReducedPerLevel = REDUCE_SHARED_SIZE;
    firstTime = 1;
    while (numValuesToReduce > 1) {
        if (numValuesToReduce < maxReducedPerLevel) {
            valuesPerCUDABlock = numValuesToReduce;
            numCUDABlocks = 1;
        }
        else {
            valuesPerCUDABlock = maxReducedPerLevel;
            numCUDABlocks = ceil((qreal)numValuesToReduce/valuesPerCUDABlock);
        }
        sharedMemSize = valuesPerCUDABlock*sizeof(qreal);
        if (firstTime) {
             statevec_calcInnerProductKernel<<<numCUDABlocks, valuesPerCUDABlock, sharedMemSize>>>(
                 getRealComp,
                 bra.stateVec.real, bra.stateVec.imag, 
                 ket.stateVec.real, ket.stateVec.imag, 
                 numValuesToReduce, 
                 bra.firstLevelReduction);
            firstTime = 0;
        } else {
            cudaDeviceSynchronize();    
            copySharedReduceBlock<<<numCUDABlocks, valuesPerCUDABlock/2, sharedMemSize>>>(
                    bra.firstLevelReduction, 
                    bra.secondLevelReduction, valuesPerCUDABlock); 
            cudaDeviceSynchronize();    
            swapDouble(&(bra.firstLevelReduction), &(bra.secondLevelReduction));
        }
        numValuesToReduce = numValuesToReduce/maxReducedPerLevel;
    }
    cudaMemcpy(&innerProdImag, bra.firstLevelReduction, sizeof(qreal), cudaMemcpyDeviceToHost);
    
    // return complex
    Complex innerProd;
    innerProd.real = innerProdReal;
    innerProd.imag = innerProdImag;
    return innerProd;
}

__global__ void statevec_collapseToKnownProbOutcomeKernel(Qureg qureg, int measureQubit, int outcome, qreal totalProbability)
{
    // ----- sizes
    long long int sizeBlock,                                           // size of blocks
         sizeHalfBlock;                                       // size of blocks halved
    // ----- indices
    long long int thisBlock,                                           // current block
         index;                                               // current index for first half block
    // ----- measured probability
    qreal   renorm;                                    // probability (returned) value
    // ----- temp variables
    long long int thisTask;                                   // task based approach for expose loop with small granularity
    // (good for shared memory parallelism)
    long long int numTasks=qureg.numAmpsPerChunk>>1;

    // ---------------------------------------------------------------- //
    //            dimensions                                            //
    // ---------------------------------------------------------------- //
    sizeHalfBlock = 1LL << (measureQubit);                       // number of state vector elements to sum,
    // and then the number to skip
    sizeBlock     = 2LL * sizeHalfBlock;                           // size of blocks (pairs of measure and skip entries)

    // ---------------------------------------------------------------- //
    //            find probability                                      //
    // ---------------------------------------------------------------- //

    //
    // --- task-based shared-memory parallel implementation
    //
    renorm=1/sqrt(totalProbability);
    qreal *stateVecReal = qureg.stateVec.real;
    qreal *stateVecImag = qureg.stateVec.imag;

    thisTask = blockIdx.x*blockDim.x + threadIdx.x;
    if (thisTask>=numTasks) return;
    thisBlock = thisTask / sizeHalfBlock;
    index     = thisBlock*sizeBlock + thisTask%sizeHalfBlock;

    if (outcome==0){
        stateVecReal[index]=stateVecReal[index]*renorm;
        stateVecImag[index]=stateVecImag[index]*renorm;

        stateVecReal[index+sizeHalfBlock]=0;
        stateVecImag[index+sizeHalfBlock]=0;
    } else if (outcome==1){
        stateVecReal[index]=0;
        stateVecImag[index]=0;

        stateVecReal[index+sizeHalfBlock]=stateVecReal[index+sizeHalfBlock]*renorm;
        stateVecImag[index+sizeHalfBlock]=stateVecImag[index+sizeHalfBlock]*renorm;
    }
}

/*
 * outcomeProb must accurately be the probability of that qubit outcome in the state-vector, or
 * else the state-vector will lose normalisation
 */
void statevec_collapseToKnownProbOutcomeLocal(Qureg qureg, const int measureQubit, int outcome, qreal outcomeProb)
{
    // stage 1 done!
    // chunkId done!
    
    int threadsPerCUDABlock, CUDABlocks;
    threadsPerCUDABlock = DEFAULT_THREADS_PER_BLOCK;
    CUDABlocks = ceil((qreal)(qureg.numAmpsPerChunk>>1)/threadsPerCUDABlock);
    statevec_collapseToKnownProbOutcomeKernel<<<CUDABlocks, threadsPerCUDABlock>>>(qureg, measureQubit, outcome, outcomeProb);
}



//
// Density Matrix Operations
//

__global__ void densmatr_initPureStateKernel(
    long long int numPureAmps,
    qreal *targetVecReal, qreal *targetVecImag, 
    qreal *copyVecReal, qreal *copyVecImag) 
{
    // this is a particular index of the pure copyQureg
    long long int index = blockIdx.x*blockDim.x + threadIdx.x;
    if (index>=numPureAmps) return;
    
    qreal realRow = copyVecReal[index];
    qreal imagRow = copyVecImag[index];
    for (long long int col=0; col < numPureAmps; col++) {
        qreal realCol =   copyVecReal[col];
        qreal imagCol = - copyVecImag[col]; // minus for conjugation
        targetVecReal[col*numPureAmps + index] = realRow*realCol - imagRow*imagCol;
        targetVecImag[col*numPureAmps + index] = realRow*imagCol + imagRow*realCol;
    }
}

void densmatr_initPureStateLocal(Qureg targetQureg, Qureg copyQureg)
{
    int threadsPerCUDABlock, CUDABlocks;
    threadsPerCUDABlock = DEFAULT_THREADS_PER_BLOCK;
    CUDABlocks = ceil((qreal)(copyQureg.numAmpsPerChunk)/threadsPerCUDABlock);
    densmatr_initPureStateKernel<<<CUDABlocks, threadsPerCUDABlock>>>(
        copyQureg.numAmpsPerChunk,
        targetQureg.stateVec.real, targetQureg.stateVec.imag,
        copyQureg.stateVec.real,   copyQureg.stateVec.imag);
}


//
// Functions only for single GPU version
// And not used at all in distributed version
//


qreal NOT_USED_AT_ALL statevec_calcTotalProbLocal(Qureg qureg){
    /* IJB - implemented using Kahan summation for greater accuracy at a slight floating
       point operation overhead. For more details see https://en.wikipedia.org/wiki/Kahan_summation_algorithm */
    /* Don't change the bracketing in this routine! */
    qreal pTotal=0;
    qreal y, t, c;
    long long int index;
    long long int numAmpsPerRank = qureg.numAmpsPerChunk;

    copyStateFromGPU(qureg);

    c = 0.0;
    for (index=0; index<numAmpsPerRank; index++){
        /* Perform pTotal+=qureg.stateVec.real[index]*qureg.stateVec.real[index]; by Kahan */
        // pTotal+=qureg.stateVec.real[index]*qureg.stateVec.real[index];
        y = qureg.stateVec.real[index]*qureg.stateVec.real[index] - c;
        t = pTotal + y;
        c = ( t - pTotal ) - y;
        pTotal = t;

        /* Perform pTotal+=qureg.stateVec.imag[index]*qureg.stateVec.imag[index]; by Kahan */
        //pTotal+=qureg.stateVec.imag[index]*qureg.stateVec.imag[index];
        y = qureg.stateVec.imag[index]*qureg.stateVec.imag[index] - c;
        t = pTotal + y;
        c = ( t - pTotal ) - y;
        pTotal = t;


    }
    return pTotal;
}

void NOT_USED_AT_ALL seedQuESTDefaultLocal(){
    // init MT random number generator with three keys -- time and pid
    // for the MPI version, it is ok that all procs will get the same seed as random numbers will only be 
    // used by the master process

    unsigned long int key[2];
    getQuESTDefaultSeedKey(key); 
    init_by_array(key, 2); 
}

// already redefined in distributed version
qreal NOT_USED_AT_ALL statevec_getRealAmpLocal(Qureg qureg, long long int index){
    // stage 1 done!
    qreal el=0;
    cudaMemcpy(&el, &(qureg.stateVec.real[index]), 
            sizeof(*(qureg.stateVec.real)), cudaMemcpyDeviceToHost);
    return el;
}

// already redefined in distributed version
qreal NOT_USED_AT_ALL statevec_getImagAmpLocal(Qureg qureg, long long int index){
    // stage 1 done!
    qreal el=0;
    cudaMemcpy(&el, &(qureg.stateVec.imag[index]), 
            sizeof(*(qureg.stateVec.imag)), cudaMemcpyDeviceToHost);
    return el;
}

#ifdef __cplusplus
}
#endif

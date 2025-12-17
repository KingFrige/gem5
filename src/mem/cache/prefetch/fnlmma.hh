/**
 * Copyright (c) 2025 Arm Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2018 Metempsy Technology Consulting
 * Copyright (c) 2024 Samsung Electronics
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MEM_CACHE_PREFETCH_FNLMMA_HH__
#define __MEM_CACHE_PREFETCH_FNLMMA_HH__

#include <queue>

#include "mem/cache/prefetch/queued.hh"
#include "mem/packet.hh"

namespace gem5
{

struct FNLMMAPrefetcherParams;

namespace prefetch
{

class FNLMMA : public Queued
{
  private:
    const unsigned int LOG2_BLOCK_SIZE;

    const bool AHEADPRED;
    const unsigned int DISTAHEAD;
    const unsigned int NSHIFT;
    const unsigned int LOGMULTSIZE;

    const unsigned int MMA_FILT_SIZE;
    std::vector<uint64_t> PREVPRED;

    const unsigned int DISTAHEADMAX;
    std::vector<uint64_t> PREVADDR;
    std::vector<uint64_t> PREFCAND;

    const int NBWAYPRED;
    const int LOGTAGNEXTMISS;
    const int LOGWAYNEXTMISS;
    const int SIZEWAYNEXTMISS;

    const unsigned int MAXFNL;
    const unsigned int PERIODRESET;
    const unsigned int FNL_NBENTRIES;
    std::vector<int> WorthPF;
    std::vector<int> Touched;

    const unsigned int NBWAYISHADOW;
    const unsigned int SIZESHADOWICACHE;
    std::vector<std::vector<uint64_t>> ShadowICache;

    const bool FITERFNLON;
    const unsigned int NBWAYFILTERFNL;
    const unsigned int SIZEWAYFILTERFNL;
    const unsigned int SIZEFILTERFNL;
    std::vector<std::vector<uint64_t>> JUSTNLPREFETCH;

    std::vector<uint64_t> GNtag;
    std::vector<uint64_t> GNblock;
    std::vector<int> GNbMiss;
    std::vector<int8_t> GU;

    uint64_t PrefetchCandidate;
    int ptReset;

    class PredictMiss
    {
      private:
        FNLMMA *outer;
        uint64_t RANDSEED;

      public:
        uint64_t *Ntag;   // 12 bits
        uint64_t *NBlock; // 58 bits
        int8_t *U;
        int *NbMiss; // 1 bit for replacement and confidence
        int distahead;

        PredictMiss(FNLMMA *_outer) : outer(_outer), RANDSEED(0x3f79a17b4) {}

        uint64_t
        MYRANDOM()
        {
            uint64_t X = (RANDSEED >> 7) * 0x9745931;
            if (X == RANDSEED) {
                X++;
            }
            RANDSEED = X;
            return (RANDSEED & 127);
        }

        void
        init(int X)
        {
            Ntag = outer->GNtag.data();
            NBlock = outer->GNblock.data();
            NbMiss = outer->GNbMiss.data();
            U = outer->GU.data();
            int total_entries = outer->NBWAYPRED * outer->SIZEWAYNEXTMISS;
            for (int i = 0; i < total_entries; i++) {
                U[i] = 0;
                Ntag[i] = 0;
                NBlock[i] = 0;
                NbMiss[i] = 0;
            }
            distahead = X;
        }
        uint64_t
        AheadPredict(uint64_t Addr)
        {
            outer->PrefetchCandidate = 0;
            //  manage  the table as a skewed cache :-)
            int index[outer->NBWAYPRED];
            int A = Addr & (outer->SIZEWAYNEXTMISS - 1);
            int B =
                (Addr >> outer->LOGWAYNEXTMISS) & (outer->SIZEWAYNEXTMISS - 1);
            for (int i = 0; i < outer->NBWAYPRED; i++) {
                index[i] = (A ^ B) + (i << outer->LOGWAYNEXTMISS);
                A = (A >> 7) + ((A & 127) << (outer->LOGWAYNEXTMISS - 7));
            }

            uint64_t tag = (Addr >> outer->LOGWAYNEXTMISS) &
                           ((1 << outer->LOGTAGNEXTMISS) - 1);
            int NHIT = -1;
            for (int i = 0; i < outer->NBWAYPRED; i++) {
                if (Ntag[index[i]] == tag) {
                    NHIT = i;
                    outer->PrefetchCandidate = NBlock[index[NHIT]];
                    break;
                }
            }
            if (NHIT == -1) {
                return (0);
            }
            uint64_t X = 0;
            if (U[index[NHIT]] >
                0) { // there were at least two  misses on the same block
                X = NBlock[index[NHIT]];
            }
            return (X);
        }

        void
        LinkAhead(uint64_t Block, uint64_t PrevAddr, uint8_t Hit)
        {
            // Fill the table  on I-cache miss
            if (Hit) {
                return;
            }
            int index[outer->NBWAYPRED];
            int A = PrevAddr & (outer->SIZEWAYNEXTMISS - 1);
            int B = (PrevAddr >> outer->LOGWAYNEXTMISS) &
                    (outer->SIZEWAYNEXTMISS - 1);
            for (int i = 0; i < outer->NBWAYPRED; i++) {
                index[i] = (A ^ B) + (i << outer->LOGWAYNEXTMISS);
                A = (A >> 7) + ((A & 127) << (outer->LOGWAYNEXTMISS - 7));
            }
            uint64_t tag = (PrevAddr >> outer->LOGWAYNEXTMISS) &
                           ((1 << outer->LOGTAGNEXTMISS) - 1);
            int NHIT = -1;
            for (int i = 0; i < outer->NBWAYPRED; i++) {
                if (Ntag[index[i]] == tag) {
                    NbMiss[index[i]]++;
                    NHIT = i; // increase the confidence
                    if (NBlock[index[i]] == Block) {
                        U[index[i]] = 1;
                    } else {
                        NBlock[index[i]] = Block;
                        U[index[i]] = 0;
                    };
                    return;
                }
            }
            // let us try to allocate a new entry
            int X = MYRANDOM() % outer->NBWAYPRED;
            for (int i = 0; i < outer->NBWAYPRED; i++) {
                if (U[index[X]] == 0) {
                    NHIT = X;
                    break;
                };
                X = (X + 1) % outer->NBWAYPRED;
            }
            if (NHIT == -1) {
                // decay some entry
                if ((MYRANDOM() & 15) == 0) {
                    U[index[X]] = 0;
                }
            }
            if (NHIT != -1) {
                // allocate the entry
                NBlock[index[NHIT]] = Block;
                Ntag[index[NHIT]] = tag;
                NbMiss[index[NHIT]] = 0;
                U[index[X]] = 0;
            }
        }
    };

    PredictMiss AHEAD;
    PredictMiss AHEADphist;

  public:
    unsigned int degree;

    FNLMMA(const FNLMMAPrefetcherParams &p);
    ~FNLMMA() = default;

    void JustFnl(uint64_t Block);
    bool WasNotJustFnl(uint64_t Block);
    bool WasNotJustAHEAD(uint64_t Block);
    bool WasNotJustMMA(uint64_t Block);
    bool IsInIShadow(uint64_t Block, bool Insert);
    uint64_t PrefAheadPredictedBlock(uint64_t Block,
                                     std::vector<AddrPriority> &addresses);

    void calculatePrefetch(const PrefetchInfo &pfi,
                           std::vector<AddrPriority> &addresses,
                           const CacheAccessor &cache) override;
};

} // namespace prefetch
} // namespace gem5

#endif /* __MEM_CACHE_PREFETCH_FNLMMA_HH__ */

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

#include "mem/cache/prefetch/fnlmma.hh"

#include "debug/HWPrefetch.hh"
#include "params/FNLMMAPrefetcher.hh"

namespace gem5
{

namespace prefetch
{
FNLMMA::FNLMMA(const FNLMMAPrefetcherParams &p)
    : Queued(p),
      LOG2_BLOCK_SIZE(p.log2_block_size),

      AHEADPRED(p.enable_AHEADPred),
      DISTAHEAD(p.dist_ahead),
      NSHIFT(p.nshift),
      LOGMULTSIZE(p.log_mult_size),

      MMA_FILT_SIZE(p.mma_filt_size),
      PREVPRED(MMA_FILT_SIZE, 0),

      DISTAHEADMAX(p.ahead_max_dist),
      PREVADDR(DISTAHEADMAX + 1, 0),
      PREFCAND(DISTAHEADMAX + 1, 0),

      NBWAYPRED(p.nbway_pred),
      LOGTAGNEXTMISS(p.log_tag_next_miss),
      LOGWAYNEXTMISS(10 + LOGMULTSIZE),
      SIZEWAYNEXTMISS(1 << LOGWAYNEXTMISS),

      MAXFNL(p.maxfnl),
      PERIODRESET(p.period_reset),
      FNL_NBENTRIES(1 << (16 + LOGMULTSIZE)),
      WorthPF(FNL_NBENTRIES, 0),
      Touched(FNL_NBENTRIES, 0),

      NBWAYISHADOW(p.nbway_Ishadow),
      SIZESHADOWICACHE(64 * NBWAYISHADOW),
      ShadowICache(64, std::vector<uint64_t>(NBWAYISHADOW, 0)),

      FITERFNLON(p.enable_fiterFNL),
      NBWAYFILTERFNL(p.nbway_filterFNL),
      SIZEWAYFILTERFNL(p.sizeway_filterFNL),
      SIZEFILTERFNL(SIZEWAYFILTERFNL * NBWAYFILTERFNL),

      JUSTNLPREFETCH(SIZEWAYFILTERFNL,
                     std::vector<uint64_t>(NBWAYFILTERFNL, 0)),
      GNtag(FNL_NBENTRIES, 0),
      GNblock(NBWAYPRED * SIZEWAYNEXTMISS, 0),
      GNbMiss(NBWAYPRED * SIZEWAYNEXTMISS, 0),
      GU(NBWAYPRED * SIZEWAYNEXTMISS, 0),
      AHEAD(this),
      AHEADphist(this)
{
    std::cout << " L1I prefetcher" << std::endl;
    AHEAD.init(DISTAHEAD);
    AHEADphist.init(DISTAHEAD);
    PrefetchCandidate = 0;
    ptReset = 0;
}

void
FNLMMA::JustFnl(uint64_t Block)
{
    // mark the block has just being fetch or prefetch (FIFO management)
    uint64_t set = (Block & (SIZEWAYFILTERFNL - 1));
    uint64_t tag = (Block / SIZEWAYFILTERFNL) & ((1 << 15) - 1);

    std::cout << "JustFnl -> Block: 0x" << std::hex << Block << ", set: 0x"
              << set << ", tag: 0x" << tag << std::dec << std::endl;
    for (int i = NBWAYFILTERFNL - 1; i > 0; i--) {
        JUSTNLPREFETCH[set][i] = JUSTNLPREFETCH[set][i - 1];
    }
    JUSTNLPREFETCH[set][0] = tag;
}

bool
FNLMMA::WasNotJustFnl(uint64_t Block)
{
    // check if the block has been fetched or prefetched
    uint64_t prev = Block - 1;
    int set = (prev & (SIZEWAYFILTERFNL - 1));
    uint64_t tag = (prev / SIZEWAYFILTERFNL) & ((1 << 15) - 1);
    bool ret = true;
    for (int i = 0; i < NBWAYFILTERFNL; i++) {
        if (JUSTNLPREFETCH[set][i] == tag) {
            ret = false;
            break;
        }
    }

    std::cout << "WasNotJustFnl -> Block: 0x" << std::hex << Block
              << ", set: 0x" << set << ", tag: 0x" << tag
              << ", WasNotJustFnl: " << ret << std::dec << std::endl;
    return ret;
}

bool
FNLMMA::WasNotJustAHEAD(uint64_t Block)
{
    bool ret = true;
    // verify that the block has not been already prefetched by MMA recently
    for (int i = MMA_FILT_SIZE - 1; i >= 0; i--) {
        if (PREVPRED[i] == Block) {
            ret = false;
            break;
        }
    }

    return ret;
}

bool
FNLMMA::WasNotJustMMA(uint64_t Block)
{
    bool ret = true;
    for (int i = MMA_FILT_SIZE - 1; i >= 0; i--) {
        if (PREVPRED[i] == Block) {
            ret = false;
            break;
        }
    }

    return ret;
}

bool
FNLMMA::IsInIShadow(uint64_t Block, bool Insert)
{
    // also manage replacement policy if Insert = true
    int Hit = -1;
    int set = Block & 63;
    uint64_t tag = (Block >> 6) & ((1 << 15) - 1);
    for (int i = 0; i < NBWAYISHADOW; i++) {
        if (tag == ShadowICache[set][i]) {
            Hit = i;
            break;
        }
    }
    if (Insert) {
        // Simple solution for software management of LRU
        int Max = (Hit != -1) ? Hit : NBWAYISHADOW - 1;
        for (int i = Max; i > 0; i--) {
            ShadowICache[set][i] = ShadowICache[set][i - 1];
        }
        ShadowICache[set][0] = tag;
    }
    std::cout << "IsInIShadow -> Block: 0x" << std::hex << Block << ", set: 0x"
              << set << ", tag: 0x" << tag << ", Hit: 0x" << Hit << std::dec
              << std::endl;
    return (Hit != -1);
}

uint64_t
FNLMMA::PrefAheadPredictedBlock(uint64_t Block,
                                std::vector<AddrPriority> &addresses)
{
    uint64_t ret_block = Block;
    if (Block != 0) {
        if (WasNotJustMMA(Block)) {
            int index = (Block) & (FNL_NBENTRIES - 1);
            // avoid issing prefetch the block if the previous block was
            // prefetched

            std::cout << std::hex << "prefetch Ahead -> addr: 0x"
                      << (Block << LOG2_BLOCK_SIZE) << std::dec << std::endl;
            addresses.push_back(AddrPriority(Block << LOG2_BLOCK_SIZE, 0));

            if (WorthPF[index] > 0) {
                for (int i = 1; i <= MAXFNL; i++) {
                    uint64_t pf_Block = Block + i;
                    if ((WasNotJustFnl(Block)) || (i == MAXFNL)) {
                        std::cout << std::hex << "prefetch Ahead -> addr: 0x"
                                  << (pf_Block << LOG2_BLOCK_SIZE) << std::dec
                                  << std::endl;
                        addresses.push_back(
                            AddrPriority(pf_Block << LOG2_BLOCK_SIZE, 0));
                    }
                    if (WorthPF[(index + i) & (FNL_NBENTRIES - 1)] == 0) {
                        break;
                    }
                }
                if (FITERFNLON) {
                    JustFnl(Block);
                }
            }
        } else {
            ret_block = 0;
        }
    }
    return ret_block;
}

void
FNLMMA::calculatePrefetch(const PrefetchInfo &pfi,
                          std::vector<AddrPriority> &addresses,
                          const CacheAccessor &cache)
{
    Addr addr = pfi.getAddr();

    bool cache_hit = !pfi.isCacheMiss();

    std::cout << "access addr: 0x" << std::hex << addr << ", cache_hit: 0x"
              << static_cast<int>(cache_hit) << std::dec << std::endl;
    uint64_t Block = addr >> LOG2_BLOCK_SIZE;
    int index = Block & (FNL_NBENTRIES - 1);
    bool ShadowMiss = (!IsInIShadow(Block, 1));
    uint64_t AheadPredictedBlock = 0;
    // prefetch is triggered only on misses on the Shadow I-cache
    std::cout << "ShadowMiss: " << ShadowMiss << std::endl;
    if (ShadowMiss) {
        // The FNL prefetcher
        /////// Manage if it is worth prefetching next block
        int previndex = (index - 1) & (FNL_NBENTRIES - 1);
        Touched[index] = 1;
        if (Touched[previndex]) // if ((index & 63)!=0)
        { // the previous block was read not so long ago: it was worth
          // prefetching this block
            if ((cache_hit == 0) ||
                (WorthPF[previndex])) { // this allows to reduce the pressure
                                        // on L2
                WorthPF[index] = 3;
            }
        }

        std::cout << std::dec << "start ptReset: " << ptReset << std::endl;
        for (int i = ptReset; i < ptReset + (FNL_NBENTRIES / PERIODRESET); i++)
        // Once a block has become worth prefetching, it keeps this status for
        // at least three intervals of PERIODRESET I-Shadow misses
        {

            if (Touched[i]) {
                if (WorthPF[i] > 0) {
                    WorthPF[i]--;
                }
            }
            Touched[i] = 0;
        }
        ptReset += (FNL_NBENTRIES / PERIODRESET);
        ptReset &= (FNL_NBENTRIES - 1);
        std::cout << "end ptReset: " << ptReset << std::endl;

        ////////
        // Next-line prefetch
        if (WorthPF[index] > 0) {
            if (WasNotJustAHEAD(Block)) {
                for (int i = 1; i <= MAXFNL; i++) {
                    uint64_t pf_Block = Block + i;
                    // if Block B-1 was accessed recently one has only to
                    // prefetch Block block+FNL
                    if ((WasNotJustFnl(Block)) || (i == MAXFNL)) {
                        std::cout << std::hex << "prefetch FNL -> addr: 0x"
                                  << (pf_Block << LOG2_BLOCK_SIZE) << std::dec
                                  << std::endl;
                        addresses.push_back(
                            AddrPriority(pf_Block << LOG2_BLOCK_SIZE, 0));
                    }
                    if (WorthPF[(index + i) & (FNL_NBENTRIES - 1)] == 0) {
                        break;
                    }
                }
            }
            if (FITERFNLON) {
                JustFnl(Block);
            }
        }
        /////// END OF THE FNL prefetcher

        if (AHEADPRED) {
            AheadPredictedBlock = AHEADphist.AheadPredict(
                (addr >> 2) ^ (PREVADDR[NSHIFT - 1] << 1));
            std::cout << std::hex << "MMA-AHEADphist -> addr: 0x" << addr
                      << ", AheadPredictedBlock: 0x" << AheadPredictedBlock
                      << std::dec << std::endl;
            AheadPredictedBlock =
                PrefAheadPredictedBlock(AheadPredictedBlock, addresses);

            // First access resulted in a miss
            if (AheadPredictedBlock == 0) {
                //////////////
                AheadPredictedBlock = AHEAD.AheadPredict(addr >> 2);
                std::cout << std::hex << "MMA-AHEAD -> addr: 0x" << addr
                          << ", AheadPredictedBlock: 0x" << AheadPredictedBlock
                          << std::dec << std::endl;

                AheadPredictedBlock =
                    PrefAheadPredictedBlock(AheadPredictedBlock, addresses);
            } // else PrefetchCandidate=0;
              /////
            if ((Block != (PREVADDR[0] >> 4) + 1) ||
                (MAXFNL == 0)) { // Link Block to the address of the block that
                                 // missed DISTAHEAD+1 before
                AHEAD.LinkAhead(Block, PREVADDR[AHEAD.distahead], cache_hit);

                // the PC based  prefetch candidate  was not correct
                if ((PREFCAND[AHEAD.distahead] != 0) &
                    (PREFCAND[AHEAD.distahead] != Block)) {
                    AHEADphist.LinkAhead(
                        Block,
                        PREVADDR[AHEADphist.distahead] ^
                            (PREVADDR[AHEADphist.distahead + NSHIFT] << 1),
                        cache_hit);
                }
            }

            for (int i = DISTAHEADMAX; i > 0; i--) {
                PREVADDR[i] = PREVADDR[i - 1];
            }
            PREVADDR[0] = addr >> 2;
            for (int i = DISTAHEADMAX; i > 0; i--) {
                PREFCAND[i] = PREFCAND[i - 1];
            }
            PREFCAND[0] = PrefetchCandidate;

            if (AheadPredictedBlock != 0) {
                for (int i = MMA_FILT_SIZE - 1; i >= 1; i--) {
                    PREVPRED[i] = PREVPRED[i - 1];
                }
                PREVPRED[0] = AheadPredictedBlock;
            }
        }
        if (debug::HWPrefetch) {
            std::cout << "output debug info" << std::endl;
            for (AddrPriority &addr_prio : addresses) {
                std::cout << std::hex << "prefetch addr: 0x" << addr_prio.first
                          << std::dec << std::endl;
            }
        }
    }
    std::cout << "end access" << std::endl;
    std::cout << std::endl;
}

} // namespace prefetch
} // namespace gem5

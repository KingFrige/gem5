from m5.objects import *


class L1Cache(Cache):
    tag_latency = 2
    data_latency = 2
    response_latency = 2
    tgts_per_mshr = 8
    # Consider the L2 a victim cache also for clean lines
    writeback_clean = True
    replacement_policy = RRIPRP()


# Instruction Cache
class L1I(L1Cache):
    mshrs = 16
    size = "64KiB"
    assoc = 8
    mshrs = 12
    is_read_only = True
    prefetcher = MultiPrefetcher(
        prefetchers=[
            StridePrefetcher(degree=8, latency=1, prefetch_on_access=True),
            FNLMMAPrefetcher(latency=2),
            TaggedPrefetcher(use_virtual_addresses=True),
        ]
    )


# Data Cache
class L1D(L1Cache):
    mshrs = 16
    size = "64KiB"
    assoc = 8
    mshrs = 12
    tag_latency = 4
    data_latency = 4
    response_latency = 4
    write_buffers = 16
    prefetcher = MultiPrefetcher(
        prefetchers=[
            StridePrefetcher(degree=8, latency=1, prefetch_on_access=True),
            SmsPrefetcher(),
            BOPPrefetcher(),
        ]
    )


# L2 Cache
class L2(Cache):
    tag_latency = 11
    data_latency = 11
    response_latency = 11
    mshrs = 96  # 96-entry Transaction Queue
    tgts_per_mshr = 8
    size = "2MiB"
    assoc = 8
    write_buffers = 8
    clusivity = "mostly_incl"
    # Simple stride prefetcher
    tags = BaseSetAssoc()
    replacement_policy = RRIPRP()

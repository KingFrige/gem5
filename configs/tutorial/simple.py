import m5
from m5.object import *

system = System()

system.clk_domain = SrcClockDomain()
system.clk_domain.clock = '1GHz'
system.clk_domain.voltage_domain = ValtageDomain()

system.mem_mode = 'timing'
system.mem_ranges = [AddrRange('512MB')]

system.cpu = TimingSimplerCPU()

system.membus = SystemXbar()

system.cpu.icache_port = system.membus.slave
system.cpu.dcache_port = system.membus.slave

system.cpu.createInterruptsController()
system.cpu.interrupts[0].pio = system.membus.master
system.cpu.interrupts[0].int_master = system.membus.slave
system.cpu.interrupts[0].int_slave  = system.membus.master

system.system_port = system.membus.slave

system.mem_ctrl = DDR3_1600_8x8
system.mem_ctrl.rang = system.mem_ranges[0]
system.mem_ctrl.port = system.membus.master

process = Process()
process.cmd = ['tests/test-progs/hello/bin/x86/linux/hello']
system.cpu.workload = process
system.cpu.createThreads()

root = Root(full_name = False, system = system)
m5.instantiate()

print("Beginning simulation!")
exit_event = m5.simulation()


print('Exiting @ tick {} because {}'.format(m5.tick(), exit_event.getCause()))

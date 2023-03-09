# Usage with Vitis IDE:
# In Vitis IDE create a Single Application Debug launch configuration,
# change the debug type to 'Attach to running target' and provide this 
# tcl script in 'Execute Script' option.
# Path of this script: C:\Users\user\workspace\vestel_scale_system\_ide\scripts\debugger_vestel_scale-default_1.tcl
# 
# 
# Usage with xsct:
# To debug using xsct, launch xsct and run below command
# source C:\Users\user\workspace\vestel_scale_system\_ide\scripts\debugger_vestel_scale-default_1.tcl
# 
connect -url tcp:127.0.0.1:3121
targets -set -filter {jtag_cable_name =~ "Xilinx HW-S7-SP701 FT4232H 54092145116A" && level==0 && jtag_device_ctx=="jsn-HW-S7-SP701 FT4232H-54092145116A-037c7093-0"}
fpga -file C:/Users/user/workspace/vestel_scale/_ide/bitstream/taylor_reiz_wrapper.bit
targets -set -nocase -filter {name =~ "*microblaze*#0" && bscan=="USER2" }
loadhw -hw C:/Users/user/workspace/system_vestel/export/system_vestel/hw/taylor_reiz_wrapper.xsa -regs
configparams mdm-detect-bscan-mask 2
targets -set -nocase -filter {name =~ "*microblaze*#0" && bscan=="USER2" }
rst -system
after 3000
targets -set -nocase -filter {name =~ "*microblaze*#0" && bscan=="USER2" }
dow C:/Users/user/workspace/vestel_scale/Debug/vestel_scale.elf
targets -set -nocase -filter {name =~ "*microblaze*#0" && bscan=="USER2" }
con

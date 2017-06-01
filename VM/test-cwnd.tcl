/*
@author: Xuanlong
@email:<xuanlong@pku.edu.cn>
@time: 2017/06/01
*/

namespace import ::tcl::mathfunc::*

if { $argc != 5} {
	puts "uncorrect paras"
	#exit
} else {
	#bk_flow_num/dirty_rate/vm_size/enable_alg1/enable_alg2
	set bk_flow_num [lindex $argv 0]
	set dirtyRate [lindex $argv 1]
	set vmSize [lindex $argv 2]
	set enable_alg1 [lindex $argv 3]
	set enable_alg2 [lindex $argv 4]	
}

set  rng  [new RNG]
$rng  seed 0
set  r  [new RandomVariable/Uniform]
$r  use-rng $rng


set fg_flow_num 1
set global_start_time 0.0
set global_stop_time 100


Trace set show_tcphdr_ 1


proc getRandDouble { min max } {  
	global rng r
	$r  set  min_ $min
	$r  set  max_ $max
	set result [$r value] 
	return $result  
}

proc getRandInt { min max } {
	set temp_double [getRandDouble $min $max]
	set result [round $temp_double]         
	return $result
}

Agent/TCP/FullTcp set tcpTick_ 0.000001
Agent/TCP/FullTcp set segsize_ 1460
Agent/TCP/FullTcp set interval_ 0.000001
Agent/TCP set window_ 55
#Agent/TCP/FullTcp set minrto_ 0.2
Agent/TCP/FullTcp set minrto_ 0.000001

Queue set limit_ 8
Queue/DropTail set mean_pktsize_ 1500
set queue_size 8

set band_width 1Gb
set link_delay 25us



#版本设置
set ns [new Simulator]

#trace
#set tf [open out-$bk_flow_num-$dirty_rate-$vm_size-$enable_alg1-$enable_alg2.tr w]
#$ns trace-all $tf
set tf [open out.tr w]
#$ns trace-all $tf

set enablenam 0
#set nf [open out-$bk_flow_num-$dirty_rate-$vm_size-$enable_alg1-$enable_alg2.nam w]
#set nf [open out.nam w]
#if { $enablenam == 1 } {
#	$ns namtrace-all $nf
#}

#trace cwnd
#set cf [open cwnd-$bk_flow_num-$dirty_rate-$vm_size-$enable_alg1-$enable_alg2.tr w]
set cf [open cwnd.tr w]


set server_num 8
set left_server_min 0
set left_server_max 3
set right_server_min 4
set right_server_max 7

#nodes
for {set i 0} {$i < $server_num} {incr i} {
	set server_nodes($i) [$ns node]
}

set router_left [$ns node]
set router_middle [$ns node]
set router_right [$ns node]

for {set i $left_server_min} {$i <= $left_server_max} {incr i} {
	$ns duplex-link $server_nodes($i) $router_left $band_width $link_delay DropTail
	#$ns queue-limit $server_nodes($i) $router_left $queue_size
	#$ns queue-limit $router_left $server_nodes($i) $queue_size
}

for {set i $right_server_min} {$i <= $right_server_max} {incr i} {
	$ns duplex-link $server_nodes($i) $router_right $band_width $link_delay DropTail
	#$ns queue-limit $server_nodes($i) $router_right $queue_size
	#$ns queue-limit $router_right $server_nodes($i) $queue_size
}

$ns duplex-link $router_left $router_middle $band_width $link_delay DropTail
$ns duplex-link $router_middle $router_right $band_width $link_delay DropTail

$ns queue-limit $router_left $router_middle $queue_size
$ns queue-limit $router_middle $router_left $queue_size

$ns queue-limit $router_middle $router_right $queue_size
$ns queue-limit $router_right $router_middle $queue_size


set q_ [$ns monitor-queue $server_nodes(7) $router_right [open qm.out w] 0.0001]
#cwnd记录
proc Record { } {
	global cf ns fg_src fg_flow_num q_ bk_src bk_flow_num
	#$q_ instproc length
	set interval 0.00001
	#set interval 0.00001
	set now [ $ns now]
	puts -nonewline $cf "$now" 
	#set val [$bk_src(1) set cwnd_]
	#uts -nonewline $cf "\t$val"
	for {set i 0} {$i < $fg_flow_num} {incr i} {
		set val [$fg_src($i) set cwnd_]
		puts -nonewline $cf "\t$val"
	}
	#for {set i 0} {$i < $bk_flow_num} {incr i} {
	#	set val [$bk_src($i) set cwnd_]
	#	puts -nonewline $cf "\t$val"
	#}
	#$q_ length
	#set q-length [$q_ length]
	#set q_length [expr [$q_ set size_]/1500]
	#puts -nonewline $cf "\t$q_length"
	puts $cf ""
#	set val [$src set cwnd_]
#	puts $cf "$now\t$val" 
	$ns at [expr $now + $interval] "Record"
}

$ns at 0.0 "Record"


Application set dirtyRate $dirtyRate
Application set vmThd 1460
Application set vmSize $vmSize

for {set i 0} {$i < $bk_flow_num} {incr i} {

	set int1 [getRandInt $left_server_min $left_server_max]
	set int2 [getRandInt $right_server_min $right_server_max]
	set temp_back_src1 -1
	set temp_back_sink1 -1
	if { [expr $i % 2] } {
		set temp_back_src1 $int1
		set temp_back_sink1 $int2
	} else {
		set temp_back_src1 $int2
		set temp_back_sink1 $int1
	}
	#puts "background :  $temp_back_src1 $temp_back_sink1"

	set bk_src_node($i) $server_nodes($temp_back_src1)
	set bk_sink_node($i) $server_nodes($temp_back_sink1)
	
	set bk_src($i) [new Agent/TCP/FullTcp]
	set bk_sink($i) [new Agent/TCP/FullTcp]

	$bk_src($i) set fid_ $i
	$bk_sink($i) set fid_ $i
	$ns attach-agent $bk_src_node($i) $bk_src($i)
	$ns attach-agent $bk_sink_node($i) $bk_sink($i)
	$ns connect $bk_src($i) $bk_sink($i)
	$bk_sink($i) listen
	
	set bk_ftp($i) [new Application/FTP]
	$bk_ftp($i) set type_ FTP
	$bk_ftp($i) attach-agent $bk_src($i)
	set bk_start [getRandDouble $global_start_time [expr $global_start_time+0.5]]
	set bk_stop [getRandDouble [expr $global_stop_time-0.5] $global_stop_time]
	$ns at $bk_start "$bk_ftp($i) start"
	$ns at $bk_stop "$bk_ftp($i) stop"
	puts "background :  $temp_back_src1 $temp_back_sink1\t$bk_start\t$bk_stop"
}

for {set i 0} {$i < $fg_flow_num} {incr i} {

	set int_first [getRandInt $left_server_min $left_server_max]
	set int_second [getRandInt $right_server_min $right_server_max]
	set temp_fg_src -1
	set temp_fg_sink -1
	if { [expr $i % 2] } {
		set temp_fg_src $int_first
		set temp_fg_sink $int_second
	} else {
		set temp_fg_src $int_second
		set temp_fg_sink $int_first
	}

	set fg_src_node($i) $server_nodes($temp_fg_src)
	set fg_sink_node($i) $server_nodes($temp_fg_sink)
	#set fg_src_node($i) $server_nodes(5)
	#set fg_sink_node($i) $server_nodes(2)
	
	set fg_src($i) [new Agent/TCP/FullTcp]
	set fg_sink($i) [new Agent/TCP/FullTcp]

	$fg_src($i) set enableVM_ 1
	$fg_src($i) set enableMigrationAlg1_ $enable_alg1
	$fg_src($i) set enableMigrationAlg2_ $enable_alg2
	
	$fg_src($i) set fid_ [expr 500+$i]
	$fg_sink($i) set fid_ [expr 500+$i]
	$ns attach-agent $fg_src_node($i) $fg_src($i)
	$ns attach-agent $fg_sink_node($i) $fg_sink($i)
	$ns connect $fg_src($i) $fg_sink($i)
	$fg_sink($i) listen
	
	set fg_ftp($i) [new Application/FTP]
	$fg_ftp($i) set type_ FTP
	$fg_ftp($i) attach-agent $fg_src($i)

	#set fg_start_time [getRandDouble 2.0 3.0]
	#$ns at $fg_start_time "$fg_ftp($i) send $vm_size"
	$ns at 0.0 "$fg_ftp($i) send $vmSize"
	#puts "foreground :  $temp_fg_src $temp_fg_sink"
	#puts "foreground :  $temp_fg_src $temp_fg_sink $fg_start_time"
}

proc finish {} {
	global ns bk_flow_num dirtyRate vmSize enable_alg1 enable_alg2 tf fg_flow_num fg_ftp # cf nf enablenam 
	$ns flush-trace
	close $tf
	set avg_data 0.0
	set avg_time 0.0
	set count 0
	#set max 0.0
	#set maxTime 0.0
	for {set i 0} {$i < $fg_flow_num} {incr i} {
		set tempftp $fg_ftp($i)
		set vmdata [$fg_ftp($i) set m_totalSend]
		set vmtime [$fg_ftp($i) set m_totalTime]
	}
	#set avg_data [expr $avg_data-$max]
	#set avg_time [expr $avg_time-$maxTime]
	#set count [expr $count-1]
	
	
	#close $cf
	#close $nf
	#if { $enablenam == 1 } {
	#	puts "running nam..."
	#	exec nam out-$bk_flow_num-$dirty_rate-$vm_size-$enable_alg1-$enable_alg2.nam &
	#}
	exit 0
}

#set tf [open out.tr w]
#$ns trace-queue $server_nodes(6) $router_right $tf
#$ns trace-queue $router_right $server_nodes(6) $tf
#$ns trace-queue $router_left $server_nodes(0) $tf


$ns at $global_stop_time "finish"

$ns run



namespace import ::tcl::mathfunc::*

if { $argc !=5} {
	puts "uncorrect parameters"
} else {
	set bk_flow_num [lindex $argv 0]
	set dirty_rate [lindex $argv 1]
	set vm_size [lindex $argv 2]
	set vmThd [lindex $argv 3]
	set bandwidth [lindex $argv 4]
	#vthd/bandwidth
}

set rng [new RNG]
$rng seed 1
set r [new RandomVariable/Uniform]
$r use-rng $rng
$r set min_ 0.0
$r set max_ 10.0  
#provide the mix_ and max_ parameters

proc getRandDouble {min max} \
{
	global rng r
	$r set min_ $min
	$r set max_ $max
	set result [$r value]
	return $result
}

proc getRandInt { min max } {
	set temp_double [getRandDouble $min $max]
	set result [round $temp_double]         
	return $result
}


set fg_flow_num 1
set global_start_time 0.0
set global_stop_time 10.0

#Trace set show_tcphdr_ 1



#TCP based parameters
Agent/TCP/FullTcp set tcpTick_ 
Agent/TCP/FullTcp set segsize_
Agent/TCP/FullTcp set interval_
Agent/TCP/FullTcp set minrto_ 
Agent/TCP set window_

Queue set limit_ 8
Queue/DropTail set mean_pktsize_ 1500

# no bind right now

set queue_size 8
#set bandwidth 1Gb
set link_delay 25us


set ns [new Simulator]
#Trace not set yet


#the network topo
set server_num 8
set left_server_min 0
set left_server_max 3
set right_server_min 4
set right_server_max 7

#nodes
for {set i 0} {$i < $server_num} {incr i} {
	set server_nodes($i) [$ns node]
}


#产生三个节点做路由
set router_left [$ns node]
set router_middle [$ns node]
set router_right [$ns node]

#自适应带宽
for {set i $bandwidth} {$i < 500000000} {incr i 50000000} {
	set bandwidth $i
	puts "$bandwidth"
}

#节点和路由链接起来  带宽和延迟还没有设置   带宽加到这里面
for {set i left_server_min} {$i < $left_server_max} {incr i} {
	$ns duplex-link $server_nodes($i) $router_left $bandwidth $link_delay DropTail
}

for {set i right_server_min} {$i < $right_server_max} {incr i} {
	$ns duplex-link $server_nodes($i) $router_right $bandwidth $link_delay DropTail
}

$ns duplex-link $router_left $router_middle $bandwidth $link_delay DropTail
$ns duplex-link $router_middle $router_right $bandwidth $link_delay DropTail

#设置队列长度 queue_size还没有设置
$ns queue_limit $router_left $router_middle $queue_size  
$ns queue_limit $router_middle $router_left $queue_size

$ns queue_limit $router_middle $router_right $queue_size
$ns queue_limit $router_right $router_middle $queue_size  


#应用层绑定数据
Application set dirtyRate $dirty_rate
Application set vmThresh $vmThd
Application set bandwidth $bandwidth
#设置随机背景流
for {set i 0} {$i < $bk_flow_num} {incr i} {
	set int1 [getRandInt $left_server_min $left_server_max]
	set int2 [getRandInt $right_server_min $right_server_max]
	set temp_back_src1 -1
	set temp_back_sink1 -1
	#基数为源端的流 偶数为目的端的流
	if {[expr $i % 2]} {
		set temp_back_src1 $int1
		set temp_back_sink1 $int2
	} else {
		set temp_back_src1 $int2
		set temp_back_sink1 $int1
	}


	set bk_src_node($i) $server_nodes($temp_back_src1)
	set bk_sink_node($i) $server_nodes($temp_back_sink1)
	
	set bk_src($i) [new Agent/TCP/FullTcp]
	set bk_sink($i) [new Agent/TCP/FullTcp]
	#代理连接
	#fid_是指flowid
	#每一个绑定的变量当对象创建时都自动初始化为缺省值，如果在所有层次中都找不到该变量的定义，将会打印出一条警告信息\
	大多数变量的初始值是在ns-default.tcl中定义
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


#设置随机前景流
for {set i 0} {$i < $fg_flow_num} {incr i} {
	set int_first [getRandInt $left_server_min $left_server_max]
	set int_second [getRandInt $right_server_min $right_server_max]
	set temp_fg_src -1
	set temp_fg_sink -1
	#判定源端和输出端
	if {[expr $i % 2]} {
		set temp_fg_src $int_first
		set temp_fg_sink $int_second
	} else {
		set temp_fg_src $int_second
		set temp_fg_sink $int_first
	}

	set fg_src_node($i) $server_nodes($temp_fg_src)
	set fg_src_node($i) $server_nodes($temp_fg_sink)

	set fg_src($i) [new Agent/TCP/FullTcp]
	set fg_sink($i) [new Agent/TCP/FullTcp]

	#虚拟机运行？
	$fg_src($i) enableVM_ 1
	$fg_src($i) set fid_ [expr 500+$i]
	$fg_sink($i) set fid_ [expr 500+$i]
	$ns attach-agent $fg_src_node($i) $fg_src($i)
	$ns attach-agent $fg_src_node($i) $fg_sink($i)
	$ns connect $fg_src($i) $fg_sink($i)
	$fg_sink($i) listen

    
    #TCP上使用FTP
	set fg_ftp($i) [new Application/FTP]
	$fg_ftp($i) set type_ FTP
	$fg_ftp($i) attach-agent $fg_src($i)


    #这里发送的是字节
	$ns at 0.0 "$fg_ftp($i) send $vm_size"

}

proc finish {} {
	global ns bk_flow_num dirty_rate vm_size vmThd fg_ftp 
	#一直没有加trace文件
	set total_data 0.0
	set total_time 0.0
	set vm_count 0
	for {set i 0} {$i < $fg_flow_num} {incr i} {
		set tempftp $fg_ftp($i)
		#vmdata是所有轮的数据
		#totaldata是第一轮发的
		set vmdata [$fg_ftp($i) set totaldata]
		set vmtime [$fg_ftp($i) set totaltime]
		if {$vmdata >= $vm_size} {
			set vm_count [expr $vm_count+1]
			set total_data [expr $total_data + $vmdata]
			set total_time [expr $total_time + $vmdata]
			puts "$vmdata\t$vmtime"
		}
	}
	if {vm_count > 0} {
		puts "vm_count\t[expr $vm_count]"
		puts "total_data:\t[expr $total_data]"
		puts "total_time:\t[expr $total_time]"
	}
	exit 0
}

$ns at global_stop_time "finish"

$ns run

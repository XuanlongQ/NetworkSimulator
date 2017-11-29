#!/usr/bin/python

from mininet.net import Mininet
from mininet.node import Controller, RemoteController, OVSController
from mininet.node import CPULimitedHost, Host, Node
from mininet.node import OVSKernelSwitch, UserSwitch
from mininet.node import IVSSwitch
from mininet.cli import CLI
from mininet.log import setLogLevel, info
from mininet.link import TCLink, Intf
from subprocess import call
import time

def myNetwork():

	net = Mininet( topo=None,
				   build=False,
				   ipBase='10.0.0.0/8')

	info( '*** Adding controller\n' )
	c0=net.addController(name='c0',
					  controller=RemoteController,
					  ip='127.0.0.1',
					  protocol='tcp',
					  port=6633)

	info( '*** Add switches\n')
	print("add switch")
	#"""
	u1 = net.addSwitch('u1', cls=OVSKernelSwitch, dpid='2')
	u2 = net.addSwitch('u2', cls=OVSKernelSwitch, dpid='3')
	u3 = net.addSwitch('u3', cls=OVSKernelSwitch, dpid='4')
	l1 = net.addSwitch('l1', cls=OVSKernelSwitch, dpid='5')
	l2 = net.addSwitch('l2', cls=OVSKernelSwitch, dpid='6')
	upper_switches = ['u1', 'u2', 'u3']
	lower_switches = ['l1', 'l2']
	#"""

	"""
	u1 = net.addSwitch('u1', cls=UserSwitch, dpid='2')
	u2 = net.addSwitch('u2', cls=UserSwitch, dpid='3')
	u3 = net.addSwitch('u3', cls=UserSwitch, dpid='4')
	l1 = net.addSwitch('l1', cls=UserSwitch, dpid='5')
	l2 = net.addSwitch('l2', cls=UserSwitch, dpid='6')
	upper_switches = [u1, u2, u3]
	lower_switches = [l1, l2]
	"""

	info( '*** Add hosts\n')
	h1 = net.addHost('h1', cls=Host, ip='10.0.0.1', mac='00:00:00:00:00:01', defaultRoute='h1-eth0')
	h2 = net.addHost('h2', cls=Host, ip='10.0.0.2', mac='00:00:00:00:00:02', defaultRoute='h2-eth0')
	h3 = net.addHost('h3', cls=Host, ip='10.0.0.3', mac='00:00:00:00:00:03', defaultRoute='h3-eth0')
	h4 = net.addHost('h4', cls=Host, ip='10.0.0.4', mac='00:00:00:00:00:04', defaultRoute='h4-eth0')
	h5 = net.addHost('h5', cls=Host, ip='10.0.0.5', mac='00:00:00:00:00:05', defaultRoute='h5-eth0')
	h6 = net.addHost('h6', cls=Host, ip='10.0.0.6', mac='00:00:00:00:00:06', defaultRoute='h6-eth0')
	h7 = net.addHost('h7', cls=Host, ip='10.0.0.7', mac='00:00:00:00:00:07', defaultRoute='h7-eth0')
	h8 = net.addHost('h8', cls=Host, ip='10.0.0.8', mac='00:00:00:00:00:08', defaultRoute='h8-eth0')

	hosts = [h1, h2, h3, h4, h5, h6, h7, h8]

	info( '*** Add links\n')
	b1 = {'bw':100}
	b2 = {'bw':10}
	b3 = {'bw':10}

	#"""
	net.addLink(u1, l1, cls=TCLink, **b2)
	net.addLink(u1, l2, cls=TCLink, **b2)
	net.addLink(u2, l1, cls=TCLink, **b2)
	net.addLink(u2, l2, cls=TCLink, **b2)
	net.addLink(u3, l1, cls=TCLink, **b2)
	net.addLink(u3, l2, cls=TCLink, **b2)

	net.addLink(l1, h1, cls=TCLink, **b3)
	net.addLink(l1, h2, cls=TCLink, **b3)
	net.addLink(l1, h3, cls=TCLink, **b3)
	net.addLink(l1, h4, cls=TCLink, **b3)
	net.addLink(l2, h5, cls=TCLink, **b3)
	net.addLink(l2, h6, cls=TCLink, **b3)
	net.addLink(l2, h7, cls=TCLink, **b3)
	net.addLink(l2, h8, cls=TCLink, **b3)
	#"""

	"""
	net.addLink(u1, l1)
	net.addLink(u1, l2)
	net.addLink(u2, l1)
	net.addLink(u2, l2)
	net.addLink(u3, l1)
	net.addLink(u3, l2)

	net.addLink(l1, h1)
	net.addLink(l1, h2)
	net.addLink(l1, h3)
	net.addLink(l1, h4)
	net.addLink(l2, h5)
	net.addLink(l2, h6)
	net.addLink(l2, h7)
	net.addLink(l2, h8)
	"""

	info( '*** Starting network\n')
	net.build()
	info( '*** Starting controllers\n')
	for controller in net.controllers:
		controller.start()

	info( '*** Starting switches\n')
	net.get('u3').start([c0])
	net.get('u2').start([c0])
	net.get('l2').start([c0])
	net.get('u1').start([c0])
	net.get('l1').start([c0])

	info( '*** Post configure switches and hosts\n')
	print("start to send LLDP")

	t = 3
	for host in hosts:
		host.popen('~/tiny-lldpd/tlldpd -D -i 5 ', shell=True)

	#print("clean qos & queue")
	#h1.popen("ovs-vsctl --all destroy qos")
	#h1.popen("ovs-vsctl --all destroy queue")
	time.sleep(10)
	#print("configure queue on all switches' ports")

	"""
	for sw in upper_switches:
		i = 1
		while i <= 2:
			cmd = "ovs-vsctl -- set Port " + sw + "-eth" + str(i) + " qos=@newqos -- \
		    		--id=@newqos create QoS type=linux-htb other-config:max-rate=7500000 queues=0=@q0,1=@q1 -- \
			    	--id=@q0 create Queue other-config:min-rate=0 other-config:max-rate=4000000 -- \
			    	--id=@q1 create Queue other-config:min-rate=0 other-config:max-rate=2000000"
			print(cmd)
			h1.popen(cmd)
			i += 1
		time.sleep(1)
	"""

	#"""
	for sw in lower_switches:
		i = 1
		while i <= 7:
			"""
			cmd = "ovs-vsctl -- set Port " + sw + "-eth" + str(i) + " qos=@newqos -- \
		    		--id=@newqos create QoS type=linux-htb other-config:max-rate=3000000 queues=0=@q0,1=@q1 -- \
			    	--id=@q0 create Queue other-config:min-rate=0 other-config:max-rate=2000000 -- \
			    	--id=@q1 create Queue other-config:min-rate=0 other-config:max-rate=1000000"

			print("[ %s ]" % cmd)
			h1.popen(cmd)
			"""
			"""
			cmd = "ovs-vsctl -- set Port " + sw + "-eth" + str(i) + " qos=@newqos\
		    	-- --id=@newqos create QoS type=linux-htb other-config:max-rate=5000000 queues=1=@q1\
					-- --id=@q1 create Queue other-config:min-rate=1000 other-config:max-rate=3000000"
			print(cmd)
			h1.popen(cmd)
			"""
			i+=1
		#time.sleep(1)
	#"""

	
	"""
	print("start ping for 30 times")

	i = 0
	transfer_i = 3
	while i < transfer_i:
		cmd = 'ping 10.0.0.' + str(i+1) + ' -c 5'
		print(cmd)
		net.get('h'+str(i+5)).popen(cmd, shell=True)
		print("sleep %d secs" % t)
		time.sleep(t)
		i += 1
	"""

	#"""
	i = 0
	transfer_i = 3
	while i < transfer_i:
		"""
		cmd = 'iperf -s -i 1 -u > ~/h' + str(i+1) + '_UDP'
		print(cmd)
		net.get('h'+str(i+1)).popen(cmd, shell=True)
		"""

		#"""
		cmd = 'iperf -s -i 1 > ~/h' + str(i+1) + '_TCP'
		print(cmd)
		net.get('h'+str(i+1)).popen(cmd, shell=True)
		#"""

		#cmd = 'cd && python -m SimpleHTTPServer'
		i += 1
	

	print("sleep %d secs" % t)
	time.sleep(t)

	i = 0
	while i < transfer_i:
		"""
		cmd = 'iperf -c 10.0.0.' + str(i+1) + ' -i 1 -u -b 20M -t 10 > ~/h' + str(i+5) + '_UDP'
		print(cmd)
		net.get('h'+str(i+5)).popen(cmd, shell=True)
		"""

		#"""
		cmd = 'iperf -c 10.0.0.' + str(i+1) + ' -i 2 -t 20 > ~/h' + str(i+5) + '_TCP'
		print(cmd)
		net.get('h'+str(i+5)).popen(cmd, shell=True)

		#"""

		#cmd = 'iperf -c 10.0.0.' + str(i+1) + ' -i 1 -t 10 > ~/h' + str(i+5)
		#cmd = 'wget -O h' + str(i+5) + ' 127.0.0.1:8000/test10Mb.db > ~/h' + str(i+5)
		#cmd = 'whoami > ~/h1'

		i += 1
	#time.sleep(20)
	#"""


	CLI(net)
	net.stop()

if __name__ == '__main__':
	setLogLevel( 'info' )
	myNetwork()
	#h1, h2, h3, h4, h5, h6, h7, h8 = net.get('h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'h7', 'h8')


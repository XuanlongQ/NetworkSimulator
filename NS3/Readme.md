#this file is about NS3 simulation,may be i will join a part of emulator
at first section i will learning linux part
拓樸為(3+3)個switch的fat tree

1. 原始STP場景
使用ryu controller自帶的STP(Spanning Tree Protocol)
Note 1: 使用這樣的場景在開始傳送前必須等待ryu的STP程序完成, 以(3+3)的fat tree來說需要大約35秒(保險一點的話是40秒或更久). 

Note 2: 這個STP程序完成前, "不可以"開始傳輸, 否則會出現以下兩種情形其中一個(A)單純傳輸失敗(B)產生廣播風暴, 導致傳輸失敗 

Note 3: 出現(A)時只要把那35秒等待完即可重新傳輸, 出現(B)的話則必須關閉controller(ctrl+c按住直到全部結束), 並重啟controller, 而且必須重新等待完整的35秒

結果顯示STP協議如預期的把多路徑的拓樸變回單路徑, 於是搶路情形發生, iperf結果在~/h{$host_name}


3. 由SDN controller來減輕搶路問題，運行方法如下，iperf結果也在~/h{$host_name}
(1) 在一個terminal執行
   sudo python dr_scn_auto_stp.py
(2) 盡可能快的在另一個terminal執行(因為mininet在運行一段時間後就會開始傳輸)
   ryu-manager simple_switch_stp_13.py --observe-links

Note 1: 如果出現找不到路徑的error，表示mininet沒有準備好，中止mininet並sudo mn -c清掉OVS然後重新開始

由於每組傳輸間有3條路徑，當有三組傳輸同時進行時可以完全解決搶路問題。當使用更複雜的拓樸並增加同時傳輸的數量，效果會更顯著



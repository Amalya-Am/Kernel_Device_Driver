This is implementation of Linux kernel device driver that creates /dev/loop device that loops the input into /tmp/output file with in a hex format.

To create /dev/loop:
Run:  
	make
	sudo insmod my_loop_device.ko
	sudo chmod 666 /dev/loop

To test:
Run:
	sudo cat my_file.bin > /dev/loop
	hexdump -ve '16/1 "%02X" "\n"' my_file.bin | awk '{$1=$1;print}' > /tmp/output1
	diff /tmp/output /tmp/output1

To clean:
Run:
	make clean
	sudo rmmod my_loop_device.ko
	sudo rm /tmp/output1
	 

#!/bin/sh
#
# Copyright (c) 2019, The Linux Foundation. All rights reserved.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
[ -e /lib/smp_affinity_settings.sh ] && . /lib/smp_affinity_settings.sh

perf_setup(){

	if [ -d "/sys/kernel/debug/ath11k" ]; then
		local board=$(ipq806x_board_name)
		#read the to check whether nss_offload is enabled or not if not then set 0
		enable_nss_offload=$(cat /sys/module/ath11k/parameters/nss_offload)

		if [ "$enable_nss_offload" -eq 0 ]; then
			/etc/init.d/qca-nss-ecm stop
		fi

		if [ "$board" != "ap-hk14" && "$board" != "ap-hk10-c2" ]; then
			[ -d "/sys/class/net/wlan0" ] && echo e > /sys/class/net/wlan0/queues/rx-0/rps_cpus
			[ -d "/sys/class/net/wlan1" ] && echo e > /sys/class/net/wlan1/queues/rx-0/rps_cpus
			[ -d "/sys/class/net/wlan2" ] && echo e > /sys/class/net/wlan2/queues/rx-0/rps_cpus
		fi

		[ -d "/proc/sys/dev/nss/n2hcfg/" ] && echo 2048 > /proc/sys/dev/nss/n2hcfg/n2h_queue_limit_core0
		[ -d "/proc/sys/dev/nss/n2hcfg/" ] && echo 2048 > /proc/sys/dev/nss/n2hcfg/n2h_queue_limit_core1

		if [ "$board" == "ap-hk10-c2" ]; then
			[ -d "/proc/sys/dev/nss/rps/" ] && echo 15 > /proc/sys/dev/nss/rps/hash_bitmap
		else
			[ -d "/proc/sys/dev/nss/rps/" ] && echo 14 > /proc/sys/dev/nss/rps/hash_bitmap
		fi
	fi
}

perf_setup

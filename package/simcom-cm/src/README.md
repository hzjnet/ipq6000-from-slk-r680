LTE&5G setup datacall tool.

## Version History


1. V20200909_002
    Fixed a crash when using udhcpc.
    Add RAWIP for DHCP
2. V20200909_005
    modift apn IP/IPV4V6
    Fiexed -d parameter problem


## Usage: simcom-cm [options]

Examples:
Start network connection no parameter
	simcom-cm
Start network connection with PDP apn(default IPV4)
	simcom-cm -s CMNET
Start network connection with PDP apn(default IPV6)
	simcom-cm -6 -s CMNET
Start network connection with PDP apn(default IPV4V6)
	simcom-cm -4 -6 -s CMNET
Start network connection with PDP Context ID(Check the ID by the CGDCONT read command) and PDP apn
	simcom-cm -n 6 -s CMNET
Start network connection with PDP apn and auth
	simcom-cm -s 3gnet test 1234 0 -p 1234 -f log.txt

Options:
-s, [apn [user password auth]]   Set apn/user/password/auth get from your network provider
-f, logfilename                  Save log message of this program to file
-d, device path                  Specify device path(like /dev/cdc-wdm0)
-i, network interface            Specify network interface(default auto-detect
-n, Use connection profile       The range of permitted values (minimum value = 1) is returned by the test form of the CGDCONT command
-4, Set ip-family                IPv4 protocol
-6, Set ip-family                IPv6 protocol
-h, help                         display this help text

/*
	set ipq4029's mac address v1.0
*/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <mtd/mtd-user.h>
#include <sys/stat.h>
#include <sys/mman.h>

char mtd[20]="/dev/mmcblk0p15";

int nhx_set_mac(int sd, char *mac, int size)
{
	int fd;
	char *mmap_area = MAP_FAILED;
	struct mtd_info_user mtdInfo;
	int mtdsize = 0;

	fd = open(mtd, O_RDWR | O_SYNC);
	if (fd) {
		mtdsize = 262144;
		mmap_area = (char *) mmap(
		
			NULL, mtdsize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, fd, 0);
	} else {
		fprintf(stderr, "open %s error\n", mtd);
		return -1;
	}

	if(mmap_area != MAP_FAILED) {
		memcpy(mmap_area + sd, mac, size);
		msync(mmap_area, mtdsize, MS_SYNC);
		fsync(fd);
		munmap(mmap_area, mtdsize);
	}
	close(fd);
	return 0;
}

int nhx_get_mac(int sd,char *mac, int size)
{
	int fd;
	char *mmap_area = MAP_FAILED;
	fd = open(mtd, O_RDWR | O_SYNC);
	if (fd) {
		mmap_area = (char *) mmap(
			NULL, 0x10000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, fd, 0);
	} else {
		fprintf(stderr, "open %s error\n", mtd);
		return -1;
	}
	if(mmap_area != MAP_FAILED) {
	memcpy(mac, mmap_area + sd, size);
	munmap(mmap_area, 0x10000);
	}
	close(fd);
	return 0;
}

int macstr_to_hex(unsigned char *hex, const unsigned char *str)
{
	int i = 0;
	unsigned char c;
	const unsigned char *p;
	if (NULL == hex || NULL == str)
	{
		goto err;
	}

	p = str;

	for (i = 0; i < 6; i++)
	{
		if (*p == '-' || *p == ':')
		{
			p++;
		}
		
		if (*p >= '0' && *p <= '9')
		{
			c  = *p++ - '0';
		}
		else if (*p >= 'a' && *p <= 'f')
		{
			c  = *p++ - 'a' + 0xa;
		}
		else if (*p >= 'A' && *p <= 'F')
		{
			c  = *p++ - 'A' + 0xa;
		}
		else
		{
			goto err;
		}

		c <<= 4;	/* high 4bits of a byte */

		if (*p >= '0' && *p <= '9')
		{
			c |= *p++ - '0';
		}
		else if (*p >= 'a' && *p <= 'f')
		{
			c |= (*p++ - 'a' + 0xa);
		}
		else if (*p >= 'A' && *p <= 'F')
		{
			c |= (*p++ - 'A' + 0xa);
		}
		else
		{
			goto err;
		}
		
		hex[i] = c;
	}

	return 0;

err:
	return -1;
}

int main(int argc, char* argv[])
{
	unsigned char mac[6];
	unsigned char hmac[6];
	memset(mac, 0, 6);
	memset(hmac, 0, 6);
	if (argc == 3) {
		macstr_to_hex(hmac,argv[2]);
		if(strcmp(argv[1],"eth0")==0){
			nhx_set_mac(0,hmac, 6);
			nhx_get_mac(0,mac, 6);
			printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		}
		else if(strcmp(argv[1],"eth1")==0){
			nhx_set_mac(6,hmac, 6);
			nhx_get_mac(6,mac, 6);
			printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		}
		else if(strcmp(argv[1],"eth2")==0){
			nhx_set_mac(12,hmac, 6);
			nhx_get_mac(12,mac, 6);
			printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		}
		else if(strcmp(argv[1],"eth3")==0){
			nhx_set_mac(18,hmac, 6);
			nhx_get_mac(18,mac, 6);
			printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		}
		else if(strcmp(argv[1],"eth4")==0){
			nhx_set_mac(24,hmac, 6);
			nhx_get_mac(24,mac, 6);
			printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		}
		else if(strcmp(argv[1],"eth5")==0){
			nhx_set_mac(30,hmac, 6);
			nhx_get_mac(30,mac, 6);
			printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		}
		else{
			printf("error\r\n");
		}		
		
		
	}else if (argc == 2){ 
		 if(strcmp(argv[1],"eth0")==0){
			nhx_get_mac(0,mac, 6);
			printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		}
		else if(strcmp(argv[1],"eth1")==0){
			nhx_get_mac(6,mac, 6);
			printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		}
		else if(strcmp(argv[1],"eth2")==0){
			nhx_get_mac(12,mac, 6);
			printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		}
		else if(strcmp(argv[1],"eth3")==0){
			nhx_get_mac(18,mac, 6);
			printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		}
		else if(strcmp(argv[1],"eth4")==0){
			nhx_get_mac(24,mac, 6);
			printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		}
		else if(strcmp(argv[1],"eth5")==0){
			nhx_get_mac(30,mac, 6);
			printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		}
		else{
			printf("error\r\n");
		}
	}else{
		nhx_get_mac(0,mac, 6);
		printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	}

	return 0;
}
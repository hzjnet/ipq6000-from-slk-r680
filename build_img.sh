#!/bin/bash

echo "Build ipq image"
path=$(pwd)

if [ ! -d $path/bin/ipq/img/ ];then
	mkdir $path/bin/ipq/img
fi

rm -rf /home/qsdk/Haohua/Build_qca_image/common/build/ipq/openwrt*

cp bin/ipq/openwrt* /home/qsdk/Haohua/Build_qca_image/common/build/ipq  &&

cd /home/qsdk/Haohua/Build_qca_image/common/build  &&

export BLD_ENV_BUILD_ID=P  &&

python update_common_info.py  &&

echo "Copy bin/* to $path/bin/ipq/img"

cp -rf bin/* $path/bin/ipq/img/

echo "Build END!"
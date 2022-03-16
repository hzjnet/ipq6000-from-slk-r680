#
# Copyright (C) 2008-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=quectel-gobinet
PKG_RELEASE:=1.6.2.9

include $(INCLUDE_DIR)/package.mk

define KernelPackage/quectel-gobinet
  CATEGORY:=Quectel
  SECTION:=kernel
  TITLE:=GobiNet
  DEPENDS:= +kmod-usb-net
  FILES:=$(PKG_BUILD_DIR)/GobiNet.ko
  KCONFIG:=
endef

define KernelPackage/quectel-gobinet/description
	Goninet
endef

MAKE_OPTS:= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)" \
	SUBDIRS="$(PKG_BUILD_DIR)"

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

define KernelPackage/quectel-gobinet/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/quectel-gobinet.init $(1)/etc/init.d/quectel-gobinet
endef

$(eval $(call KernelPackage,quectel-gobinet))

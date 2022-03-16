#
# Copyright (C) 2006-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=quectel-CM
PKG_RELEASE:=1.6.0.16
include $(INCLUDE_DIR)/package.mk

define Package/quectel-CM
  SECTION:=utils
  CATEGORY:=Quectel
  DEPENDS:=+libpthread
  TITLE:=quectel-CM
endef

define Package/quectel-CM/description
 quectel-CM.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/quectel-CM/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/quectel-CM $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/etc/hotplug.d/net
	$(INSTALL_BIN) ./files/quectel-CM.hotplug $(1)/etc/hotplug.d/net/99-quectel-CM
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./files/quectel.default $(1)/etc/uci-defaults/99-quectel
endef

$(eval $(call BuildPackage,quectel-CM))

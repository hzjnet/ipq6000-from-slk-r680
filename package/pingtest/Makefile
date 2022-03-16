#
# Copyright (C) 2006-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=pingtest
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/pingtest
  CATEGORY:=Examples
  TITLE:=ping test
endef

define Package/pingtest/description
 SET MAC.
endef

# ifeq ($(CONFIG_TARGET_ipq_ipq40xx),)
  # MAKE_CFLAGS += IPQ40XX
# endif

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/pingtest/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/pingtest $(1)/sbin/
endef

$(eval $(call BuildPackage,pingtest))


include $(TOPDIR)/rules.mk

PKG_NAME:=nhxfac
PKG_VERSION:=1.0
PKG_RELEASE:=20210422

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=NHX
  CATEGORY:=NHX
  TITLE:=Factory tools
  PKGARCH:=all
  SUBMENU:=Fac
endef

define Package/$(PKG_NAME)/description
NHX Factory scripts.
endef

define Build/Prepare
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(CP) ./files/nhxfac $(1)/usr/sbin
endef

$(eval $(call BuildPackage,$(PKG_NAME)))

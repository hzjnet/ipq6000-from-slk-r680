include $(TOPDIR)/rules.mk

PKG_NAME:=slkmac
PKG_VERSION:=1.0
PKG_BUILD_DIR:= $(BUILD_DIR)/$(PKG_NAME)


include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/nls.mk

define Package/slkmac
  CATEGORY:=Examples
  TITLE:=slkmac sdk
  DEPENDS:=
endef

define Package/slkmac/description
	If you can't figure out what this program does, you're probably  
	brain-dead and need immediate medical attention.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/slkmac/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/slkmac $(1)/usr/sbin/
endef


$(eval $(call BuildPackage,slkmac))

include $(TOPDIR)/rules.mk

PKG_NAME:=simcom-cm
PKG_VERSION:=2.0
PKG_BUILD_DIR:= $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/nls.mk

define Package/simcom-cm
CATEGORY:=Examples
TITLE:=simcom-cm sdk
 DEPENDS:=+libpthread 
endef

define Package/simcom-cm/description
	If you can't figure out what this program does, you're probably  
	brain-dead and need immediate medical attention.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/simcom-cm/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/simcom-cm $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,simcom-cm))

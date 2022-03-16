include $(TOPDIR)/rules.mk

PKG_NAME:=speedtest
PKG_VERSION:=1.0
PKG_RELEASE:=2020.05.08

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)
include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/$(PKG_NAME)
  SECTION:=NHX
  CATEGORY:=NHX
  TITLE:=speedtest
  DEPENDS:=+libopenssl +libcurl +ca-bundle +ca-certificates +libxml2 +libstdcpp
  PKGARCH:=all
  URL:=https://github.com/taganaka/SpeedTest
  SUBMENU:=Network
endef

define Package/$(PKG_NAME)/description
SpeedTest.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

CMKAE_OPTIONS += -DCMAKE_BUILD_TYPE=Release
TARGET_CFLAGS += -D_GNU_SOURCE -I$(STAGING_DIR)/usr/include/ -I$(STAGING_DIR)/usr/include/libxml2

MAKE_FLAGS += \
	FPIC="$(FPIC)" \
	CFLAGS="$(TARGET_CFLAGS)" \
	LDFLAGS="$(TARGET_LDFLAGS)" \
	LIBS="-lcurl -lssl -lcrypto -lxml2"

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/SpeedTest $(1)/usr/bin/SpeedTest
endef

$(eval $(call BuildPackage,$(PKG_NAME)))

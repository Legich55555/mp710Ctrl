# 
# Copyright (C) 2015 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
# This is free software, licensed under the GNU General Public License v2.
#
include $(TOPDIR)/rules.mk

PKG_NAME:=mp710Ctrl
PKG_VERSION:=0.01
PKG_RELEASE:=1
PKG_MAINTAINER:=Oleg <legich55555@gmail.com>
PKG_LICENSE:=GPL-2.0
PKG_LICENSE_FILES:=LICENSE


### This configuration is for getting the sources from local folder
#PKG_SOURCE_URL:=file:///home/oleg/Development/mp710Ctrl
#PKG_SOURCE_VERSION:=88cb2b46a439e894d85595c135a8cb4781860ed2

# This configuration is for getting the sources from GitHub
PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/Legich55555/mp710Ctrl.git
PKG_SOURCE_VERSION:=88cb2b46a439e894d85595c135a8cb4781860ed2
PKG_SOURCE:=$(PKG_NAME)-$(PKG_SOURCE_VERSION).tar.gz
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_SOURCE_VERSION)
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_SOURCE_VERSION)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/mp710Ctrl
	SECTION:=multimedia
	CATEGORY:=Multimedia
	TITLE:=Tool for MasterKit MP710
	DEPENDS:=+libusb-1.0 +libpthread
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)
#	$(STRIP) $(PKG_BUILD_DIR)/$(PKG_NAME)
endef

define Build/Download
#	$(MAKE) -C $(PKG_BUILD_DIR)
#	$(STRIP) $(PKG_BUILD_DIR)/$(PKG_NAME)
endef

define Package/mp710Ctrl/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mp710WebCtrl/mp710WebCtrl $(1)/usr/bin/
#	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mp710Sunrise/mp710Sunrise $(1)/usr/bin/
	$(INSTALL_DIR) $(1)/etc/config/mp710Ctrl
	$(CP) $(PKG_BUILD_DIR)/mp710WebCtrl/html $(1)/etc/config/mp710Ctrl
	$(CP) ./files/mp710WebCtrl.config $(1)/etc/config/mp710WebCtrl
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/mp710WebCtrl.init $(1)/etc/init.d/mp710WebCtrl
	
endef

$(eval $(call BuildPackage,mp710Ctrl))


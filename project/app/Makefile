
MAKEFILE_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
export LC_ALL=C
SHELL:=/bin/bash
MAKE=make
AT?=@

################################################################################
##   Platform Specified  Configuraton
################################################################################
include $(MAKEFILE_DIR)/Makefile.param

################################################################################
##   Public Configuraton
################################################################################

app_src := $(wildcard ./*/Makefile ./component/fastboot_server/Makefile)
app_src := $(dir $(app_src))

all: $(pkg-build)
	@echo ">>> Entrando en: $(app_src)"
	+	$(foreach target,$(app_src),$(MAKE) -C $(target) || exit -1;)
	$(call MAROC_COPY_PKG_TO_APP_OUTPUT, $(RK_PROJECT_PATH_APP), $(RK_APP_OUTPUT))

distclean: clean

clean:
	+	$(foreach target,$(app_src),$(MAKE) clean -C $(target) || exit -1;)
	@rm -rf $(RK_PROJECT_PATH_APP)

info:
	+	@$(foreach target,$(app_src),$(MAKE) -C $(target) info || exit 0;)

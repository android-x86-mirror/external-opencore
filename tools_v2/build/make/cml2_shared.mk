# Get the current local path as the first operation
SHARED_LIB_LOCAL_PATH := $(call get_makefile_dir)


# create a rule to install the CML2 config header
$(eval $(call INST_TEMPLATE,pv_config.h,$(abspath $(CFG_DIR)),$(INCDESTDIR)))

$(INCDESTDIR)/ALL_HDRS_INSTALLED: $(INCDESTDIR)/pv_config.h


define process_aggregate_shared_lib_target
$(eval CUMULATIVE_TARGET_LIST:= $(call clearvar))
$(eval CUMULATIVE_ANDROID_MK_PATH := $(call clearvar))
$(eval COMPONENT_MAKEFILES:= $(patsubst %,$(SRC_ROOT)%,$(patsubst %,%/local.mk,$(call remove_quotes,$(SOLIBDIRS_$1)))))
$(eval SOLIB:=$1)
$(eval include $(COMPONENT_MAKEFILES))
$(eval $1_CUMULATIVE_TARGET_LIST := $(CUMULATIVE_TARGET_LIST))
$(eval $1_CUMULATIVE_MAKEFILES := $(strip $(CUMULATIVE_ANDROID_MK_PATH)))
endef

ifndef shared_lib_template
define shared_lib_template
# Pseudo-targets for libraries. With this, we can use "make lib$(TARGET)" instead of "make $(DESTDIR)/lib%$(TARGET_NAME_SUFFIX).$(LIB_EXT)"
# # E.g., make libpvplayer
lib$(1): $(DESTDIR)/lib$(1)$(TARGET_NAME_SUFFIX).$(SHARED_LIB_EXT)
$(DESTDIR)/lib$(1)$(TARGET_NAME_SUFFIX).$(SHARED_LIB_EXT): $(foreach lib,$($(1)_CUMULATIVE_TARGET_LIST),$($(lib)_$(1)_fullname)) $(foreach shlib,$(strip $(call remove_quotes,$(MODS_$(1)))),$(patsubst -l%,$(DESTDIR)/lib%$(TARGET_NAME_SUFFIX).$(SHARED_LIB_EXT),$(shlib)))
	$(eval $(1)_fullname:=$(DESTDIR)/lib$1$(TARGET_NAME_SUFFIX).$(SHARED_LIB_EXT))
	@echo "Building $$@..."
	$$(call create_objdir,$$(@D))
	$$(call generate_shared_lib,$$@,$$^,$(strip $($(1)$(TARGET_NAME_SUFFIX)_PRELINK)))
	@echo "DONE building $$@."
endef
endif

# process the elements of TARGET_shared
SHARED_LIB_TARGET_LIST := $(call remove_quotes,$(TARGET_shared))
# process the elements of TESTAPPS
APP_TARGET_LIST := $(call remove_quotes,$(TESTAPPS))
#process the elements of LIBS-static
STATIC_LIBS_TARGET_LIST := $(patsubst -l%,%,$(call remove_quotes,$(LIBS_static)))


#################
# set the default library mode to shared-archive
DEFAULT_LIBTYPE := shared-archive
$(strip $(foreach target,$(SHARED_LIB_TARGET_LIST),$(call process_aggregate_shared_lib_target,$(target))))

$(foreach lib,$(SHARED_LIB_TARGET_LIST),$(eval $(call shared_lib_template,$(lib))))
SHARED_LIB_FULLNAMES := $(patsubst %,$(DESTDIR)/%, $(patsubst %,lib%$(TARGET_NAME_SUFFIX).$(SHARED_LIB_EXT),$(SHARED_LIB_TARGET_LIST)))

#################
define lib_include_template
	$(eval $1_MAKEFILE := $(patsubst %,$(SRC_ROOT)%,$(patsubst %,%/local.mk,$1)))
	$(eval include $($1_MAKEFILE))
endef

# set the default library mode to static [Is this needed?]
DEFAULT_LIBTYPE := static
$(foreach mk,$(strip $(call remove_quotes,$(LIBDIR_static))),$(eval $(call lib_include_template,$(mk))))
ALL_LIBS := $(patsubst %,$(DESTDIR)/%, $(patsubst %,lib%$(TARGET_NAME_SUFFIX).$(STAT_LIB_EXT),$(STATIC_LIBS_TARGET_LIST)))

#################
define mk_include_template
	$(eval $1_MAKEFILE := $(patsubst %,$(SRC_ROOT)%,$(patsubst %,%/local.mk,$(call remove_quotes,$(TESTAPP_DIR_$1)))))
	$(eval include $($1_MAKEFILE))
endef

# include test app makefiles 
$(foreach mk,$(strip $(call remove_quotes,$(APP_TARGET_LIST))),$(eval $(call mk_include_template,$(mk))))
ALL_BINS := $(patsubst %,$(BUILD_ROOT)/bin/$(OUTPUT_DIR_COMPONENT)/%$(TARGET_EXT),$(APP_TARGET_LIST))
#################


CMD_COMPLETION_TARGETS += $(patsubst %,lib%,$(SHARED_LIB_TARGET_LIST) $(STATIC_LIBS_TARGET_LIST)) $(APP_TARGET_LIST)

default: all 

all: shared static $(ALL_BINS)

shared: $(SHARED_LIB_FULLNAMES) 
static: $(ALL_LIBS)



LOCAL_PATH := $(SHARED_LIB_LOCAL_PATH)
AGGREGATE_LIBS_MAKEFILE := 1
-include $(PLATFORM_EXTRAS)


# ----------------------------------------------------------------------
#				CML2 Configuration
# ----------------------------------------------------------------------
#
# These variables could be set in the environment or set as an argument to make.
# START_CONFIG  - path of start file which specifies the start menu of the component.
# CML2_FLAGS    - flags passed with cml2 configuration.
# CML2_CONF_OUT - cml2 config output file.
# CML2_CONF_IN  - cml2 config input file.
# GUI           - to force X Mode set the flag to y.

ifeq ($(START_CONFIG),)
  START_CONFIG  = $(SRC_ROOT)/tools_v2/build/package/cml2/pvconfig_rules
endif

ifeq ($(CML2_CONF_OUT),)
  CML2_CONF_OUT = $(START_CONFIG)/pv_config.mk
endif

CML2_FLAGS += -o $(CML2_CONF_OUT)

# Default configuration in X mode for win32
ifeq ($(BUILD_ARCH),win32)
  GUI = y
endif

# Set X mode if GUI set to y
ifeq ($(GUI),y)
  GUI_FLAG += -x
endif

CONF_IN_FOUND := $(wildcard config.in)

ifeq ($(strip $(CML2_CONF_IN)),)
  CML2_CONF_IN = $(CONF_IN_FOUND)
endif

ifneq ($(CML2_CONF_IN),)
  CML2_CONF_IN_FLAG = -i $(CML2_CONF_IN)
  CML2_CONF_IN_FLAG_SILENT = -I $(CML2_CONF_IN)
endif

# Compile CML2 rule base.
cml2-compile:
	@python $(SRC_ROOT)/tools_v2/build/package/cml2/src/cmlcompile.py $(START_CONFIG)/start.cml

# Run CML2 tool to display the menu.
menu-config: cml2-compile
	@python $(SRC_ROOT)/tools_v2/build/package/cml2/src/cmlconfigure.py $(CML2_FLAGS) $(GUI_FLAG)

# Run CML2 tool to display the menu and use pre-configure file config.in (link) as an input.
old-config: cml2-compile
	@echo Old configuration. Input from $(CML2_CONF_IN)
	@python $(SRC_ROOT)/tools_v2/build/package/cml2/src/cmlconfigure.py $(CML2_FLAGS) $(CML2_CONF_IN_FLAG) $(GUI_FLAG)

# Run CML2 tool and use pre-configure file config.in (link) as an input without any user intervention.
silent-config: cml2-compile
	@echo Silent configuration. Input from $(CML2_CONF_IN)
	@python $(SRC_ROOT)/tools_v2/build/package/cml2/src/cmlconfigure.py $(CML2_FLAGS) $(CML2_CONF_IN_FLAG_SILENT)

.PHONY:: cml2-compile menu-config old-config silent-config
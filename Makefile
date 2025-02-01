###################
### NUTTX BUILD ###
###################
ifneq ($(APPDIR),)

MENUDESC = "hysim"

include $(APPDIR)/Directory.mk

#############################
### REGULAR DESKTOP BUILD ###
#############################
else

SIMULATIONS = control_client pad_server telem_client

.PHONY: $(SIMULATIONS)

all: $(SIMULATIONS)

$(SIMULATIONS):
	$(MAKE) -C $(abspath $@)

clean:
	for sim in $(SIMULATIONS); do $(MAKE) -C $$sim clean; done

endif

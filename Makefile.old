SIMULATIONS = control_client pad_server telem_client

.PHONY: $(SIMULATIONS)

all: $(SIMULATIONS)

$(SIMULATIONS):
	$(MAKE) -C $(abspath $@)

clean:
	for sim in $(SIMULATIONS); do $(MAKE) -C $$sim clean; done

EXPERIMENTS := $(notdir $(shell find . -maxdepth 1 -type d))
EXPERIMENTS := $(filter-out ., $(EXPERIMENTS))
EXPERIMENTS := $(filter-out utils, $(EXPERIMENTS))
EXPERIMENTS := $(filter-out .git, $(EXPERIMENTS))

CLEAN-RULES:=${EXPERIMENTS:%=%-clean}
ALL-RULES:=${EXPERIMENTS:%=%-all}

$(info "modules to build: ${EXPERIMENTS}")

all: ${ALL-RULES}

clean: ${CLEAN-RULES}

${CLEAN-RULES}:%-clean:%
	make -C $^ clean

${ALL-RULES}:%-all:%
	make -C $^

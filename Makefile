# Distributed Systems - Home Work 2
#
# Delegates to the per-question Makefiles. Run "make help" for the list.

.PHONY: all q2 q4 q7 test bench clean help

## build all three questions
all: q2 q4 q7

q2:
	$(MAKE) -C Q2

q4:
	$(MAKE) -C Q4

q7:
	$(MAKE) -C Q7

## correctness check for both
test:
	$(MAKE) -C Q2 test
	$(MAKE) -C Q4 test
	$(MAKE) -C Q7 test

## timing sweep for both
bench:
	$(MAKE) -C Q2 bench
	$(MAKE) -C Q4 bench
	$(MAKE) -C Q7 bench

## remove binaries and generated data from both
clean:
	$(MAKE) -C Q2 clean
	$(MAKE) -C Q4 clean
	$(MAKE) -C Q7 clean

help:
	@echo "Distributed Systems - Home Work 2"
	@echo ""
	@echo "  make          build Q2, Q4 and Q7"
	@echo "  make q2       build Q2 only          (or: cd Q2 && make)"
	@echo "  make q4       build Q4 only"
	@echo "  make q7       build Q7 only"
	@echo "  make test     correctness, all three"
	@echo "  make bench    timing sweep, all three"
	@echo "  make clean    remove binaries and generated data"
	@echo ""
	@echo "Each question has its own Makefile with more targets:"
	@echo "  cd Q4 && make help"
	@echo ""
	@echo "On the cluster first:  module purge && module load openmpi/4.1.5"

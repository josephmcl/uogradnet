#
# chatlog: the UOGradNet logger bot.
#
#   make          build ./chatlog
#   make clean    remove objects
#
# Put CC=clang (or similar) in a .env file to change the compiler.
#

-include .env

target = chatlog

cc = $(or $(CC),cc)

source_ext = c
header_directory = include
source_directory = source
build_directory  = build
object_directory = $(build_directory)/object
binary_directory = .

sources = $(wildcard $(source_directory)/*.$(source_ext))
headers = $(wildcard $(header_directory)/*.h)
objects = $(sources:$(source_directory)/%.$(source_ext)=$(object_directory)/%.o)

postgres_include = $(shell pg_config --includedir 2>/dev/null)

rm = rm -f

gccflags = -Wall -Wextra -Wpedantic -std=c99 -g -O2
compiler_flags := $(gccflags)
includes := -I$(header_directory) $(if $(postgres_include),-I$(postgres_include))
libraries := -lpq

define speaker
	@echo [make:$$PPID] $(1)
	@$(1)
endef

all: $(binary_directory)/$(target)

$(binary_directory)/$(target): $(objects)
	$(call speaker,\
	$(cc) $(objects) -o $@ $(libraries))

$(objects): $(object_directory)/%.o: $(source_directory)/%.$(source_ext) $(headers)
	@mkdir -p $(object_directory)
	$(call speaker,\
	$(cc) $(compiler_flags) -c $< -o $@ $(includes))

.PHONY: all clean
clean:
	@$(rm) $(objects)
	@$(rm) $(binary_directory)/$(target)

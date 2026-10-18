#
# UOGradNet
#
#   make              build the chatlog bot
#   make check        build, then check what can be checked without secrets
#   make preview      serve www/ on http://localhost:8000 to look at the pages
#   sudo make deploy  install everything on the server
#
# First time on a fresh VM, run `sudo sh deploy/bootstrap.sh` once (README).
#

chatlog_directory = chatlog
web_directory     = www
deploy_directory  = deploy

define speaker
	@echo [make:$$PPID] $(1)
	@$(1)
endef

all: chatlog

.PHONY: chatlog
chatlog:
	@$(MAKE) --no-print-directory -C $(chatlog_directory)

.PHONY: check
check: chatlog
	$(call speaker,\
	sh $(deploy_directory)/check.sh)

.PHONY: preview
preview:
	$(call speaker,\
	python3 -m http.server --directory $(web_directory) 8000)

.PHONY: deploy
deploy: chatlog
	$(call speaker,\
	sh $(deploy_directory)/deploy.sh)

.PHONY: clean
clean:
	@$(MAKE) --no-print-directory -C $(chatlog_directory) clean

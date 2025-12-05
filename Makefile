VERSION ?= v0.1.0
BRANCH_DEV  = dev
BRANCH_MAIN = main
RETRIES ?= 3
SLEEP   ?= 2

.PHONY: help release commit push push_main merge tag test changelog return_dev

help:
	@echo "Available commands:"
	@echo "  make commit                    - Add and commit all files (on $(BRANCH_DEV) branch)"
	@echo "  make push                      - Push the $(BRANCH_DEV) branch (with retry)"
	@echo "  make push_main                 - Push the $(BRANCH_MAIN) branch (with retry)"
	@echo "  make merge                     - Merge $(BRANCH_DEV) into $(BRANCH_MAIN) and push"
	@echo "  make tag VERSION=vX.Y.Z        - Create and push a Git tag (default: $(VERSION), with retry)"
	@echo "  make release VERSION=vX.Y.Z    - Full release workflow"
	@echo "  make test                      - Run tests"
	@echo "  make changelog                 - Update CHANGELOG.md"

return_dev:
	@current=$$(git rev-parse --abbrev-ref HEAD); \
	if [ "$$current" != "$(BRANCH_DEV)" ]; then \
		echo "↩️  Returning to $(BRANCH_DEV)..."; \
		git checkout $(BRANCH_DEV); \
	fi

commit:
	@git checkout $(BRANCH_DEV)
	@if [ -n "$$(git status --porcelain)" ]; then \
		echo "📝 Committing changes on $(BRANCH_DEV)..."; \
		git add .; \
		git commit -m "chore(release): prepare $(VERSION)"; \
	else \
		echo "✅ Nothing to commit on $(BRANCH_DEV)."; \
	fi
	@$(MAKE) return_dev

push:
	@echo "⬆️  Pushing $(BRANCH_DEV) to origin (with retry, $(RETRIES)x max)..."
	@git checkout $(BRANCH_DEV)
	@n=0; \
	until [ $$n -ge $(RETRIES) ]; do \
	  if git push origin $(BRANCH_DEV); then \
	    echo "✅ Push of $(BRANCH_DEV) succeeded."; \
	    break; \
	  fi; \
	  n=$$((n+1)); \
	  echo "⚠️  Push failed. Retry $$n/$(RETRIES) in $(SLEEP)s..."; \
	  sleep $(SLEEP); \
	done; \
	if [ $$n -ge $(RETRIES) ]; then \
	  echo "❌ Push of $(BRANCH_DEV) failed after $(RETRIES) attempts."; \
	  exit 1; \
	fi
	@$(MAKE) return_dev

push_main:
	@echo "⬆️  Pushing $(BRANCH_MAIN) to origin (with retry, $(RETRIES)x max)..."
	@git checkout $(BRANCH_MAIN)
	@n=0; \
	until [ $$n -ge $(RETRIES) ]; do \
	  if git push origin $(BRANCH_MAIN); then \
	    echo "✅ Push of $(BRANCH_MAIN) succeeded."; \
	    break; \
	  fi; \
	  n=$$((n+1)); \
	  echo "⚠️  Push failed. Retry $$n/$(RETRIES) in $(SLEEP)s..."; \
	  sleep $(SLEEP); \
	done; \
	if [ $$n -ge $(RETRIES) ]; then \
	  echo "❌ Push of $(BRANCH_MAIN) failed after $(RETRIES) attempts."; \
	  exit 1; \
	fi
	@$(MAKE) return_dev

merge:
	@echo "🔀 Merging $(BRANCH_DEV) into $(BRANCH_MAIN)..."
	@git checkout $(BRANCH_MAIN)
	@git merge --no-ff --no-edit $(BRANCH_DEV)
	@$(MAKE) push_main
	@$(MAKE) return_dev

tag:
	@if git rev-parse $(VERSION) >/dev/null 2>&1; then \
		echo "❌ Tag $(VERSION) already exists."; \
		exit 1; \
	else \
		echo "🏷️  Creating annotated tag $(VERSION)..."; \
		git tag -a $(VERSION) -m "Release version $(VERSION)"; \
		echo "⬆️  Pushing tag $(VERSION) (with retry)..."; \
		n=0; \
		until [ $$n -ge $(RETRIES) ]; do \
		  if git push origin $(VERSION); then \
		    echo "✅ Tag $(VERSION) pushed successfully."; \
		    break; \
		  fi; \
		  n=$$((n+1)); \
		  echo "⚠️  Push tag failed. Retry $$n/$(RETRIES) in $(SLEEP)s..."; \
		  sleep $(SLEEP); \
		done; \
		if [ $$n -ge $(RETRIES) ]; then \
		  echo "❌ Pushing tag $(VERSION) failed after $(RETRIES) attempts."; \
		  exit 1; \
		fi; \
	fi
	@$(MAKE) return_dev

release:
	@$(MAKE) changelog
	@$(MAKE) commit
	@$(MAKE) push
	@$(MAKE) merge
	@$(MAKE) tag VERSION=$(VERSION)
	@$(MAKE) return_dev

test:
	cd build && ctest --output-on-failure

changelog:
	bash scripts/update_changelog.sh
	@$(MAKE) return_dev


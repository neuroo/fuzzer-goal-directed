include ../../rules/common.mk


LOC_DIST_DIR=../../$(DIST_DIR)/fuzzer/ui
STATIC_DIST_DIR=$(LOC_DIST_DIR)/static
NODE_MODULES_DIST_DIR=$(LOC_DIST_DIR)/node_modules

TARGET=dist/bundle.js

.PHONY: clean


all: clean prepare dependencies ui package

dependencies:
	npm install --loglevel=error

ui:
	webpack -p --config webpack.config.prod.js --progress --colors \
	        --display-error-details ----display-reasons

package:
	@cp -f ./index.html $(LOC_DIST_DIR)
	@cp -f ./dist/bundle.js $(STATIC_DIST_DIR)

	@mkdir -p $(NODE_MODULES_DIST_DIR)/materialize-css/bin
	@cp -f node_modules/materialize-css/bin/materialize.css $(NODE_MODULES_DIST_DIR)/materialize-css/bin
	@cp -f node_modules/materialize-css/bin/materialize.js $(NODE_MODULES_DIST_DIR)/materialize-css/bin
	@mkdir -p $(NODE_MODULES_DIST_DIR)/materialize-css/font/roboto
	@cp -f node_modules/materialize-css/font/roboto/* $(NODE_MODULES_DIST_DIR)/materialize-css/font/roboto/

	@mkdir -p $(NODE_MODULES_DIST_DIR)/jquery/dist
	@cp -f node_modules/jquery/dist/jquery.js $(NODE_MODULES_DIST_DIR)/jquery/dist

prepare:
	@mkdir -p $(LOC_DIST_DIR)
	@mkdir -p $(STATIC_DIST_DIR)
	@mkdir -p $(NODE_MODULES_DIST_DIR)

clean:
	@rm -rf $(LOC_DIST_DIR)

dev:
	webpack-dev-server --config webpack.config.js --colors

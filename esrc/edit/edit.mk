example-edit: $(BIN_DIR)/edit.com

$(BIN_DIR)/edit.com:	tools $(BIN_DIR)/edit.ihx
	$(LBIN_DIR)/load $(BIN_DIR)/edit

$(BIN_DIR)/edit.ihx:	libraries $(BIN_DIR)/edit.rel $(BIN_DIR)/edit.arf 
	$(CLD) $(CLD_FLAGS) -nf $(BIN_DIR)/edit.arf

$(BIN_DIR)/edit.rel: $(ESRC_DIR)/edit/edit.c
	$(CCC) $(CCC_FLAGS) -o $(BIN_DIR) $(ESRC_DIR)/edit/edit.c

$(BIN_DIR)/edit.arf:	$(BIN_DIR)/generic.arf
	$(QUIET)$(SED) 's/$(REPLACE_TAG)/edit/' $(BIN_DIR)/generic.arf > $(BIN_DIR)/edit.arf 

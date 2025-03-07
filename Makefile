CC := clang
CFLAGS := -Wall \
		  -I include \
		  -g
LDFLAGS := -lm -lreadline

SRC_DIR := src
BUILD_DIR := build

SRC := $(shell find $(SRC_DIR) -name '*.c')
OBJ := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))

EXEC := $(BUILD_DIR)/jash
INSTALL_DEST := /usr/bin/jash

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Pattern rule to create object files with directories
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(EXEC)
	@./$(EXEC)

install: $(EXEC)
	install -Dm755 $(EXEC) $(INSTALL_DEST)
	echo "$(INSTALL_DEST)" | sudo tee -a /etc/shell > /dev/null

clean:
	@rm -rf $(BUILD_DIR)
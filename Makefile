# Makefile for Dave's Network Inquisition
# Development Version: v1.0-dev-2025-01-14

CC = gcc
CFLAGS = `pkg-config --cflags gtk4 vte-2.91-gtk4` -Wall -O2
LIBS = `pkg-config --libs gtk4 vte-2.91-gtk4` -lm -lpthread
TARGET = network-inq
SOURCE = network-inq.c
INSTALL_DIR = /usr/local/bin
DESKTOP_DIR = /usr/share/applications

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(SOURCE) $(CFLAGS) $(LIBS) -o $(TARGET)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) $(INSTALL_DIR)/
	@echo "Creating desktop entry..."
	@echo "[Desktop Entry]" > network-inq.desktop
	@echo "Name=Dave's Network Inquisition" >> network-inq.desktop
	@echo "Comment=Network monitoring and diagnostic tool" >> network-inq.desktop
	@echo "Exec=$(INSTALL_DIR)/$(TARGET)" >> network-inq.desktop
	@echo "Icon=network-workgroup" >> network-inq.desktop
	@echo "Terminal=false" >> network-inq.desktop
	@echo "Type=Application" >> network-inq.desktop
	@echo "Categories=Network;System;GTK;" >> network-inq.desktop
	install -m 644 network-inq.desktop $(DESKTOP_DIR)/
	@echo "Installation complete! You can now run '$(TARGET)' from terminal"
	@echo "or find 'Dave's Network Inquisition' in your application menu."

uninstall:
	rm -f $(INSTALL_DIR)/$(TARGET)
	rm -f $(DESKTOP_DIR)/network-inq.desktop
	@echo "Uninstallation complete."

.PHONY: all clean install uninstall

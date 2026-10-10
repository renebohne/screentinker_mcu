TARGET := ../screentinker_sim

all: $(TARGET)

$(TARGET):
	@cd .. && pio run -e simulator

clean:
	@cd .. && pio run -e simulator -t clean
	@rm -f $(TARGET)

.PHONY: all clean

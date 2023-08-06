.PHONY: release debug clean

debug:
	@make -s -C src/ debug

release:
	@make -s -C src/ release

clean:
	@make -s -C src/ clean
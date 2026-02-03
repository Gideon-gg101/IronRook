.PHONY: all clean release

all: release

release:
	mkdir -p build
	cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_POPCNT=ON
	cd build && $(MAKE) -j$(shell nproc)
	cp build/IronRook .

clean:
	rm -rf build IronRook

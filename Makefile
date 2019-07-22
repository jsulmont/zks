all:
	make -C build all

clean:
	make -C build clean

relcmake:
	rm -rf build && mkdir build
	cd build && cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..

dbgcmake:
	rm -rf build && mkdir build
	cd build && cmake -DCMAKE_BUILD_TYPE=Debug -DSANITIZE_ADDRESS=ON ..

cmake: dbgcmake

re:
	rm -rf build && mkdir build
	cd build && cmake ..

docker:
	docker build -t jansulmont/zks .


docker-run:
	docker run --rm jansulmont/zks

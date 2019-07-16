all: upload  httpserver

httpserver: HttpServer.cpp ThreadPool.hpp Util.hpp
	g++ -o $@ $^  -std=c++11 -lpthread

upload: Upload.cpp
	g++ -o $@ $^  -std=c++11

.PHONY:clean
clean:
	rm -rf httpserver all upload

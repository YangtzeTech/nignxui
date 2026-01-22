.PHONY: deb clean install uninstall

deb:
	rm -rf build &&mkdir build
	cd build && cmake .. -DCMAKE_BUILD_TYPE=Release &&make -j$(nproc)
	cp build/nginxui package/usr/bin/
	dpkg-deb --build "package" "nginxui.deb"

clean:
	rm -rf build package/usr/bin/nginxui nginxui.deb

install:
	dpkg -i nginxui.deb

uninstall:
	sudo dpkg -r nginxui

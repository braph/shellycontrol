shellycontrol: shellycontrol.c
	gcc -o shellycontrol shellycontrol.c -lgpiod -lcurl

install:
	install -Dm 755 shellycontrol /bin/shellycontrol
	install -Dm 644 shellycontrol.service /etc/systemd/system/shellycontrol.service

clean:
	rm -f shellycontrol

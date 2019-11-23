; Amiga Shell
; Install device and start it (with sashimi)
; Call it with "execute amiga_install_and_start.ash" on AmigaDOS Shell...

echo "Install Device..."
copy build/build-000/ksz8851.device.000 to devs:networks/ksz8851.device

echo "open sashimi"
run >NIL: c:sashimi CONSOLE

echo "flush devs..."
c:flush -d >NIL:
sleep 2

echo "open device..."
sleep 1
c:sanautil -d ksz8851.device -u 0 STATUS
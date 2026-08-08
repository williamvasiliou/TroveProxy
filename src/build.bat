gcc.exe -Wall -Wextra -Wpedantic client.c -o client.exe -lws2_32
gcc.exe -Wall -Wextra -Wpedantic -shared dll.c -o dll.dll -lws2_32
gcc.exe -Wall -Wextra -Wpedantic -c proxy.c -o proxy.obj
gcc.exe -Wall -Wextra -Wpedantic server.c -o server.exe -lws2_32
gcc.exe -Wall -Wextra -Wpedantic -c starter.c -o starter.obj
gcc.exe -Wall -Wextra -Wpedantic -c window.c -o window.obj
gcc.exe -mwindows proxy.obj starter.obj window.obj -o window.exe -lws2_32

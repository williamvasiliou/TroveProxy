gcc.exe -Wall -Wextra -Wpedantic client.c -o client.exe -lws2_32
gcc.exe -Wall -Wextra -Wpedantic -shared dll.c -o dll.dll -lws2_32
gcc.exe -Wall -Wextra -Wpedantic proxy.c -o proxy.exe -lws2_32
gcc.exe -Wall -Wextra -Wpedantic server.c -o server.exe -lws2_32
gcc.exe -Wall -Wextra -Wpedantic starter.c -o starter.exe

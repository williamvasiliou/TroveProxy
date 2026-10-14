gcc.exe -Wall -Wextra -Wpedantic client.c -o client.exe -lws2_32
gcc.exe -Wall -Wextra -Wpedantic -shared patch_dll.c -o patch_dll.dll
gcc.exe -Wall -Wextra -Wpedantic injector.c -o injector.exe
gcc.exe -Wall -Wextra -Wpedantic proxy.c -o proxy.exe -lws2_32
gcc.exe -Wall -Wextra -Wpedantic server.c -o server.exe -lws2_32

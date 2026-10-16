# TroveProxy

## `injector`

Modify `test/injector/injector.c` to specify the `path` to `patch_dll.dll`. See above the `show_menu` function.

Run `build.bat` to build the files in `test/injector`.

To test, start by running `client.exe`, `proxy.exe`, and `server.exe`. Connect directly to the server. Send data. The data should not flow through the proxy. Then, disconnect and restart the server. Run `injector.exe`, search for `client.exe`, and inject. Connect directly and send data again. Observe the data using the proxy.

## `starter`

Modify `test/starter/starter.c` to specify the `dll_path`, `executable`, and `working_directory` to `dll.dll`, `client.exe`, and the directory with `client.exe`, respectively. See above the `main` function.

Run `build.bat` to build the files in `test/starter`.

To test, start by running `client.exe`, `proxy.exe`, and `server.exe`. Connect directly to the server. Send data. The data should not flow through the proxy. Then, disconnect and restart the server. Run `starter.exe`, connect directly, and send data again. Observe the data using the proxy.

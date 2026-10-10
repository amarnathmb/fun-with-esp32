#pragma once

/* Registers routes and starts the configuration web server. */
void startSettingsServer();
/* Processes one pending web-server client request. */
void handleSettingsServerClient();

#pragma once

/* A bit ugly, but we'll define a single function to handle injection so its easier to do cross platform */
void launch_and_inject(const char* gmod_path, const char* lje_path, int argc, char** argv);

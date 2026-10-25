// use_fast_gpu.cpp - asks laptops with two GPUs to run this program on the fast one.
//
// Windows laptops with Intel graphics plus an NVIDIA or AMD card start OpenGL
// programs on the slow built-in Intel chip unless the program says otherwise. The
// graphics drivers look for these two exported variables in the .exe; setting
// them to 1 picks the dedicated GPU. Every step is linked with this file.

extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

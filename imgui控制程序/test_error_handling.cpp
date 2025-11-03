/**
 * @file test_error_handling.cpp
 * @brief Simple test program to verify error handling improvements
 */

#include "MyImGui.h"
#include <iostream>

#ifdef RUN_TESTS

void TestDrawFunction()
{
    // Simple test UI
    ImGui::Begin("Error Handling Test");
    ImGui::Text("This is a test of the error handling improvements!");
    ImGui::Text("If you see this, the initialization was successful.");
    ImGui::End();
}

int main()
{
    std::wcout << L"=== MyImGui Error Handling Test ===" << std::endl;
    
    MyImGui gui;
    
    // Test 1: Normal initialization
    std::wcout << L"\nTest 1: Normal initialization..." << std::endl;
    if (gui.Init(nullptr, L"Error Handling Test", 800, 600, SW_SHOW, 16.0f))
    {
        std::wcout << L"? Initialization successful!" << std::endl;
        
        // Test 2: Normal run with valid draw function
        std::wcout << L"\nTest 2: Running with valid draw function..." << std::endl;
        
        // Run for a short time then exit
        int frameCount = 0;
        gui.Run([&]() {
            TestDrawFunction();
            frameCount++;
            if (frameCount > 60) // Run for about 1 second at 60fps
            {
                PostQuitMessage(0);
            }
        });
        
        std::wcout << L"? Rendering loop completed successfully!" << std::endl;
        
        gui.Shutdown();
        std::wcout << L"? Shutdown completed successfully!" << std::endl;
    }
    else
    {
        std::wcerr << L"? Initialization failed!" << std::endl;
        return 1;
    }
    
    // Test 3: Test error handling with null draw function
    std::wcout << L"\nTest 3: Testing null draw function error handling..." << std::endl;
    MyImGui gui2;
    if (gui2.Init(nullptr, L"Test Window 2", 400, 300))
    {
        gui2.Run(nullptr); // This should trigger error handling
        gui2.Shutdown();
    }
    
    std::wcout << L"\n=== All tests completed ===" << std::endl;
    std::wcout << L"Press any key to exit..." << std::endl;
    std::wcin.get();
    
    return 0;
}

#endif